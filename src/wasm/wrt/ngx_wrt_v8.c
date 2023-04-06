#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_wrt.h>
#include <ngx_wavm_host.h>
#include <ngx_wasi.h>
#include <v8bridge.h>

#ifdef NGX_WASM_WAT
#include <ngx_wasm_wat.h>
#endif


static void ngx_v8_destroy_instance(ngx_wrt_instance_t *instance);


static wasm_engine_t  *v8_engine = NULL;


static wasm_config_t *
ngx_v8_init_conf(ngx_wavm_conf_t *conf, ngx_log_t *log)
{
    if (conf->compiler.len) {
        if (!ngx_str_eq(conf->compiler.data, conf->compiler.len, "auto", -1)) {
            ngx_wavm_log_error(NGX_LOG_ERR, log, NULL,
                               "invalid compiler \"%V\"",
                               &conf->compiler);
            return NULL;
        }

        ngx_wavm_log_error(NGX_LOG_INFO, log, NULL,
                           "using V8 with compiler: \"%V\"",
                           &conf->compiler);
    }

    return wasm_config_new();
}


static ngx_int_t
ngx_v8_init_engine(ngx_wrt_engine_t *engine, wasm_config_t *config,
    ngx_pool_t *pool, ngx_wrt_err_t *err)
{
    wasm_config_delete(config);

    if (engine->engine == NULL) {
        /* only the main process starts an engine;
           further invocations reuse the existing engine */
        if (!v8_engine) {
            /* V8 in multi-threaded mode does not work well with `fork()`.
               We also need to set `--no-liftoff` here, because tiering up
               is not possible under single-threaded mode and we need TurboFan
               for performance. */
            ngx_v8_set_flags("--no-liftoff --single-threaded");

            if (ngx_v8_enable_wasm_trap_handler(1)) {
                ngx_wavm_log_error(NGX_LOG_INFO, pool->log, NULL,
                                   "enabled v8 trap handler");

            } else {
                ngx_wavm_log_error(NGX_LOG_ERR, pool->log, NULL,
                                   "failed to enable v8 trap handler");
            }

            v8_engine = wasm_engine_new();
            if (v8_engine == NULL) {
                return NGX_ERROR;
            }
        }

        engine->engine = v8_engine;
    }

    engine->store = wasm_store_new(engine->engine);
    if (engine->store == NULL) {
        return NGX_ERROR;
    }

    engine->pool = pool;
    engine->exiting = 0;

    return NGX_OK;
}


static void
ngx_v8_destroy_engine(ngx_wrt_engine_t *engine)
{
    wasm_store_delete(engine->store);
}


static ngx_int_t
ngx_v8_wat2wasm(wasm_byte_vec_t *wat, wasm_byte_vec_t *wasm,
    ngx_wrt_err_t *err)
{
#ifdef NGX_WASM_WAT
    wasm_byte_vec_t  out;
#else
    static char      err_msg[] = ".wat support not compiled in";
    static size_t    err_len = sizeof(err_msg) - 1;
#endif

#ifdef NGX_WASM_WAT
    wasm_byte_vec_new(&out, 0, NULL);

    err->res = ngx_wasm_wat_to_wasm(wat, &out);

    /* V8's own API needs to create the wasm_byte_vec_t*
       because its implementation is backed by a C++ array. */
    wasm_byte_vec_copy(wasm, &out);

    /* we can free cwabt's wasm_byte_vec_t* manually
       because it conforms to the C ABI. */
    ngx_free(out.data);

#else
    err->trap = NULL;

    err->res = ngx_alloc(sizeof(wasm_byte_vec_t), ngx_cycle->log);
    if (err->res == NULL) {
        return NGX_ERROR;
    }

    err->res->data = ngx_calloc(err_len, ngx_cycle->log);
    if (err->res->data == NULL) {
        ngx_free(err->res);
        err->res = NULL;
        return NGX_ERROR;
    }

    err->res->size = err_len;
    ngx_memcpy(err->res->data, err_msg, err_len);
#endif /* NGX_WASM_WAT */

    return err->res == NULL ? NGX_OK : NGX_ERROR;
}


static ngx_int_t
ngx_v8_validate(ngx_wrt_engine_t *engine, wasm_byte_vec_t *bytes,
    ngx_wrt_err_t *err)
{
    if (!wasm_module_validate(engine->store, bytes)) {
        return NGX_ERROR;
    }

    return NGX_OK;
}


