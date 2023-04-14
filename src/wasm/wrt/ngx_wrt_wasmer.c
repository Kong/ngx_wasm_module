#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_wrt.h>
#include <ngx_wavm_host.h>
#include <ngx_wasi.h>


#define NGX_WRT_WASMER_CONFIG_NAME  "ngx_wasm_module"


typedef enum {
    NGX_WRT_IMPORT_HFUNC,
    NGX_WRT_IMPORT_WASI,
    NGX_WRT_IMPORT_HWASI,
} ngx_wrt_import_kind_e;


struct ngx_wrt_import_s {
    ngx_wrt_import_kind_e          kind;

    union {
        ngx_wavm_hfunc_t          *hfunc;
        ngx_str_t                  wasi_str;
    } of;
};


static void ngx_wasmer_last_err(ngx_wrt_res_t **res);


static wasm_config_t *
ngx_wasmer_init_conf(ngx_wavm_conf_t *conf, ngx_log_t *log)
{
    wasm_config_t  *config = wasm_config_new();
    char           *auto_compiler;

    if (!conf->compiler.len
        || ngx_str_eq(conf->compiler.data, conf->compiler.len,
                      "auto", -1)
        )
    {
        if (wasmer_is_compiler_available(LLVM)) {
            wasm_config_set_compiler(config, LLVM);
            auto_compiler = "llvm";

        } else if (wasmer_is_compiler_available(CRANELIFT)) {
            wasm_config_set_compiler(config, CRANELIFT);
            auto_compiler = "cranelift";

        } else if (wasmer_is_compiler_available(SINGLEPASS)) {
            wasm_config_set_compiler(config, SINGLEPASS);
            auto_compiler = "singlepass";

        } else {
            ngx_wavm_log_error(NGX_LOG_ERR, log, NULL,
                               "no known compiler available");
            goto error;
        }

        if (conf->compiler.len) {
            ngx_wavm_log_error(NGX_LOG_INFO, log, NULL,
                               "using wasmer with compiler: \"auto\" "
                               "(chosen: \"%s\")",
                               auto_compiler);
        }

        return config;
    }

    if (ngx_str_eq(conf->compiler.data, conf->compiler.len,
                   "cranelift", -1))
    {
        wasm_config_set_compiler(config, CRANELIFT);

    } else if (ngx_str_eq(conf->compiler.data, conf->compiler.len,
                          "singlepass", -1))
    {
        wasm_config_set_compiler(config, SINGLEPASS);

    } else if (ngx_str_eq(conf->compiler.data, conf->compiler.len,
                          "llvm", -1))
    {
        wasm_config_set_compiler(config, LLVM);

    } else {
        ngx_wavm_log_error(NGX_LOG_ERR, log, NULL,
                           "invalid compiler \"%V\"",
                           &conf->compiler);
        goto error;
    }

    ngx_wavm_log_error(NGX_LOG_INFO, log, NULL,
                       "using wasmer with compiler: \"%V\"",
                       &conf->compiler);

    return config;

error:

    wasm_config_delete(config);

    return NULL;
}


static ngx_int_t
ngx_wasmer_init_engine(ngx_wrt_engine_t *engine, wasm_config_t *config,
    ngx_pool_t *pool, ngx_wrt_err_t *err)
{
    engine->engine = wasm_engine_new_with_config(config);
    if (engine->engine == NULL) {
        ngx_wasmer_last_err(&err->res);
        return NGX_ERROR;
    }

    engine->store = wasm_store_new(engine->engine);
    if (engine->store == NULL) {
        ngx_wasmer_last_err(&err->res);
        return NGX_ERROR;
    }

    engine->pool = pool;
    engine->wasi_config = wasi_config_new(NGX_WRT_WASMER_CONFIG_NAME);
    /* used only for name lookup at module link time */
    engine->wasi_env = wasi_env_new(engine->store, engine->wasi_config);

    return NGX_OK;
}


static void
ngx_wasmer_destroy_engine(ngx_wrt_engine_t *engine)
{
    wasi_env_delete(engine->wasi_env);
    /* wasi_config_delete(engine->wasi_config); */

    wasm_store_delete(engine->store);
    wasm_engine_delete(engine->engine);
}


static ngx_int_t
ngx_wasmer_wat2wasm(wasm_byte_vec_t *wat, wasm_byte_vec_t *wasm,
    ngx_wrt_err_t *err)
{
    wat2wasm(wat, wasm);
    if (wasm->data == NULL) {
        ngx_wasmer_last_err(&err->res);
        return NGX_ERROR;
    }

    return NGX_OK;
}


