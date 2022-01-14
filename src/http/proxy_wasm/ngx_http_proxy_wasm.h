#ifndef _NGX_HTTP_PROXY_WASM_H_INCLUDED_
#define _NGX_HTTP_PROXY_WASM_H_INCLUDED_


#include <ngx_http_wasm.h>
#include <ngx_proxy_wasm.h>


ngx_int_t ngx_http_proxy_wasm_ecode(ngx_proxy_wasm_err_e ecode);
ngx_proxy_wasm_stream_ctx_t *ngx_http_proxy_wasm_stream_ctx(
    ngx_proxy_wasm_filter_t *filter, void *data);
void ngx_http_proxy_wasm_free_context(ngx_proxy_wasm_stream_ctx_t *sctx);
ngx_int_t ngx_http_proxy_wasm_resume(ngx_proxy_wasm_filter_ctx_t *fctx,
    ngx_wasm_phase_t *phase);


static ngx_inline ngx_http_wasm_req_ctx_t *
ngx_http_proxy_wasm_get_rctx(ngx_wavm_instance_t *instance)
{
    ngx_proxy_wasm_filter_ctx_t  *fctx;
    ngx_proxy_wasm_stream_ctx_t  *sctx;
    ngx_http_wasm_req_ctx_t      *rctx;

    fctx = ngx_proxy_wasm_get_fctx(instance);
    sctx = fctx->sctx;
    rctx = (ngx_http_wasm_req_ctx_t *) sctx->data;

    return rctx;
}


static ngx_inline ngx_http_request_t *
ngx_http_proxy_wasm_get_req(ngx_wavm_instance_t *instance)
{
    ngx_http_wasm_req_ctx_t  *rctx;

    rctx = ngx_http_proxy_wasm_get_rctx(instance);

    return rctx->r;
}


#endif /* _NGX_HTTP_PROXY_WASM_H_INCLUDED_ */
