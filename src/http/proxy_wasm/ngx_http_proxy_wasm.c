#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_http_proxy_wasm.h>


#define NGX_HTTP_PROXY_WASM_EOF  1


static ngx_http_request_t *ngx_http_proxy_wasm_request(ngx_proxy_wasm_t *pwm);
static ngx_int_t ngx_http_proxy_wasm_on_request_headers(ngx_proxy_wasm_t *pwm);
static void ngx_http_proxy_wasm_on_request_body(ngx_http_request_t *r);
static ngx_int_t ngx_http_proxy_wasm_on_response_headers(ngx_proxy_wasm_t *pwm);
static ngx_int_t ngx_http_proxy_wasm_on_response_body(ngx_proxy_wasm_t *pwm);


static ngx_inline ngx_int_t
ngx_http_proxy_wasm_next_action(ngx_proxy_wasm_t *pwm, ngx_int_t rc,
    ngx_uint_t ret)
{
    ngx_http_wasm_req_ctx_t     *rctx;
    ngx_http_proxy_wasm_rctx_t  *prctx;

    if (rc != NGX_OK) {
        return rc;
    }

    switch (ret) {
    case NGX_PROXY_WASM_ACTION_PAUSE:
    case NGX_PROXY_WASM_ACTION_CONTINUE:
        rctx = ngx_http_proxy_wasm_rctx(pwm);
        prctx = (ngx_http_proxy_wasm_rctx_t *) rctx->data;
        prctx->next_action = ret;
        break;

    default:
        ngx_proxy_wasm_log_error(NGX_LOG_WASM_NYI, pwm->log, 0,
                                 "NYI - next_action: %l", ret);
        break;
    }

    return rc;
}


ngx_uint_t
ngx_http_proxy_wasm_ctxid(ngx_proxy_wasm_t *pwm)
{
    ngx_uint_t           ctxid;
    ngx_http_request_t  *r;

    r = ngx_http_proxy_wasm_request(pwm);
    ctxid = r->connection->number;

    dd("pwm->ctxid: %ld, ctxid: %ld", pwm->ctxid, ctxid);

    return ctxid;
}


ngx_uint_t
ngx_http_proxy_wasm_ecode(ngx_proxy_wasm_t *pwm, ngx_uint_t ecode)
{
    return NGX_HTTP_INTERNAL_SERVER_ERROR;
}


ngx_int_t
ngx_http_proxy_wasm_create_context(ngx_proxy_wasm_t *pwm)
{
    ngx_uint_t                   ctxid;
    ngx_http_request_t          *r;
    ngx_http_wasm_req_ctx_t     *rctx;
    ngx_http_proxy_wasm_rctx_t  *prctx;
    wasm_val_vec_t              *rets;

    rctx = ngx_http_proxy_wasm_rctx(pwm);
    prctx = (ngx_http_proxy_wasm_rctx_t *) rctx->data;
    r = rctx->r;

    if (prctx == NULL) {
        prctx = ngx_pcalloc(r->pool, sizeof(ngx_http_proxy_wasm_rctx_t)
                                     * *pwm->max_filters);
        if (prctx == NULL) {
            return NGX_ERROR;
        }

        /* allows retrieving pwm from rctx */
        prctx->pwm = pwm;

        rctx->data = prctx;
    }

    prctx = &prctx[pwm->filter_idx];

    if (!prctx->context_created) {
        ctxid = ngx_http_proxy_wasm_ctxid(pwm);

        ngx_log_debug3(NGX_LOG_DEBUG_WASM, pwm->log, 0,
                       "proxy_wasm creating context id \"#%d\""
                       " (rctx: %p, prctx: %p)",
                       ctxid, rctx, prctx);

        if (ngx_wavm_instance_call_funcref(pwm->instance,
                                           pwm->proxy_on_context_create,
                                           &rets, ctxid, pwm->ctxid)
            != NGX_OK)
        {
            return NGX_ERROR;
        }

        prctx->context_created = 1;
    }

    return NGX_OK;
}


