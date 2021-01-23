#ifndef _NGX_WRT_H_INCLUDED_
#define _NGX_WRT_H_INCLUDED_


#include <ngx_wasm.h>

#include <wasmtime.h>


#define NGX_WRT_NAME             "wasmtime"


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


u_char *ngx_wrt_error_log_handler(ngx_wrt_res_t *res, u_char *buf,
    size_t len);


#endif /* _NGX_WRT_H_INCLUDED__ */
