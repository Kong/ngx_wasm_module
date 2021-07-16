#ifndef _NGX_HTTP_PROXY_WASM_H_INCLUDED_
#define _NGX_HTTP_PROXY_WASM_H_INCLUDED_


#include <ngx_http_wasm.h>
#include <ngx_proxy_wasm.h>


typedef struct {
    ngx_proxy_wasm_t           *pwm;
    ngx_proxy_wasm_action_t     next_action;

    /* cache */

    ngx_str_t                  *authority;

    /* flags */

    unsigned                    context_created:1;
} ngx_http_proxy_wasm_rctx_t;


ngx_uint_t ngx_http_proxy_wasm_ctxid(ngx_proxy_wasm_t *pwm);
ngx_uint_t ngx_http_proxy_wasm_ecode(ngx_proxy_wasm_t *pwm, ngx_uint_t ecode);
ngx_int_t ngx_http_proxy_wasm_create_context(ngx_proxy_wasm_t *pwm);
void ngx_http_proxy_wasm_destroy_context(ngx_proxy_wasm_t *pwm);

ngx_int_t ngx_http_proxy_wasm_resume(ngx_proxy_wasm_t *pwm,
    ngx_wasm_phase_t *phase, ngx_wavm_ctx_t *wvctx);


static ngx_inline ngx_http_wasm_req_ctx_t *
ngx_http_proxy_wasm_rctx(ngx_proxy_wasm_t *pwm)
{
    ngx_wavm_ctx_t           *wvctx;
    ngx_http_wasm_req_ctx_t  *rctx;

    wvctx = pwm->instance->ctx;

    ngx_wasm_assert(wvctx == &pwm->wvctx);

    rctx = (ngx_http_wasm_req_ctx_t *) wvctx->data;

    ngx_wasm_assert(wvctx->log == rctx->r->connection->log);

    return rctx;
}


static ngx_inline ngx_http_request_t *
ngx_http_proxy_wasm_request(ngx_proxy_wasm_t *pwm)
{
    ngx_http_wasm_req_ctx_t  *rctx;

    rctx = ngx_http_proxy_wasm_rctx(pwm);

    return rctx->r;
}


static ngx_inline ngx_http_proxy_wasm_rctx_t *
ngx_http_proxy_wasm_prctx(ngx_proxy_wasm_t *pwm)
{
    ngx_http_wasm_req_ctx_t     *rctx;
    ngx_http_proxy_wasm_rctx_t  *prctx;

    rctx = ngx_http_proxy_wasm_rctx(pwm);
    prctx = (ngx_http_proxy_wasm_rctx_t *) rctx->data;

    ngx_wasm_assert(prctx);

    return &prctx[pwm->filter_idx];
}


#endif /* _NGX_HTTP_PROXY_WASM_H_INCLUDED_ */