void
ngx_http_proxy_wasm_destroy_context(ngx_proxy_wasm_t *pwm)
{
    size_t                       i;
    ngx_uint_t                   ctxid;
    ngx_http_request_t          *r;
    ngx_http_wasm_req_ctx_t     *rctx;
    ngx_http_proxy_wasm_rctx_t  *prctx;

    rctx = ngx_http_proxy_wasm_rctx(pwm);
    ctxid = ngx_http_proxy_wasm_ctxid(pwm);
    r = rctx->r;

    ngx_log_debug1(NGX_LOG_DEBUG_WASM, pwm->log, 0,
                   "wasm destroying proxy wasm ctxid %l", ctxid);

    (void) ngx_wavm_instance_call_funcref(pwm->instance,
                                          pwm->proxy_on_context_finalize,
                                          NULL, ctxid);

    prctx = (ngx_http_proxy_wasm_rctx_t *) rctx->data;
    prctx = &prctx[pwm->filter_idx];
    prctx->context_created = 0;

    /* check if all filters are finished for this request */

    prctx = (ngx_http_proxy_wasm_rctx_t *) rctx->data;

    for (i = 0; i < *pwm->max_filters; i++) {
        if (prctx[i].context_created) {
            return;
        }
    }

    if (prctx->authority.data) {
        ngx_pfree(r->pool, prctx->authority.data);
    }

    if (prctx->scheme.data) {
        ngx_pfree(r->pool, prctx->scheme.data);
    }

    /* all filters are finished */
    ngx_pfree(rctx->r->pool, prctx);
}


/* phases */


