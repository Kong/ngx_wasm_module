#ifndef _NGX_HTTP_PROXY_WASM_H_INCLUDED_
#define _NGX_HTTP_PROXY_WASM_H_INCLUDED_


#include <ngx_proxy_wasm.h>
#include <ngx_http_wasm.h>
#include <ngx_http_proxy_wasm_dispatch.h>


ngx_int_t ngx_http_proxy_wasm_ecode(ngx_proxy_wasm_err_e ecode);
ngx_proxy_wasm_ctx_t *ngx_http_proxy_wasm_ctx(
    ngx_proxy_wasm_filter_t *filter, void *data);
ngx_int_t ngx_http_proxy_wasm_resume(ngx_proxy_wasm_filter_ctx_t *fctx,
    ngx_proxy_wasm_step_e step, ngx_uint_t *ret);


static ngx_inline ngx_http_wasm_req_ctx_t *
ngx_http_proxy_wasm_get_rctx(ngx_wavm_instance_t *instance)
{
    ngx_proxy_wasm_filter_ctx_t  *fctx;
    ngx_proxy_wasm_ctx_t         *pwctx;
    ngx_http_wasm_req_ctx_t      *rctx;

    fctx = ngx_proxy_wasm_instance2fctx(instance);
    pwctx = fctx->parent;
    if (!pwctx) {
        return NULL;
    }

    rctx = (ngx_http_wasm_req_ctx_t *) pwctx->data;

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