static ngx_int_t
ngx_v8_init_module(ngx_wrt_module_t *module, ngx_wrt_engine_t *engine,
    wasm_byte_vec_t *bytes, wasm_importtype_vec_t *imports,
    wasm_exporttype_vec_t *exports, ngx_wrt_err_t *err)
{
    module->module = wasm_module_new(engine->store, bytes);
    if (module->module == NULL) {
        return NGX_ERROR;
    }

    wasm_module_imports(module->module, imports);
    wasm_module_exports(module->module, exports);

    module->engine = engine;
    module->import_types = imports;
    module->export_types = exports;
    module->nimports = 0;
    module->imports = ngx_pcalloc(engine->pool,
                                  sizeof(ngx_wavm_hfunc_t *) * imports->size);
    if (module->imports == NULL) {
        return NGX_ERROR;
    }

    module->import_kinds = ngx_pcalloc(engine->pool,
                                       sizeof(ngx_wrt_import_kind_e)
                                       * imports->size);
    if (module->import_kinds == NULL) {
        return NGX_ERROR;
    }

    return NGX_OK;
}


static ngx_int_t
ngx_v8_link_module(ngx_wrt_module_t *module, ngx_array_t *hfuncs,
    ngx_wrt_err_t *err)
{
    size_t                    i, j;
    unsigned                  found, fail;
    const wasm_importtype_t  *importtype;
    const wasm_name_t        *importmodule, *importname;
    ngx_wavm_hfunc_t         *hfunc;
    ngx_str_t                 name;

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

        if (ngx_str_eq(importmodule->data, importmodule->size,
                       "wasi_snapshot_preview1", -1))
        {
            /* resolve wasi */

            name.len = importname->size;
            name.data = (u_char *) importname->data;

            hfunc = ngx_wavm_host_hfunc_create(module->engine->pool,
                                               &ngx_wasi_host, &name);

            if (hfunc) {
                dd("   -> wasi resolved: \"%.*s\"",
                   (int) importname->size, importname->data);

                module->imports[i] = hfunc;
                module->import_kinds[i] = NGX_WRT_IMPORT_WASI;

                module->nimports++;

            } else {
                ngx_wavm_log_error(NGX_LOG_ERR, module->engine->pool->log, NULL,
                                   "NYI - unhandled WASI function \"%V\"",
                                   importname);

                fail = 1;
            }

        } else if (ngx_str_eq(importmodule->data,
                              importmodule->size,
                              "env", -1))
        {

            /* resolve hfunc */

            found = 0;

            for (j = 0; j < hfuncs->nelts; j++) {
                hfunc = ((ngx_wavm_hfunc_t **) hfuncs->elts)[j];

                if (ngx_str_eq(importname->data, importname->size,
                               hfunc->def->name.data, hfunc->def->name.len))
                {
                    ngx_wasm_assert(i == hfunc->idx);

                    dd("   -> hfuncs resolved: \"%.*s\"",
                       (int) hfunc->def->name.len, hfunc->def->name.data);

                    module->imports[i] = hfunc;
                    module->import_kinds[i] = NGX_WRT_IMPORT_HFUNC;

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
                               "cannot resolve unknown module \"%V\"",
                               importmodule);

            fail = 1;
        }
    }

    if (fail) {
        return NGX_ERROR;
    }

    ngx_wasm_assert(module->nimports == module->import_types->size);

    return NGX_OK;
}


static void
ngx_v8_destroy_module(ngx_wrt_module_t *module)
{
    size_t  i;

    for (i = 0; i < module->nimports; i++) {
        if (module->import_kinds[i] == NGX_WRT_IMPORT_WASI) {
            ngx_wavm_host_hfunc_destroy(module->imports[i]);
        }
    }

    ngx_pfree(module->engine->pool, module->imports);
    wasm_module_delete(module->module);

    ngx_pfree(module->engine->pool, module->import_kinds);
}


static ngx_int_t
ngx_v8_init_store(ngx_wrt_store_t *store, ngx_wrt_engine_t *engine,
    void *data)
{
    store->store = wasm_store_new(engine->engine);
    if (store->store == NULL) {
        return NGX_ERROR;
    }

    store->data = data;

    return NGX_OK;
}


static void
ngx_v8_destroy_store(ngx_wrt_store_t *store)
{
    wasm_store_delete(store->store);
}