ngx_int_t
ngx_http_proxy_wasm_resume(ngx_proxy_wasm_t *pwm, ngx_wasm_phase_t *phase,
    ngx_wavm_ctx_t *wvctx)
{
    ngx_int_t                    rc = NGX_ERROR;
    ngx_http_request_t          *r;
    ngx_http_wasm_req_ctx_t     *rctx;
    ngx_http_proxy_wasm_rctx_t  *prctx;

    rctx = ngx_http_proxy_wasm_rctx(pwm);
    prctx = (ngx_http_proxy_wasm_rctx_t *) rctx->data;
    r = rctx->r;

    switch (phase->index) {

    case NGX_HTTP_REWRITE_PHASE:
        rc = ngx_http_proxy_wasm_on_request_headers(pwm);
        if (rc == NGX_OK) {
            /* next op */
            rc = NGX_DECLINED;
        }

        if (prctx->next_action == NGX_PROXY_WASM_ACTION_PAUSE) {
            if (r != r->main) {
                /* subrequest */
                ngx_wasm_log_error(NGX_LOG_WASM_NYI, pwm->log, 0,
                                   "NYI - proxy_wasm cannot pause "
                                   "after \"rewrite\" phase in subrequests");

                prctx->next_action = NGX_PROXY_WASM_ACTION_CONTINUE;

            } else {
                /* rewrite treats NGX_AGAIN as NGX_OK which would
                 * finalize the connection */
                ngx_log_debug0(NGX_LOG_DEBUG_WASM, pwm->log, 0,
                               "proxy_wasm delaying pause until \"access\" phase");
            }

            /* next phase */
            rc = NGX_DONE;
        }

        break;

    case NGX_HTTP_ACCESS_PHASE:
        if (prctx->next_action == NGX_PROXY_WASM_ACTION_PAUSE) {
            ngx_log_debug0(NGX_LOG_DEBUG_WASM, pwm->log, 0,
                           "proxy_wasm pausing");
            rc = NGX_AGAIN;

        } else {
            /* next phase */
            rc = NGX_OK;
        }

        break;

    case NGX_HTTP_CONTENT_PHASE:
        rc = ngx_http_wasm_read_client_request_body(r,
                      ngx_http_proxy_wasm_on_request_body);

        if (prctx->next_action == NGX_PROXY_WASM_ACTION_PAUSE) {
            ngx_log_debug0(NGX_LOG_DEBUG_WASM, pwm->log, 0,
                           "proxy_wasm pausing");
            rc = NGX_AGAIN;

        } else if (r != r->main) {
            /* subrequest */
            rc = NGX_OK;
        }

        break;

    case NGX_HTTP_WASM_HEADER_FILTER_PHASE:
        rc = ngx_http_proxy_wasm_on_response_headers(pwm);
        if (prctx->next_action == NGX_PROXY_WASM_ACTION_PAUSE) {
            ngx_wasm_log_error(NGX_LOG_WASM_NYI, pwm->log, 0,
                               "NYI - proxy_wasm cannot pause "
                               "after \"header_filter\" phase");

            prctx->next_action = NGX_PROXY_WASM_ACTION_CONTINUE;
        }

        break;

    case NGX_HTTP_WASM_BODY_FILTER_PHASE:
        rc = ngx_http_proxy_wasm_on_response_body(pwm);
        if (prctx->next_action == NGX_PROXY_WASM_ACTION_PAUSE) {
            ngx_wasm_log_error(NGX_LOG_WASM_NYI, pwm->log, 0,
                               "NYI - proxy_wasm cannot pause "
                               "after \"body_filter\" phase");

            prctx->next_action = NGX_PROXY_WASM_ACTION_CONTINUE;
        }

        break;

    case NGX_HTTP_LOG_PHASE:
        rc = ngx_proxy_wasm_on_log(pwm);
        if (rc != NGX_OK) {
            break;
        }

        /* fallthrough */

    case NGX_HTTP_WASM_DONE_PHASE:
        rc = ngx_proxy_wasm_on_done(pwm);
        break;

    default:
        /* next op */
        rc = NGX_DECLINED;
        break;

    }

    if (rc == NGX_ERROR) {
        pwm->ecode = NGX_PROXY_WASM_ERR_INSTANCE_TRAPPED;

    } else if (!rctx->resp_content_chosen && rctx->local_resp_stashed) {
        /* next phase */
        rc = NGX_DONE;
    }

    ngx_wasm_assert(rc == NGX_OK
                    || rc == NGX_DONE
                    || rc == NGX_AGAIN
                    || rc == NGX_ERROR
                    || rc == NGX_DECLINED
                    || rc >= NGX_HTTP_SPECIAL_RESPONSE);

    return rc;
}


static ngx_int_t
ngx_http_proxy_wasm_on_request_headers(ngx_proxy_wasm_t *pwm)
{
    ngx_int_t            rc;
    ngx_uint_t           ctxid, nheaders;
    ngx_http_request_t  *r;
    wasm_val_vec_t      *rets;

    r = ngx_http_proxy_wasm_request(pwm);
    ctxid = ngx_http_proxy_wasm_ctxid(pwm);
    nheaders = ngx_http_wasm_req_headers_count(r);

    if (pwm->abi_version == NGX_PROXY_WASM_0_1_0) {
        /* 0.1.0 */
        rc = ngx_wavm_instance_call_funcref(pwm->instance,
                                            pwm->proxy_on_http_request_headers,
                                            &rets, ctxid, nheaders);

    } else {
        /* 0.2.0+ */
        rc = ngx_wavm_instance_call_funcref(pwm->instance,
                                            pwm->proxy_on_http_request_headers,
                                            &rets, ctxid, nheaders,
                                            NGX_HTTP_PROXY_WASM_EOF);
    }

    return ngx_http_proxy_wasm_next_action(pwm, rc, rets->data[0].of.i32);
}


