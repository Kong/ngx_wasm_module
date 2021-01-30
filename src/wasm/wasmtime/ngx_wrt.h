#ifndef _NGX_WRT_H_INCLUDED_
#define _NGX_WRT_H_INCLUDED_


#include <ngx_core.h>

#include <wasm.h>
#include <wasmtime.h>


typedef struct wasmtime_error_t  ngx_wrt_res_t;


static ngx_inline ngx_int_t
ngx_wrt_wat2wasm(wasm_byte_vec_t *wat, wasm_byte_vec_t *wasm,
    ngx_wrt_res_t **res)
{
    *res = wasmtime_wat2wasm(wat, wasm);

    return *res == NULL ? NGX_OK : NGX_ERROR;
}


static ngx_inline ngx_int_t
ngx_wrt_module_new(wasm_engine_t *e, wasm_byte_vec_t *bytes,
    wasm_module_t **out, ngx_wrt_res_t **res)
{
    *res = wasmtime_module_new(e, bytes, out);

    return *res == NULL ? NGX_OK : NGX_ERROR;
}


static ngx_inline void
ngx_wrt_config_init(wasm_config_t *config)
{
    wasmtime_config_cranelift_opt_level_set(config, WASMTIME_OPT_LEVEL_SPEED);
    wasmtime_config_debug_info_set(config, true);
    wasmtime_config_profiler_set(config, WASMTIME_PROFILING_STRATEGY_NONE);
    wasmtime_config_strategy_set(config, WASMTIME_STRATEGY_AUTO);
    //wasmtime_config_max_instances_set(config, 10000);
}


u_char *ngx_wrt_error_log_handler(ngx_wrt_res_t *res, u_char *buf,
    size_t len);


#endif /* _NGX_WRT_H_INCLUDED_ */