static ngx_int_t
ngx_wasmer_validate(ngx_wrt_engine_t *engine, wasm_byte_vec_t *bytes,
    ngx_wrt_err_t *err)
{
    if (!wasm_module_validate(engine->store, bytes)) {
        ngx_wasmer_last_err(&err->res);
        return NGX_ERROR;
    }

    return NGX_OK;
}


static ngx_int_t
ngx_wasmer_init_module(ngx_wrt_module_t *module, ngx_wrt_engine_t *engine,
    wasm_byte_vec_t *bytes, wasm_importtype_vec_t *imports,
    wasm_exporttype_vec_t *exports, ngx_wrt_err_t *err)
{
    module->module = wasm_module_new(engine->store, bytes);
    if (module->module == NULL) {
        ngx_wasmer_last_err(&err->res);
        return NGX_ERROR;
    }

    wasm_module_imports(module->module, imports);
    wasm_module_exports(module->module, exports);

    module->engine = engine;
    module->import_types = imports;
    module->export_types = exports;
    module->nimports = 0;
    module->imports = ngx_pcalloc(engine->pool,
                                  sizeof(ngx_wrt_import_t) * imports->size);
    if (module->imports == NULL) {
        return NGX_ERROR;
    }

    return NGX_OK;
}


static ngx_int_t
ngx_wasmer_link_module(ngx_wrt_module_t *module, ngx_array_t *hfuncs,
    ngx_wrt_err_t *err)
{
    size_t                      i, j;
    unsigned                    found, fail;
    const wasm_importtype_t    *importtype;
    const wasm_name_t          *importmodule, *importname, *wasiname;
    ngx_wavm_hfunc_t           *hfunc;
    ngx_wrt_import_t           *import;
    wasi_version_t              wasi_version;
    wasmer_named_extern_vec_t   wasi_imports;
    wasmer_named_extern_t      *wasmer_extern;
    ngx_str_t                  *wasi_version_name = NULL;
    ngx_str_t                  *wasi_str;
    ngx_str_t                   name;
    static ngx_str_t            wasi_snapshot0 =
                                    ngx_string("wasi_snapshot_preview0");
    static ngx_str_t            wasi_snapshot1 =
                                    ngx_string("wasi_snapshot_preview1");

    /* WASI */

    wasi_version = wasi_get_wasi_version(module->module);

    switch (wasi_version) {

    case SNAPSHOT0:
        wasi_version_name = &wasi_snapshot0;
        break;

    case SNAPSHOT1:
        wasi_version_name = &wasi_snapshot1;
        break;

    case INVALID_VERSION:
        goto linking;

    default:
        /* NYI */
        ngx_wasm_log_error(NGX_LOG_WASM_NYI, ngx_cycle->log, 0,
                           "NYI - unhandled WASI version: %d", wasi_version);
        return NGX_ERROR;

    }

linking:

    /* linking */

    fail = 0;

    for (i = 0; i < module->import_types->size; i++) {
        importtype = ((wasm_importtype_t **) module->import_types->data)[i];
        importmodule = wasm_importtype_module(importtype);
        importname = wasm_importtype_name(importtype);

        dd("  importing \"%.*s.%.*s\" (%lu/%lu) ",
           (int) importmodule->size, importmodule->data,
           (int) importname->size, importname->data,
           i + 1, module->import_types->size);

        if (wasi_version_name
            && ngx_str_eq(wasi_version_name->data, wasi_version_name->len,
                          importmodule->data, wasi_version_name->len))
        {
            if (!module->wasi) {
                module->wasi = 1;

                if (!wasi_get_unordered_imports(module->engine->wasi_env,
                                                module->module,
                                                &wasi_imports))
                {
                    ngx_wasmer_last_err(&err->res);
                    return NGX_ERROR;
                }
            }

            /* resolve wasi */

            found = 0;

            name.len = importname->size;
            name.data = (u_char *) importname->data;

            hfunc = ngx_wavm_host_hfunc_create(module->engine->pool,
                                               &ngx_wasi_host, &name);

            if (hfunc) {
                dd("   -> wasi resolved by host function: \"%.*s\"",
                   (int) importname->size, importname->data);

                import = &module->imports[i];
                import->kind = NGX_WRT_IMPORT_HWASI;
                import->of.hfunc = hfunc;

                module->nimports++;

                found = 1;

            } else {
                for (j = 0; j < wasi_imports.size; j++) {
                    wasmer_extern = ((wasmer_named_extern_t **)
                                     wasi_imports.data)[j];
                    wasiname = wasmer_named_extern_name(wasmer_extern);

                    if (ngx_str_eq(importname->data, importname->size,
                                   wasiname->data, wasiname->size))
                    {
                        dd("   -> wasi resolved by runtime: \"%.*s\"",
                           (int) importname->size, importname->data);

                        import = &module->imports[i];
                        import->kind = NGX_WRT_IMPORT_WASI;

                        wasi_str = &import->of.wasi_str;
                        wasi_str->data = ngx_palloc(module->engine->pool,
                                                    wasiname->size);
                        if (wasi_str->data == NULL) {
                            fail = 1;
                            break;
                        }

                        ngx_memcpy(wasi_str->data, wasiname->data,
                                   wasiname->size);
                        wasi_str->len = wasiname->size;

                        module->nimports++;

                        found = 1;

                        break;
                    }
                }
            }

            if (!found) {
                ngx_wavm_log_error(NGX_LOG_ERR, module->engine->pool->log, NULL,
                                   "unhandled WASI function \"%V\"",
                                   importname);

                fail = 1;
            }

        } else if (ngx_str_eq(importmodule->data, importmodule->size,
                              "env", -1))
        {

            /* resolve hfunc */

            found = 0;

            for (j = 0; j < hfuncs->nelts; j++) {
                hfunc = ((ngx_wavm_hfunc_t **) hfuncs->elts)[j];

                if (ngx_str_eq(importname->data,
                               importname->size,
                               hfunc->def->name.data,
                               hfunc->def->name.len))
                {
                    ngx_wasm_assert(i == hfunc->idx);

                    dd("   -> hfuncs resolved: \"%.*s\"",
                       (int) hfunc->def->name.len, hfunc->def->name.data);

                    import = &module->imports[i];
                    import->kind = NGX_WRT_IMPORT_HFUNC;
                    import->of.hfunc = hfunc;

                    module->nimports++;

                    found = 1;

                    break;
                }
            }

            if (!found) {
                ngx_wavm_log_error(NGX_LOG_ERR, module->engine->pool->log, NULL,
                                   "failed resolving \"%V\" host function",
                                   importname);
                fail = 1;
            }

        } else {
            ngx_wavm_log_error(NGX_LOG_ERR, module->engine->pool->log, NULL,
                               "cannot resolve unknown \"%V\" module",
                               importmodule);

            fail = 1;
        }
    }

    if (module->wasi) {
        wasmer_named_extern_vec_delete(&wasi_imports);
    }

    if (fail) {
        return NGX_ERROR;
    }

    ngx_wasm_assert(module->nimports == module->import_types->size);

    return NGX_OK;
}