static void
ngx_http_proxy_wasm_on_request_body(ngx_http_request_t *r)
{
    size_t                       len = 0;
    ngx_int_t                    rc;
    ngx_uint_t                   ctxid;
    ngx_file_t                   file;
    ngx_http_wasm_req_ctx_t     *rctx;
    ngx_http_proxy_wasm_rctx_t  *prctx;
    ngx_proxy_wasm_t            *pwm;
    wasm_val_vec_t              *rets;

    if (r->request_body == NULL
        || r->request_body->bufs == NULL)
    {
        /* no body */
        return;
    }

    rc = ngx_http_wasm_rctx(r, &rctx);
    if (rc != NGX_OK) {
        ngx_wasm_log_error(NGX_LOG_ERR, r->connection->log, 0,
                           "failed to retrieve request context "
                           "after reading client body");
        return;
    }

    if (r->request_body->temp_file) {
        file = r->request_body->temp_file->file;

        if (file.fd != NGX_INVALID_FILE
            && ngx_fd_info(file.fd, &file.info) == NGX_FILE_ERROR)
        {
            ngx_log_error(NGX_LOG_CRIT, r->connection->log, ngx_errno,
                          ngx_fd_info_n " \"%V\" failed", file.name);

            if (ngx_close_file(file.fd) == NGX_FILE_ERROR) {
                ngx_log_error(NGX_LOG_ALERT, r->connection->log, ngx_errno,
                              ngx_close_file_n " \"%V\" failed", file.name);
            }

            file.fd = NGX_INVALID_FILE;
            return;
        }

        len = ngx_file_size(&file.info);

    } else {
        len = ngx_wasm_chain_len(r->request_body->bufs, NULL);
    }

    ngx_log_debug1(NGX_LOG_DEBUG_WASM, r->connection->log, 0,
                   "proxy wasm client request body size: %d bytes", len);

    if (len) {
        prctx = (ngx_http_proxy_wasm_rctx_t *) rctx->data;
        pwm = prctx->pwm;
        ctxid = ngx_http_proxy_wasm_ctxid(pwm);

        rc = ngx_wavm_instance_call_funcref(pwm->instance,
                                            pwm->proxy_on_http_request_body,
                                            &rets, ctxid, len,
                                            NGX_HTTP_PROXY_WASM_EOF);

        (void) ngx_http_proxy_wasm_next_action(pwm, rc,
                                               rets->data[0].of.i32);
    }
}


static ngx_int_t
ngx_http_proxy_wasm_on_response_headers(ngx_proxy_wasm_t *pwm)
{
    ngx_int_t                 rc;
    ngx_uint_t                ctxid, nheaders;
    ngx_http_wasm_req_ctx_t  *rctx = ngx_http_proxy_wasm_rctx(pwm);
    ngx_http_request_t       *r = rctx->r;
    wasm_val_vec_t           *rets;

    /* rebuild shim headers */
    rctx->reset_resp_shims = 1;

    ctxid = ngx_http_proxy_wasm_ctxid(pwm);
    nheaders = ngx_http_wasm_resp_headers_count(r);
    nheaders += ngx_http_wasm_count_shim_headers(rctx);

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
                                            NGX_HTTP_PROXY_WASM_EOF);
    }

    return ngx_http_proxy_wasm_next_action(pwm, rc, rets->data[0].of.i32);
}


static ngx_int_t
ngx_http_proxy_wasm_on_response_body(ngx_proxy_wasm_t *pwm)
{
    ngx_int_t                 rc = NGX_OK;
    ngx_uint_t                ctxid = ngx_http_proxy_wasm_ctxid(pwm);
    ngx_http_wasm_req_ctx_t  *rctx = ngx_http_proxy_wasm_rctx(pwm);
    wasm_val_vec_t           *rets;

    if (rctx->resp_chunk_len || rctx->resp_chunk_eof) {
        rc = ngx_wavm_instance_call_funcref(pwm->instance,
                                            pwm->proxy_on_http_response_body,
                                            &rets, ctxid, rctx->resp_chunk_len,
                                            rctx->resp_chunk_eof);

        return ngx_http_proxy_wasm_next_action(pwm, rc, rets->data[0].of.i32);
    }

    return NGX_OK;
}
