#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_http_proxy_wasm.h>


static ngx_http_request_t *ngx_http_proxy_wasm_request(ngx_proxy_wasm_t *pwm);
static ngx_int_t ngx_http_proxy_wasm_on_request_headers(ngx_proxy_wasm_t *pwm);
static ngx_int_t ngx_http_proxy_wasm_on_response_headers(ngx_proxy_wasm_t *pwm);
//static void ngx_proxy_wasm_on_log_handler(ngx_event_t *ev);


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


ngx_uint_t
ngx_http_proxy_wasm_ctxid(ngx_proxy_wasm_t *pwm)
{
    ngx_uint_t                ctxid;
    ngx_http_request_t       *r;

    r = ngx_http_proxy_wasm_request(pwm);
    ctxid = r->connection->number;

    dd("pwm->rctxid: %ld, ctxid: %ld", pwm->rctxid, ctxid);

    return ctxid;
}


ngx_uint_t
ngx_http_proxy_wasm_ecode(ngx_proxy_wasm_t *pwm, ngx_uint_t ecode)
{
    return NGX_HTTP_INTERNAL_SERVER_ERROR;
}


/* phases */


ngx_int_t
ngx_http_proxy_wasm_resume(ngx_proxy_wasm_t *pwm, ngx_wasm_phase_t *phase,
    ngx_wavm_ctx_t *wvctx)
{
    ngx_int_t                 rc;
#if 0
    ngx_http_wasm_req_ctx_t  *rctx;

    rctx = ngx_http_proxy_wasm_rctx(pwm);
#endif

    if (pwm->trap) {
        return NGX_DECLINED;
    }

    switch (phase->index) {

    case NGX_HTTP_REWRITE_PHASE:
        rc = ngx_http_proxy_wasm_on_request_headers(pwm);
        if (rc == NGX_OK) {
            rc = NGX_DECLINED;
        }

        break;

    case NGX_HTTP_WASM_HEADER_FILTER_PHASE:
        rc = ngx_http_proxy_wasm_on_response_headers(pwm);
        if (rc != NGX_OK) {
            rc = NGX_DECLINED;
        }

        break;

    case NGX_HTTP_LOG_PHASE:
#if 0
        pwm->yield_ev.data = rctx;
        pwm->yield_ev.log = rctx->r->connection->log;
        pwm->yield_ev.handler = ngx_proxy_wasm_on_log_handler;

        ngx_post_event(&pwm->yield_ev, &ngx_posted_events);
        rc = NGX_OK;
#else
        rc = ngx_proxy_wasm_on_log(pwm);
        if (rc != NGX_OK) {
           rc = NGX_ERROR;
        }
#endif

        break;

    default:
        rc = NGX_DECLINED;
        break;

    }

    if (rc == NGX_ERROR) {
        pwm->trap = 1;
    }

    return rc;
}


static ngx_int_t
ngx_http_proxy_wasm_on_request_headers(ngx_proxy_wasm_t *pwm)
{
    ngx_int_t                 ctxid;
    ngx_uint_t                rc;
    ngx_http_request_t       *r;
    wasm_val_vec_t           *rets;

    r = ngx_http_proxy_wasm_request(pwm);
    ctxid = ngx_http_proxy_wasm_ctxid(pwm);

    if (ngx_wavm_instance_call_funcref(pwm->instance, pwm->proxy_on_context_create,
                                       &rets, ctxid, pwm->rctxid) != NGX_OK)
    {
        return NGX_ERROR;
    }

    if (pwm->abi_version == NGX_PROXY_WASM_0_1_0) {
        /* 0.1.0 */
        rc = ngx_wavm_instance_call_funcref(pwm->instance,
                                            pwm->proxy_on_http_request_headers,
                                            &rets, ctxid,
                                            ngx_http_wasm_req_headers_count(r));

    } else {
        /* 0.2.0+ */
        rc = ngx_wavm_instance_call_funcref(pwm->instance,
                                            pwm->proxy_on_http_request_headers,
                                            &rets, ctxid,
                                            ngx_http_wasm_req_headers_count(r),
                                            0); // TODO: end_of_stream
    }

    if (rc != NGX_OK) {
        return NGX_ERROR;
    }

    switch (rets->data[0].of.i32) {

    case NGX_PROXY_WASM_ACTION_CONTINUE:
        break;

    case NGX_PROXY_WASM_ACTION_PAUSE:
        break;

    default:
        break;

    }

    return NGX_OK;
}


static ngx_int_t
ngx_http_proxy_wasm_on_response_headers(ngx_proxy_wasm_t *pwm)
{
    ngx_int_t                 ctxid;
    ngx_uint_t                rc, nheaders;
    ngx_http_request_t       *r;
    wasm_val_vec_t           *rets;

    r = ngx_http_proxy_wasm_request(pwm);
    ctxid = ngx_http_proxy_wasm_ctxid(pwm);
    nheaders = ngx_http_wasm_resp_headers_count(r);

    if (pwm->abi_version == NGX_PROXY_WASM_0_1_0) {
        /* 0.1.0 */
        rc = ngx_wavm_instance_call_funcref(pwm->instance,
                                            pwm->proxy_on_http_response_headers,
                                            &rets, ctxid, nheaders);

    } else {
        /* 0.2.0+ */
        rc = ngx_wavm_instance_call_funcref(pwm->instance,
                                            pwm->proxy_on_http_response_headers,
                                            &rets, ctxid, nheaders,
                                            0); // TODO: end_of_stream
    }

    if (rc != NGX_OK) {
        return NGX_ERROR;
    }

    switch (rets->data[0].of.i32) {

    case NGX_PROXY_WASM_ACTION_CONTINUE:
        break;

    case NGX_PROXY_WASM_ACTION_PAUSE:
        break;

    default:
        break;

    }

    return NGX_OK;
}


#if 0
static void
ngx_proxy_wasm_on_log_handler(ngx_event_t *ev)
{
    ngx_proxy_wasm_t         *pwm = ev->data;
    ngx_http_wasm_req_ctx_t  *rctx;

    rctx = ngx_http_proxy_wasm_rctx(pwm);

    ngx_wavm_ctx_update(&pwm->wvctx, ev->log, rctx);

    (void) ngx_proxy_wasm_on_log(pwm);
}
#endif