static void
ngx_wasmer_destroy_module(ngx_wrt_module_t *module)
{
    size_t             i;
    ngx_wrt_import_t  *import;

    for (i = 0; i < module->nimports; i++) {
        import = &module->imports[i];

        if (import->kind == NGX_WRT_IMPORT_HWASI) {
            wasm_functype_delete(import->of.hfunc->functype);
        }
    }

    ngx_pfree(module->engine->pool, module->imports);
    wasm_module_delete(module->module);
    wasm_importtype_vec_delete(module->import_types);
    wasm_exporttype_vec_delete(module->export_types);
}


static ngx_int_t
ngx_wasmer_init_store(ngx_wrt_store_t *store, ngx_wrt_engine_t *engine,
    void *data)
{
    store->store = wasm_store_new(engine->engine);
    if (store->store == NULL) {
        return NGX_ERROR;
    }

    store->data = data;

    /* used as a store for the instance's WASI environment */
    store->wasi_env = wasi_env_new(store->store,
                                   wasi_config_new(NGX_WRT_WASMER_CONFIG_NAME));

    return NGX_OK;
}


static void
ngx_wasmer_destroy_store(ngx_wrt_store_t *store)
{
    wasi_env_delete(store->wasi_env);
    wasm_store_delete(store->store);
}