static ngx_int_t
ngx_v8_init_instance(ngx_wrt_instance_t *instance, ngx_wrt_store_t *store,
    ngx_wrt_module_t *module, ngx_pool_t *pool, ngx_wrt_err_t *err)
{
    size_t               i;
    ngx_uint_t           nimports = 0;
    ngx_wavm_hfunc_t    *import;
    ngx_v8_hfunc_ctx_t  *hctx = NULL;
    wasm_func_t         *func;

    instance->imports = ngx_pcalloc(pool,
                                    sizeof(wasm_extern_t *) * module->nimports);
    if (instance->imports == NULL) {
        goto error;
    }

    instance->pool = pool;
    instance->store = store;
    instance->module = module;

    instance->ctxs = ngx_pcalloc(pool,
                                 sizeof(ngx_v8_hfunc_ctx_t) * module->nimports);
    if (instance->ctxs == NULL) {
        goto error;
    }

    for (i = 0; i < module->nimports; i++) {
        import = module->imports[i];

        hctx = &instance->ctxs[i];
        hctx->instance = (ngx_wavm_instance_t *) store->data;
        hctx->hfunc = import;

        func = wasm_func_new_with_env(store->store, hctx->hfunc->functype,
                                      &ngx_wavm_hfunc_trampoline,
                                      hctx, NULL);

        instance->imports[i] = wasm_func_as_extern(func);
        nimports++;
    }

    ngx_wasm_assert(nimports == module->nimports);

    instance->instance = wasm_instance_new(store->store, module->module,
                                           (const wasm_extern_t **)
                                           instance->imports, &err->trap);
    if (instance->instance == NULL) {
        dd("wasm_instance_new failed");
        goto error;
    }

    wasm_instance_exports(instance->instance, &instance->externs);

    return NGX_OK;

error:

    ngx_v8_destroy_instance(instance);

    return NGX_ERROR;
}


static void
ngx_v8_destroy_instance(ngx_wrt_instance_t *instance)
{
    size_t          i;
    wasm_func_t    *func;
    wasm_extern_t  *ext;

    if (instance->imports) {
        for (i = 0; i < instance->module->nimports; i++) {
            ext = instance->imports[i];

            /* wasm_func_as_extern and wasm_extern_as_func
               are not idempotent functions in V8 */
            func = (wasm_func_t *) ext;

            wasm_func_delete(func);
        }

        ngx_pfree(instance->pool, instance->imports);
    }

    if (instance->ctxs) {
        ngx_pfree(instance->pool, instance->ctxs);
    }

    if (instance->instance) {
        wasm_instance_delete(instance->instance);
        wasm_extern_vec_delete(&instance->externs);
    }

}


static ngx_int_t
ngx_v8_init_extern(ngx_wrt_extern_t *ext, ngx_wrt_instance_t *instance,
    ngx_uint_t idx)
{
    ngx_wrt_module_t         *module = instance->module;
    wasm_exporttype_t        *exporttype;
    const wasm_name_t        *exportname;
    const wasm_externtype_t  *externtype;

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
ngx_v8_destroy_extern(ngx_wrt_extern_t *ext)
{
    /* NOP */
    return;
}


static ngx_int_t
ngx_v8_call(ngx_wrt_instance_t *instance, ngx_str_t *func_name,
    ngx_uint_t func_idx, wasm_val_vec_t *args, wasm_val_vec_t *rets,
    ngx_wrt_err_t *err)
{
    wasm_extern_t  *ext;
    wasm_func_t    *func;

    ext = instance->externs.data[func_idx];

    ngx_wasm_assert(wasm_extern_kind(ext) == WASM_EXTERN_FUNC);

    func = wasm_extern_as_func(ext);

    err->trap = wasm_func_call(func, args->data, rets->data);
    if (err->trap) {
        dd("trap caught");
        return NGX_ABORT;
    }

    return NGX_OK;
}


static wasm_trap_t *
ngx_v8_trap(ngx_wrt_store_t *store, wasm_byte_vec_t *msg)
{
    return wasm_trap_new(store->store, msg);
}


static u_char *
ngx_v8_log_handler(ngx_wrt_res_t *res, u_char *buf, size_t len)
{
    u_char  *p = buf;

    if (res->size) {
        p = ngx_snprintf(buf, ngx_min(len, res->size), "%V", res);

        ngx_free(res->data);
        ngx_free(res);
    }

    return p;
}


ngx_wrt_t  ngx_wrt = {
    ngx_v8_init_conf,
    ngx_v8_init_engine,
    ngx_v8_destroy_engine,
    ngx_v8_validate,
    ngx_v8_wat2wasm,
    ngx_v8_init_module,
    ngx_v8_link_module,
    ngx_v8_destroy_module,
    ngx_v8_init_store,
    ngx_v8_destroy_store,
    ngx_v8_init_instance,
    ngx_v8_destroy_instance,
    ngx_v8_call,
    ngx_v8_init_extern,
    ngx_v8_destroy_extern,
    ngx_v8_trap,
    NULL,                              /* get_ctx */
    ngx_v8_log_handler,
};
