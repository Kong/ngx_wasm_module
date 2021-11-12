#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_wrt.h>
#include <ngx_wavm_host.h>


static ngx_int_t
ngx_wasmtime_init_conf(wasm_config_t *config, ngx_wavm_conf_t *conf,
    ngx_log_t *log)
{
    wasm_name_t        msg;
    wasmtime_error_t  *err = NULL;

    wasmtime_config_debug_info_set(config, false);

    if (conf->compiler.len) {
        if (ngx_strncmp(conf->compiler.data, "auto", 4) == 0) {
            err = wasmtime_config_strategy_set(config,
                           WASMTIME_STRATEGY_AUTO);

        } else if (ngx_strncmp(conf->compiler.data, "lightbeam", 9) == 0) {
            err = wasmtime_config_strategy_set(config,
                           WASMTIME_STRATEGY_LIGHTBEAM);

        } else if (ngx_strncmp(conf->compiler.data, "cranelift", 9) == 0) {
            err = wasmtime_config_strategy_set(config,
                           WASMTIME_STRATEGY_CRANELIFT);

        } else {
            ngx_wavm_log_error(NGX_LOG_ERR, log, NULL,
                               "invalid compiler \"%V\"",
                               &conf->compiler);
            return NGX_ERROR;
        }

        if (err) {
            wasmtime_error_message(err, &msg);

            ngx_wavm_log_error(NGX_LOG_ERR, log, NULL,
                               "failed setting \"%V\" compiler: %*.s",
                               &conf->compiler, msg.size, msg.data);

            wasmtime_error_delete(err);
            wasm_name_delete(&msg);

            return NGX_ERROR;
        }

        ngx_wavm_log_error(NGX_LOG_INFO, log, NULL,
                           "using wasmtime with compiler: \"%V\"",
                           &conf->compiler);
    }

    return NGX_OK;
}


static ngx_int_t
ngx_wasmtime_init_engine(ngx_wrt_engine_t *engine, wasm_config_t *config,
    ngx_wrt_err_t *err)
{
    engine->engine = wasm_engine_new_with_config(config);
    if (engine->engine == NULL) {
        return NGX_ERROR;
    }

    engine->linker = wasmtime_linker_new(engine->engine);
    if (engine->linker == NULL) {
        return NGX_ERROR;
    }

    wasmtime_linker_allow_shadowing(engine->linker, 1);

    err->res = wasmtime_linker_define_wasi(engine->linker);
    if (err->res) {
        return NGX_ERROR;
    }

    return NGX_OK;
}


static void
ngx_wasmtime_destroy_engine(ngx_wrt_engine_t *engine)
{
    wasmtime_linker_delete(engine->linker);
    wasm_engine_delete(engine->engine);
}


static ngx_int_t
ngx_wasmtime_wat2wasm(wasm_byte_vec_t *wat, wasm_byte_vec_t *wasm,
    ngx_wrt_err_t *err)
{
    err->res = wasmtime_wat2wasm(wat->data, wat->size, wasm);

    return err->res == NULL ? NGX_OK : NGX_ERROR;
}


static ngx_int_t
ngx_wasmtime_validate(ngx_wrt_engine_t *engine, wasm_byte_vec_t *bytes,
    ngx_wrt_err_t *err)
{
    err->res = wasmtime_module_validate(engine->engine, (u_char *) bytes->data,
                                        bytes->size);
    if (err->res) {
        return NGX_ERROR;
    }

    return NGX_OK;
}


static ngx_int_t
ngx_wasmtime_init_module(ngx_wrt_module_t *module, ngx_wrt_engine_t *engine,
    wasm_byte_vec_t *bytes, wasm_importtype_vec_t *imports,
    wasm_exporttype_vec_t *exports, ngx_wrt_err_t *err)
{
    wasmtime_moduletype_t  *module_type;

    err->res = wasmtime_module_new(engine->engine, (u_char *) bytes->data,
                                   bytes->size, &module->module);
    if (err->res) {
        return NGX_ERROR;
    }

    module_type = wasmtime_module_type(module->module);

    wasmtime_moduletype_imports(module_type, imports);
    wasmtime_moduletype_exports(module_type, exports);

    wasmtime_moduletype_delete(module_type);

    module->import_types = imports;
    module->export_types = exports;
    module->engine = engine;

    return NGX_OK;
}