static ngx_int_t
ngx_wasmer_init_instance(ngx_wrt_instance_t *instance, ngx_wrt_store_t *store,
    ngx_wrt_module_t *module, ngx_pool_t *pool, ngx_wrt_err_t *err)
{
    size_t                   i, j;
    ngx_uint_t               nimports = 0;
    ngx_wrt_import_t        *import;
    ngx_wasmer_hfunc_ctx_t  *hctx, *hctxs = NULL;
    wasm_func_t             *func;
    wasmer_named_extern_t   *wasmer_extern;
    ngx_str_t               *wasi_str;
    const wasm_name_t       *wasiname;

    if (module->wasi
        && !wasi_get_unordered_imports(store->wasi_env,
                                       module->module,
                                       &instance->wasi_imports))
    {
        ngx_wasmer_last_err(&err->res);
        return NGX_ERROR;
    }

    wasm_extern_vec_new_uninitialized(&instance->env, module->nimports);

    hctxs = ngx_pcalloc(pool,
                        sizeof(ngx_wasmer_hfunc_ctx_t) * module->nimports);
    if (hctxs == NULL) {
        goto error;
    }

    for (i = 0; i < module->nimports; i++) {
        import = &module->imports[i];

        switch (import->kind) {

        case NGX_WRT_IMPORT_WASI:
            for (j = 0; j < instance->wasi_imports.size; j++) {
                wasmer_extern = ((wasmer_named_extern_t **)
                                 instance->wasi_imports.data)[j];
                wasiname = wasmer_named_extern_name(wasmer_extern);
                wasi_str = &import->of.wasi_str;

                if (ngx_str_eq(wasi_str->data, wasi_str->len,
                               wasiname->data, wasiname->size))
                {
                    instance->env.data[i] = (wasm_extern_t *)
                        wasmer_named_extern_unwrap(
                            instance->wasi_imports.data[j]);
                    nimports++;
                    break;
                }
            }

            break;

        case NGX_WRT_IMPORT_HWASI:
        case NGX_WRT_IMPORT_HFUNC:
            hctx = &hctxs[i];
            hctx->instance = (ngx_wavm_instance_t *) store->data;
            hctx->hfunc = import->of.hfunc;

            func = wasm_func_new_with_env(store->store, hctx->hfunc->functype,
                                          &ngx_wavm_hfunc_trampoline,
                                          hctx, NULL);

            instance->env.data[i] = wasm_func_as_extern(func);
            nimports++;
            break;

        default:
            /* NYI */
            ngx_wasm_assert(0);
            return NGX_ABORT;

        }
    }

    ngx_wasm_assert(nimports == module->nimports);
    ngx_wasm_assert(instance->env.size == module->nimports);

    instance->pool = pool;
    instance->store = store;
    instance->module = module;
    instance->instance = wasm_instance_new(store->store, module->module,
                                           &instance->env, &err->trap);
    if (instance->instance == NULL) {
        dd("wasm_instance_new failed");
        goto error;
    }

    wasm_instance_exports(instance->instance, &instance->externs);

    instance->ctxs = hctxs;

    return NGX_OK;

error:

    if (module->wasi) {
        wasmer_named_extern_vec_delete(&instance->wasi_imports);
    }

    if (hctxs) {
        ngx_pfree(pool, hctxs);
    }

    wasm_extern_vec_delete(&instance->env);

    ngx_wasmer_last_err(&err->res);

    return NGX_ERROR;
}


static void
ngx_wasmer_destroy_instance(ngx_wrt_instance_t *instance)
{
    size_t             i;
    ngx_wrt_import_t  *import;

    if (instance->instance) {
        wasm_instance_delete(instance->instance);
    }

    if (instance->module->wasi) {
        wasmer_named_extern_vec_delete(&instance->wasi_imports);
    }

    if (instance->ctxs) {
        /**
         * Remove from instance->env the WASI entries which were
         * already deleted as part of instance->wasi_imports
         */
        for (i = 0; i < instance->module->nimports; i++) {
            import = &instance->module->imports[i];

            if (import->kind == NGX_WRT_IMPORT_WASI) {
                instance->env.data[i] = NULL;
            }
        }

        ngx_pfree(instance->pool, instance->ctxs);

        wasm_extern_vec_delete(&instance->externs);
    }

    wasm_extern_vec_delete(&instance->env);
}


