#ifndef _NGX_HTTP_PROXY_WASM_H_INCLUDED_
#define _NGX_HTTP_PROXY_WASM_H_INCLUDED_


#include <ngx_http_wasm.h>
#include <ngx_proxy_wasm.h>


ngx_uint_t ngx_http_proxy_wasm_ecode(ngx_proxy_wasm_filter_ctx_t *fctx,
    ngx_wasm_phase_t *phase, ngx_uint_t ecode);
ngx_proxy_wasm_ctx_t *ngx_http_proxy_wasm_get_context(ngx_proxy_wasm_t *pwm,
    void *data);
void ngx_http_proxy_wasm_destroy_context(ngx_proxy_wasm_ctx_t *prctx);
ngx_int_t ngx_http_proxy_wasm_resume(ngx_proxy_wasm_filter_ctx_t *fctx,
    ngx_wasm_phase_t *phase);

ngx_proxy_wasm_ctx_t *ngx_proxy_wasm_host_get_prctx(ngx_wavm_instance_t *instance);
ngx_http_wasm_req_ctx_t *ngx_http_proxy_wasm_host_get_rctx(
    ngx_wavm_instance_t *instance);

ngx_http_request_t *ngx_http_proxy_wasm_host_get_req(
    ngx_wavm_instance_t *instance);

#endif /* _NGX_HTTP_PROXY_WASM_H_INCLUDED_ */