static ngx_int_t
ngx_wasmtime_link_module(ngx_wrt_module_t *module, ngx_array_t *hfuncs,
    ngx_wrt_err_t *err)
{
    size_t                    i;
    const wasm_importtype_t  *importtype;
    const wasm_name_t        *importmodule, *importname;
    ngx_wavm_hfunc_t         *hfunc;

    for (i = 0; i < hfuncs->nelts; i++) {
        hfunc = ((ngx_wavm_hfunc_t **) hfuncs->elts)[i];

        importtype = ((wasm_importtype_t **)
                      module->import_types->data)[hfunc->idx];
        importmodule = wasm_importtype_module(importtype);
        importname = wasm_importtype_name(importtype);

        dd("  \"%.*s.%.*s\" import (%lu/%lu) ",
           (int) importmodule->size, importmodule->data,
           (int) importname->size, importname->data,
           hfunc->idx + 1, module->import_types->size);

        if (ngx_strncmp(importmodule->data, "env", 3) != 0) {
            dd("  skipping");
            continue;
        }

        dd("   -> \"%.*s\" hfunc",
           (int) hfunc->def->name.len, hfunc->def->name.data);

        err->res = wasmtime_linker_define_func(module->engine->linker,
                                               importmodule->data,
                                               importmodule->size,
                                               importname->data,
                                               importname->size,
                                               hfunc->functype,
                                               ngx_wavm_hfunc_trampoline,
                                               hfunc, NULL);
        if (err->res) {
            return NGX_ERROR;
        }
    }

    return NGX_OK;
}


static void
ngx_wasmtime_destroy_module(ngx_wrt_module_t *module)
{
    wasmtime_module_delete(module->module);
}


static ngx_int_t
ngx_wasmtime_init_store(ngx_wrt_store_t *store, ngx_wrt_engine_t *engine,
    void *data)
{
    store->store = wasmtime_store_new(engine->engine, NULL, NULL);
    if (store->store == NULL) {
        return NGX_ERROR;
    }

    store->context = wasmtime_store_context(store->store);

    wasmtime_context_set_data(store->context, data);

    return NGX_OK;
}


static void
ngx_wasmtime_destroy_store(ngx_wrt_store_t *store)
{
    wasmtime_store_delete(store->store);
}


static ngx_int_t
ngx_wasmtime_init_instance(ngx_wrt_instance_t *instance, ngx_wrt_store_t *store,
    ngx_wrt_module_t *module, ngx_wrt_err_t *err)
{
    instance->store = store;
    instance->module = module;
    instance->wasi_config = wasi_config_new();

    //wasi_config_inherit_argv(wasi_config);
    //wasi_config_inherit_env(wasi_config);
    //wasi_config_inherit_stdin(wasi_config);
    //wasi_config_inherit_stdout(wasi_config);
    //wasi_config_inherit_stderr(wasi_config);

    err->res = wasmtime_context_set_wasi(store->context,
                                         instance->wasi_config);
    if (err->res) {
        return NGX_ERROR;
    }

    err->res = wasmtime_linker_instantiate(module->engine->linker,
                                           store->context,
                                           module->module,
                                           &instance->instance,
                                           &err->trap);
    if (err->res) {
        return NGX_ERROR;
    }

    return NGX_OK;
}


static void
ngx_wasmtime_destroy_instance(ngx_wrt_instance_t *instance)
{
    /* NOP */
    return;
}