static ngx_int_t
ngx_wasmer_init_extern(ngx_wrt_extern_t *ext, ngx_wrt_instance_t *instance,
    ngx_uint_t idx)
{
    ngx_wrt_module_t          *module = instance->module;
    wasm_exporttype_t         *exporttype;
    const wasm_name_t         *exportname;
    const wasm_externtype_t   *externtype;

    ngx_wasm_assert(idx < module->export_types->size);

    exporttype = module->export_types->data[idx];
    exportname = wasm_exporttype_name(exporttype);
    externtype = wasm_exporttype_type(exporttype);

    ext->ext = instance->externs.data[idx];
    ext->instance = instance;
    ext->name = (wasm_name_t *) exportname;

    switch (wasm_externtype_kind(externtype)) {

    case WASM_EXTERN_FUNC:
        ngx_wasm_assert(wasm_extern_kind(ext->ext) == WASM_EXTERN_FUNC);
        ext->kind = NGX_WRT_EXTERN_FUNC;
        break;

    case WASM_EXTERN_MEMORY:
        ngx_wasm_assert(wasm_extern_kind(ext->ext) == WASM_EXTERN_MEMORY);
        ext->kind = NGX_WRT_EXTERN_MEMORY;
        instance->memory = wasm_extern_as_memory(ext->ext);

        if (module->wasi) {
            wasi_env_set_memory(instance->store->wasi_env, instance->memory);
        }

        break;

    case WASM_EXTERN_GLOBAL:
    case WASM_EXTERN_TABLE:
        break;

    default:
        /* NYI */
        ngx_wasm_assert(0);
        return NGX_ABORT;

    }

    return NGX_OK;
}


static void
ngx_wasmer_destroy_extern(ngx_wrt_extern_t *ext)
{
    /* NOP */
    return;
}


static ngx_int_t
ngx_wasmer_call(ngx_wrt_instance_t *instance, ngx_str_t *func_name,
    ngx_uint_t func_idx, wasm_val_vec_t *args, wasm_val_vec_t *rets,
    ngx_wrt_err_t *err)
{
    wasm_extern_t  *ext;
    wasm_func_t    *func;

    ext = instance->externs.data[func_idx];

    ngx_wasm_assert(wasm_extern_kind(ext) == WASM_EXTERN_FUNC);

    func = wasm_extern_as_func(ext);

    err->trap = wasm_func_call(func, args, rets);
    if (err->trap) {
        dd("trap caught");
        return NGX_ABORT;
    }

    if (wasmer_last_error_length()) {
        dd("err caught");
        ngx_wasmer_last_err(&err->res);
        return NGX_ERROR;
    }

    return NGX_OK;
}


static wasm_trap_t *
ngx_wasmer_trap(ngx_wrt_store_t *store, wasm_byte_vec_t *msg)
{
    return wasm_trap_new(store->store, msg);
}


static u_char *
ngx_wasmer_log_handler(ngx_wrt_res_t *res, u_char *buf, size_t len)
{
    u_char  *p = buf;

    if (res->len) {
        p = ngx_snprintf(buf, ngx_min(len, res->len), "%*s",
                         res->len - 1, /* without NULL */
                         res->data);

        ngx_free(res->data);
        ngx_free(res);
    }

    return p;
}


static void
ngx_wasmer_last_err(ngx_wrt_res_t **res)
{
    ngx_str_t   *werr = NULL;
    u_char      *err = (u_char *) "no memory";
    ngx_uint_t   errlen;
    ngx_int_t    rc;

    errlen = wasmer_last_error_length();
    if (errlen) {
        werr = ngx_alloc(sizeof(ngx_str_t), ngx_cycle->log);
        if (werr == NULL) {
            goto error;
        }

        werr->len = errlen;
        werr->data = ngx_alloc(errlen, ngx_cycle->log);
        if (werr->data == NULL) {
            goto error;
        }

        rc = wasmer_last_error_message((char *) werr->data, errlen);
        if (rc < 0) {
            err = (u_char *) "NYI: wasmer error";
            goto error;
        }

        *res = werr;
    }
#if (NGX_DEBUG)
    else {
        ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0,
                      "no wasmer error to retrieve");
    }
#endif

    return;

error:

    ngx_log_error(NGX_LOG_CRIT, ngx_cycle->log, 0, NULL,
                  "failed to retrieve last wasmer error: %s", err);

    if (werr) {
        if (werr->data) {
            ngx_free(werr->data);
        }

        ngx_free(werr);
    }
}


ngx_wrt_t  ngx_wrt = {
    ngx_wasmer_init_conf,
    ngx_wasmer_init_engine,
    ngx_wasmer_destroy_engine,
    ngx_wasmer_validate,
    ngx_wasmer_wat2wasm,
    ngx_wasmer_init_module,
    ngx_wasmer_link_module,
    ngx_wasmer_destroy_module,
    ngx_wasmer_init_store,
    ngx_wasmer_destroy_store,
    ngx_wasmer_init_instance,
    ngx_wasmer_destroy_instance,
    ngx_wasmer_call,
    ngx_wasmer_init_extern,
    ngx_wasmer_destroy_extern,
    ngx_wasmer_trap,
    NULL,                              /* get_ctx */
    ngx_wasmer_log_handler,
};
