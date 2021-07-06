#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_wrt.h>


void
ngx_wrt_config_init(wasm_config_t *config)
{
    //wasmtime_config_cranelift_opt_level_set(config, WASMTIME_OPT_LEVEL_NONE);
    wasmtime_config_debug_info_set(config, false);
    wasmtime_config_strategy_set(config, WASMTIME_STRATEGY_LIGHTBEAM);
    //wasmtime_config_profiler_set(config, WASMTIME_PROFILING_STRATEGY_NONE);
    //wasmtime_config_max_instances_set(config, 10000);
}


ngx_int_t
ngx_wrt_engine_new(wasm_config_t *config, wasm_engine_t **out,
    ngx_wavm_err_t *err)
{
    *out = wasm_engine_new_with_config(config);
    if (*out == NULL) {
        return NGX_ERROR;
    }

    return NGX_OK;
}


ngx_int_t
ngx_wrt_wat2wasm(wasm_byte_vec_t *wat, wasm_byte_vec_t *wasm,
    ngx_wavm_err_t *err)
{
    err->res = wasmtime_wat2wasm(wat, wasm);

    return err->res == NULL ? NGX_OK : NGX_ERROR;
}


ngx_int_t
ngx_wrt_module_validate(wasm_store_t *s, wasm_byte_vec_t *bytes,
    ngx_wavm_err_t *err)
{
    err->res = wasmtime_module_validate(s, bytes);

    return err->res == NULL ? NGX_OK : NGX_ERROR;
}


ngx_int_t
ngx_wrt_module_new(wasm_engine_t *e, wasm_byte_vec_t *bytes,
    wasm_module_t **out, ngx_wavm_err_t *err)
{
    err->res = wasmtime_module_new(e, bytes, out);

    return err->res == NULL ? NGX_OK : NGX_ERROR;
}


ngx_int_t
ngx_wrt_instance_new(wasm_store_t *s, wasm_module_t *m,
    wasm_extern_vec_t *imports, wasm_instance_t **instance,
    ngx_wavm_err_t *err)
{
    *instance = wasm_instance_new(s, m, (const wasm_extern_vec_t *) imports,
                                  &err->trap);
    if (*instance == NULL) {
        return NGX_ERROR;
    }

    return NGX_OK;
}


ngx_int_t
ngx_wrt_func_call(wasm_func_t *f, wasm_val_vec_t *args, wasm_val_vec_t *rets,
    ngx_wavm_err_t *err)
{
    err->trap = wasm_func_call(f, args, rets);

    return err->trap == NULL ? NGX_OK : NGX_ERROR;
}


u_char *
ngx_wrt_error_log_handler(ngx_wrt_res_t *res, u_char *buf, size_t len)
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
