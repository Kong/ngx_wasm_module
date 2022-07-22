#ifndef _NGX_WASM_FFI_H_INCLUDED_
#define _NGX_WASM_FFI_H_INCLUDED_


#include <ngx_wavm.h>
#include <ngx_wasm_ops.h>
#include <ngx_proxy_wasm.h>
#ifdef NGX_WASM_HTTP
#include <ngx_http_proxy_wasm.h>
#endif


ngx_wavm_t *ngx_wasm_ffi_main_vm();


#ifdef NGX_WASM_HTTP
typedef struct {
    ngx_str_t       *name;
    ngx_str_t       *config;
    ngx_uint_t       filter_id;
} ngx_wasm_ffi_filter_t;


ngx_int_t ngx_http_wasm_ffi_pwm_new(ngx_wavm_t *vm,
    ngx_wasm_ffi_filter_t *filters, size_t n_filters,
    ngx_wasm_ops_t **out);
ngx_int_t ngx_http_wasm_ffi_pwm_resume(ngx_http_request_t *r,
    ngx_wasm_ops_t *e, ngx_uint_t phase);
void ngx_http_wasm_ffi_pwm_free(void *cdata);
#endif


#endif /* _NGX_WASM_FFI_H_INCLUDED_ */