static ngx_int_t
ngx_wasmtime_init_extern(ngx_wrt_extern_t *ext, ngx_wrt_instance_t *instance,
    ngx_uint_t idx)
{
    ngx_wrt_store_t           *s = instance->store;
    ngx_wrt_module_t          *m = instance->module;
    wasm_exporttype_t         *exporttype;
    const wasm_externtype_t   *externtype;
    char                      *name;
    size_t                     name_len;

    if (!wasmtime_instance_export_nth(s->context, &instance->instance, idx,
                                      &name, &name_len, &ext->ext))
    {
        return NGX_ERROR;
    }

    ngx_wasm_assert(idx < m->export_types->size);

    ext->context = s->context;
    ext->name.len = name_len;
    ext->name.data = ngx_alloc(name_len, ngx_cycle->log);
    if (ext->name.data == NULL) {
        return NGX_ERROR;
    }

    ngx_memcpy(ext->name.data, name, name_len);

    exporttype = m->export_types->data[idx];
    externtype = wasm_exporttype_type(exporttype);

    switch (wasm_externtype_kind(externtype)) {

    case WASM_EXTERN_FUNC:
        ngx_wasm_assert(ext->ext.kind == WASMTIME_EXTERN_FUNC);
        ext->kind = NGX_WRT_EXTERN_FUNC;
        break;

    case WASM_EXTERN_MEMORY:
        ngx_wasm_assert(ext->ext.kind == WASMTIME_EXTERN_MEMORY);
        ext->kind = NGX_WRT_EXTERN_MEMORY;
        instance->memory = &ext->ext.of.memory;
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
ngx_wasmtime_destroy_extern(ngx_wrt_extern_t *ext)
{
    ngx_free(ext->name.data);
}


void
ngx_wasm_valvec2wasmtime(wasmtime_val_t *out, wasm_val_vec_t *vec)
{
    size_t   i;

    for (i = 0; i < vec->size; i++) {

        switch (vec->data[i].kind) {

        case WASM_I32:
            out[i].kind = WASMTIME_I32;
            out[i].of.i32 = vec->data[i].of.i32;
            break;

        case WASM_I64:
            out[i].kind = WASMTIME_I64;
            out[i].of.i64 = vec->data[i].of.i64;
            break;

        default:
            ngx_wasm_assert(0);
            break;

        }
    }
}


void
ngx_wasmtime_valvec2wasm(wasm_val_vec_t *out, wasmtime_val_t *vec, size_t nvals)
{
    size_t           i;
    wasmtime_val_t  *val;

    ngx_wasm_assert(out->size == nvals);

    for (i = 0; i < nvals; i++) {
        val = vec + i;

        switch (val->kind) {

        case WASMTIME_I32:
            out->data[i].kind = WASM_I32;
            out->data[i].of.i32 = vec[i].of.i32;
            break;

        case WASMTIME_I64:
            out->data[i].kind = WASM_I64;
            out->data[i].of.i64 = vec[i].of.i64;
            break;

        case WASMTIME_F32:
            out->data[i].kind = WASM_F32;
            out->data[i].of.f32 = vec[i].of.f32;
            break;

        case WASMTIME_F64:
            out->data[i].kind = WASM_F64;
            out->data[i].of.f64 = vec[i].of.f64;
            break;

        case WASMTIME_V128:
            ngx_wasm_assert(0);
            break;

        case WASMTIME_FUNCREF:
            ngx_wasm_assert(0);
            break;

        case WASMTIME_EXTERNREF:
            ngx_wasm_assert(0);
            break;

        default:
            ngx_wasm_assert(0);
            break;

        }
    }
}


static ngx_int_t
ngx_wasmtime_call(ngx_wrt_instance_t *instance, ngx_str_t *func_name,
    ngx_uint_t func_idx, wasm_val_vec_t *args, wasm_val_vec_t *rets,
    ngx_wrt_err_t *err)
{
    ngx_int_t           rc = NGX_ERROR;;
    wasmtime_extern_t   item;
    wasmtime_func_t    *func;
    wasmtime_val_t     *wargs = NULL, *wrets = NULL;

    if (!wasmtime_instance_export_get(instance->store->context,
                                      &instance->instance,
                                      (const char *) func_name->data,
                                      func_name->len, &item))
    {
        goto done;
    }

    ngx_wasm_assert(item.kind == WASMTIME_EXTERN_FUNC);

    func = &item.of.func;

    wargs = ngx_alloc(args->size * sizeof(wasmtime_val_t), ngx_cycle->log);
    if (wargs == NULL) {
        goto done;
    }

    ngx_wasm_valvec2wasmtime(wargs, args);

    wrets = ngx_calloc(rets->size * sizeof(wasmtime_val_t), ngx_cycle->log);
    if (wrets == NULL) {
        goto done;
    }

    err->res = wasmtime_func_call(instance->store->context,
                                  func,
                                  wargs, args->size,
                                  wrets, rets->size,
                                  &err->trap);
    if (err->trap) {
        goto done;
    }

    if (err->res) {
        goto done;
    }

    ngx_wasmtime_valvec2wasm(rets, wrets, rets->size);

    rc = NGX_OK;

done:

    if (wargs) {
        ngx_free(wargs);
    }

    if (wrets) {
        ngx_free(wrets);
    }

    return rc;
}


static wasm_trap_t *
ngx_wasmtime_trap(ngx_wrt_store_t *store, wasm_byte_vec_t *msg)
{
    return wasmtime_trap_new((const char *) msg->data, msg->size);
}


static void *
ngx_wasmtime_get_ctx(void *data)
{
   wasmtime_caller_t   *caller = data;
   wasmtime_context_t  *context = wasmtime_caller_context(caller);

   return wasmtime_context_get_data(context);
}


static u_char *
ngx_wasmtime_log_handler(ngx_wrt_res_t *res, u_char *buf, size_t len)
{
    u_char            *p = buf;
    wasmtime_error_t  *error = res;
    wasm_message_t     errmsg;

    wasmtime_error_message(error, &errmsg);

    p = ngx_snprintf(buf, len, "%*s", errmsg.size, errmsg.data);

    wasm_byte_vec_delete(&errmsg);
    wasmtime_error_delete(error);

    return p;
}


ngx_wrt_t  ngx_wrt = {
    ngx_wasmtime_init_conf,
    ngx_wasmtime_init_engine,
    ngx_wasmtime_destroy_engine,
    ngx_wasmtime_validate,
    ngx_wasmtime_wat2wasm,
    ngx_wasmtime_init_module,
    ngx_wasmtime_link_module,
    ngx_wasmtime_destroy_module,
    ngx_wasmtime_init_store,
    ngx_wasmtime_destroy_store,
    ngx_wasmtime_init_instance,
    ngx_wasmtime_destroy_instance,
    ngx_wasmtime_call,
    ngx_wasmtime_init_extern,
    ngx_wasmtime_destroy_extern,
    ngx_wasmtime_trap,
    ngx_wasmtime_get_ctx,
    ngx_wasmtime_log_handler,
};
