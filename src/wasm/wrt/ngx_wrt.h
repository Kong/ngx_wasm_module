#ifndef _NGX_WRT_H_INCLUDED_
#define _NGX_WRT_H_INCLUDED_


#include <ngx_core.h>
#include <wasm.h>

#if NGX_WASM_HAVE_WASMTIME
#include <wasmtime.h>

typedef wasmtime_error_t  ngx_wrt_res_t;
#elif NGX_WASM_HAVE_WASMER
#include <wasmer.h>

typedef ngx_str_t  ngx_wrt_res_t;

#elif NGX_WASM_HAVE_WAMR
#include <wasm_c_api.h>

typedef ngx_str_t  ngx_wrt_res_t;
#endif /* NGX_WASM_HAVE_* */


typedef struct {
    ngx_str_t        compiler;
} ngx_wavm_conf_t;


typedef struct {
    wasm_trap_t     *trap;
    ngx_wrt_res_t   *res;
} ngx_wavm_err_t;


void ngx_wavm_log_error(ngx_uint_t level, ngx_log_t *log, ngx_wavm_err_t *e,
    const char *fmt, ...);


wasm_config_t *ngx_wrt_config_create();
ngx_int_t ngx_wrt_config_init(ngx_log_t *log, wasm_config_t *wasm_config,
    ngx_wavm_conf_t *vm_config);
ngx_int_t ngx_wrt_engine_new(wasm_config_t *config, wasm_engine_t **out,
    ngx_wavm_err_t *err);
ngx_int_t ngx_wrt_wat2wasm(wasm_byte_vec_t *wat, wasm_byte_vec_t *wasm,
    ngx_wavm_err_t *err);
ngx_int_t ngx_wrt_module_validate(wasm_store_t *s, wasm_byte_vec_t *bytes,
    ngx_wavm_err_t *err);
ngx_int_t ngx_wrt_module_new(wasm_store_t *s, wasm_byte_vec_t *bytes,
    wasm_module_t **out, ngx_wavm_err_t *err);
ngx_int_t ngx_wrt_instance_new(wasm_store_t *s, wasm_module_t *m,
    wasm_extern_vec_t *imports, wasm_instance_t **instance,
    ngx_wavm_err_t *err);
ngx_int_t ngx_wrt_func_call(wasm_func_t *f, wasm_val_vec_t *args,
    wasm_val_vec_t *rets, ngx_wavm_err_t *err);
u_char *ngx_wrt_error_log_handler(ngx_wrt_res_t *res, u_char *buf, size_t len);


#endif /* _NGX_WRT_H_INCLUDED_ */
