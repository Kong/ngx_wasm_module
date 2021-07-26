#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_wrt.h>


wasm_config_t *
ngx_wrt_config_create()
{
    wasm_config_t  *wasm_config = wasm_config_new();

    wasmtime_config_debug_info_set(wasm_config, false);
    //wasmtime_config_strategy_set(wasm_config, WASMTIME_STRATEGY_LIGHTBEAM);
    //wasmtime_config_cranelift_opt_level_set(config, WASMTIME_OPT_LEVEL_NONE);
    //wasmtime_config_profiler_set(config, WASMTIME_PROFILING_STRATEGY_NONE);
    //wasmtime_config_max_instances_set(config, 10000);

    return wasm_config;
}


ngx_int_t
ngx_wrt_config_init(ngx_log_t *log, wasm_config_t *wasm_config,
    ngx_wavm_conf_t *vm_config)
{
    wasm_name_t        msg;
    wasmtime_error_t  *err = NULL;

    if (vm_config->compiler.len) {
        if (ngx_strncmp(vm_config->compiler.data, "auto", 4) == 0) {
            err = wasmtime_config_strategy_set(wasm_config, WASMTIME_STRATEGY_AUTO);

        } else if (ngx_strncmp(vm_config->compiler.data, "lightbeam", 9) == 0) {
            err = wasmtime_config_strategy_set(wasm_config, WASMTIME_STRATEGY_LIGHTBEAM);

        } else if (ngx_strncmp(vm_config->compiler.data, "cranelift", 9) == 0) {
            err = wasmtime_config_strategy_set(wasm_config, WASMTIME_STRATEGY_CRANELIFT);

        } else {
            ngx_wavm_log_error(NGX_LOG_ERR, log, NULL,
                               "invalid compiler \"%V\"",
                               &vm_config->compiler);
            return NGX_ERROR;
        }

        if (err) {
            wasmtime_error_message(err, &msg);

            ngx_wavm_log_error(NGX_LOG_ERR, log, NULL,
                               "failed setting \"%V\" compiler: %*.s",
                               &vm_config->compiler, msg.size, msg.data);

            wasmtime_error_delete(err);
            wasm_name_delete(&msg);

            return NGX_ERROR;
        }

        ngx_wavm_log_error(NGX_LOG_INFO, log, NULL,
                           "using wasmtime with compiler: \"%V\"",
                           &vm_config->compiler);
    }

    return NGX_OK;
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
    err->res = wasmtime_wat2wasm(wat->data, wat->size, wasm);

    return err->res == NULL ? NGX_OK : NGX_ERROR;
}


ngx_int_t
ngx_wrt_module_validate(wasm_store_t *s, wasm_byte_vec_t *bytes,
    ngx_wavm_err_t *err)
{
    if (!wasm_module_validate(s, bytes)) {
        return NGX_ERROR;
    }

    return NGX_OK;
}


ngx_int_t
ngx_wrt_module_new(wasm_store_t *s, wasm_byte_vec_t *bytes,
    wasm_module_t **out, ngx_wavm_err_t *err)
{
    *out = wasm_module_new(s, bytes);
    if (*out == NULL) {
        return NGX_ERROR;
    }

    return NGX_OK;
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
