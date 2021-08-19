#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_http_proxy_wasm.h>


#define NGX_HTTP_PROXY_WASM_EOF  1


static ngx_int_t ngx_http_proxy_wasm_on_request_headers(
    ngx_proxy_wasm_filter_ctx_t *fctx);
static void ngx_http_proxy_wasm_on_request_body(ngx_http_request_t *r);
static ngx_int_t ngx_http_proxy_wasm_on_response_headers(
    ngx_proxy_wasm_filter_ctx_t *fctx);
static ngx_int_t ngx_http_proxy_wasm_on_response_body(ngx_proxy_wasm_filter_ctx_t *fctx);


static ngx_inline ngx_int_t
ngx_http_proxy_wasm_next_action(ngx_proxy_wasm_filter_ctx_t *fctx, ngx_int_t rc,
    ngx_uint_t ret)
{
    ngx_proxy_wasm_ctx_t  *prctx = fctx->filter_ctx;

    if (rc != NGX_OK) {
        return rc;
    }

    switch (ret) {
    case NGX_PROXY_WASM_ACTION_PAUSE:
    case NGX_PROXY_WASM_ACTION_CONTINUE:
        prctx->next_action = ret;
        break;

    default:
        ngx_proxy_wasm_log_error(NGX_LOG_WASM_NYI, prctx->log, 0,
                                 "NYI - next_action: %l", ret);
        break;
    }

    return rc;
}


ngx_uint_t
ngx_http_proxy_wasm_ecode(ngx_proxy_wasm_filter_ctx_t *fctx,
    ngx_wasm_phase_t *phase, ngx_uint_t ecode)
{
    if (!fctx->ecode_logged) {
        ngx_proxy_wasm_log_error(NGX_LOG_CRIT, fctx->log, fctx->ecode,
                                 "proxy_wasm could not resume "
                                 "\"%V\" execution",
                                 &fctx->filter->module->name);
        fctx->ecode_logged = 1;
    }

    if (phase->index == NGX_HTTP_LOG_PHASE) {
        ngx_proxy_wasm_on_done(fctx);
    }

    return NGX_HTTP_INTERNAL_SERVER_ERROR;
}


ngx_proxy_wasm_ctx_t *
ngx_http_proxy_wasm_get_context(ngx_proxy_wasm_t *pwm, ngx_wavm_ctx_t *wvctx)
{
    ngx_http_request_t       *r;
    ngx_http_wasm_req_ctx_t  *rctx;
    ngx_proxy_wasm_ctx_t     *prctx;

    rctx = (ngx_http_wasm_req_ctx_t *) wvctx->data;
    prctx = (ngx_proxy_wasm_ctx_t *) rctx->data;
    r = rctx->r;

    if (prctx == NULL) {
        prctx = ngx_pcalloc(r->pool, sizeof(ngx_proxy_wasm_ctx_t));
        if (prctx == NULL) {
            return NULL;
        }

        prctx->pool = r->pool;
        prctx->log = r->connection->log;
        prctx->ctxid = r->connection->number;
        prctx->current_filter = 0; // TODO: increment
        prctx->max_filters = *pwm->max_filters;

        prctx->filters = ngx_pcalloc(r->pool,
                                     sizeof(ngx_proxy_wasm_filter_ctx_t *)
                                     * prctx->max_filters);
        if (prctx->filters == NULL) {
            return NULL;
        }

        rctx->data = prctx;

        // TODO: remove & use offsetof macro
        prctx->data = rctx;
    }

    dd("filter ctxid: %ld, request ctxid: %ld",
       pwm->ctxid, prctx->ctxid);

    return prctx;
}


void
ngx_http_proxy_wasm_destroy_context(ngx_proxy_wasm_ctx_t *prctx)
{
    if (prctx->authority.data) {
        ngx_pfree(prctx->pool, prctx->authority.data);
    }

    if (prctx->scheme.data) {
        ngx_pfree(prctx->pool, prctx->scheme.data);
    }

    ngx_pfree(prctx->pool, prctx->filters);
    ngx_pfree(prctx->pool, prctx);
}


/* phases */


ngx_int_t
ngx_http_proxy_wasm_resume(ngx_proxy_wasm_filter_ctx_t *fctx,
    ngx_wasm_phase_t *phase)
{
    ngx_int_t                 rc = NGX_ERROR;
    ngx_http_request_t       *r;
    ngx_http_wasm_req_ctx_t  *rctx;
    ngx_proxy_wasm_ctx_t     *prctx;

    prctx = fctx->filter_ctx;
    rctx = ngx_http_proxy_wasm_host_get_rctx(fctx->instance);
    r = rctx->r;

    switch (phase->index) {

    case NGX_HTTP_REWRITE_PHASE:
        rc = ngx_http_proxy_wasm_on_request_headers(fctx);
        if (rc == NGX_OK) {
            /* next op */
            rc = NGX_DECLINED;
        }

        if (prctx->next_action == NGX_PROXY_WASM_ACTION_PAUSE) {
            if (r != r->main) {
                /* subrequest */
                ngx_wasm_log_error(NGX_LOG_WASM_NYI, fctx->log, 0,
                                   "NYI - proxy_wasm cannot pause "
                                   "after \"rewrite\" phase in subrequests");

                prctx->next_action = NGX_PROXY_WASM_ACTION_CONTINUE;

            } else {
                /* rewrite treats NGX_AGAIN as NGX_OK which would
                 * finalize the connection */
                ngx_log_debug0(NGX_LOG_DEBUG_WASM, r->connection->log, 0,
                               "proxy_wasm delaying pause "
                               "until \"access\" phase");
            }

            /* next phase */
            rc = NGX_DONE;
        }

        break;

    case NGX_HTTP_ACCESS_PHASE:
        if (prctx->next_action == NGX_PROXY_WASM_ACTION_PAUSE) {
            ngx_log_debug0(NGX_LOG_DEBUG_WASM, r->connection->log, 0,
                           "proxy_wasm pausing");

            rc = NGX_AGAIN;

        } else {
            /* next phase */
            rc = NGX_OK;
        }

        break;

    case NGX_HTTP_CONTENT_PHASE:

        if (prctx->next_action == NGX_PROXY_WASM_ACTION_PAUSE) {
            ngx_log_debug0(NGX_LOG_DEBUG_WASM, r->connection->log, 0,
                           "proxy_wasm pausing");

            rc = NGX_AGAIN;
            break;
        }

        rc = ngx_http_wasm_read_client_request_body(r,
                      ngx_http_proxy_wasm_on_request_body);

        if (r != r->main) {
            /* subrequest */
            rc = NGX_OK;
        }

        break;

    case NGX_HTTP_WASM_HEADER_FILTER_PHASE:
        rc = ngx_http_proxy_wasm_on_response_headers(fctx);
        if (prctx->next_action == NGX_PROXY_WASM_ACTION_PAUSE) {
            ngx_wasm_log_error(NGX_LOG_WASM_NYI, fctx->log, 0,
                               "NYI - proxy_wasm cannot pause "
                               "after \"header_filter\" phase");

            prctx->next_action = NGX_PROXY_WASM_ACTION_CONTINUE;
        }

        break;

    case NGX_HTTP_WASM_BODY_FILTER_PHASE:
        rc = ngx_http_proxy_wasm_on_response_body(fctx);
        if (prctx->next_action == NGX_PROXY_WASM_ACTION_PAUSE) {
            ngx_wasm_log_error(NGX_LOG_WASM_NYI, fctx->log, 0,
                               "NYI - proxy_wasm cannot pause "
                               "after \"body_filter\" phase");

            prctx->next_action = NGX_PROXY_WASM_ACTION_CONTINUE;
        }

        break;

    case NGX_HTTP_LOG_PHASE:
        rc = ngx_proxy_wasm_on_log(fctx);
        if (rc != NGX_OK) {
            break;
        }

        break;

    case NGX_HTTP_WASM_DONE_PHASE:
        ngx_proxy_wasm_on_done(fctx);
        rc = NGX_OK;
        break;

    default:
        /* next op */
        rc = NGX_DECLINED;
        break;

    }

    if (rc == NGX_ERROR) {
        fctx->ecode = NGX_PROXY_WASM_ERR_INSTANCE_TRAPPED;

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
ngx_http_proxy_wasm_on_request_headers(ngx_proxy_wasm_filter_ctx_t *fctx)
{
    ngx_int_t            rc;
    ngx_uint_t           nheaders;
    ngx_http_request_t  *r;
    ngx_proxy_wasm_t    *pwm;
    wasm_val_vec_t      *rets;

    pwm = fctx->filter;
    r = ngx_http_proxy_wasm_host_get_req(fctx->instance);
    nheaders = ngx_http_wasm_req_headers_count(r);

    if (pwm->abi_version == NGX_PROXY_WASM_0_1_0) {
        /* 0.1.0 */
        rc = ngx_wavm_instance_call_funcref(fctx->instance,
                                            pwm->proxy_on_http_request_headers,
                                            &rets, fctx->id, nheaders);

    } else {
        /* 0.2.0+ */
        rc = ngx_wavm_instance_call_funcref(fctx->instance,
                                            pwm->proxy_on_http_request_headers,
                                            &rets, fctx->id, nheaders,
                                            NGX_HTTP_PROXY_WASM_EOF);
    }

    return ngx_http_proxy_wasm_next_action(fctx, rc, rets->data[0].of.i32);
}


static void
ngx_http_proxy_wasm_on_request_body(ngx_http_request_t *r)
{
    size_t                        i, len = 0;
    ngx_int_t                     rc;
    ngx_file_t                    file;
    ngx_http_wasm_req_ctx_t      *rctx;
    ngx_proxy_wasm_ctx_t         *prctx;
    ngx_proxy_wasm_filter_ctx_t  *fctx;
    wasm_val_vec_t               *rets;

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
        prctx = (ngx_proxy_wasm_ctx_t *) rctx->data;

        for (i = 0; i < prctx->max_filters; i++) {
            fctx = prctx->filters[i];

            ngx_wasm_assert(fctx);

            if (fctx) {
                rc = ngx_wavm_instance_call_funcref(fctx->instance,
                         fctx->filter->proxy_on_http_request_body,
                         &rets, prctx->ctxid, len,
                         NGX_HTTP_PROXY_WASM_EOF);

                (void) ngx_http_proxy_wasm_next_action(fctx, rc,
                                                       rets->data[0].of.i32);
            }
        }
    }
}


static ngx_int_t
ngx_http_proxy_wasm_on_response_headers(ngx_proxy_wasm_filter_ctx_t *fctx)
{
    ngx_int_t                 rc;
    ngx_uint_t                nheaders;
    ngx_http_wasm_req_ctx_t  *rctx;
    ngx_http_request_t       *r;
    wasm_val_vec_t           *rets;

    rctx = ngx_http_proxy_wasm_host_get_rctx(fctx->instance);
    r = rctx->r;

    /* rebuild shim headers */
    rctx->reset_resp_shims = 1;

    nheaders = ngx_http_wasm_resp_headers_count(r);
    nheaders += ngx_http_wasm_count_shim_headers(rctx);

    if (fctx->filter->abi_version == NGX_PROXY_WASM_0_1_0) {
        /* 0.1.0 */
        rc = ngx_wavm_instance_call_funcref(fctx->instance,
                                            fctx->filter->proxy_on_http_response_headers,
                                            &rets, fctx->id, nheaders);

    } else {
        /* 0.2.0+ */
        rc = ngx_wavm_instance_call_funcref(fctx->instance,
                                            fctx->filter->proxy_on_http_response_headers,
                                            &rets, fctx->id, nheaders,
                                            NGX_HTTP_PROXY_WASM_EOF);
    }

    return ngx_http_proxy_wasm_next_action(fctx, rc, rets->data[0].of.i32);
}


static ngx_int_t
ngx_http_proxy_wasm_on_response_body(ngx_proxy_wasm_filter_ctx_t *fctx)
{
    ngx_int_t                 rc;
    ngx_http_wasm_req_ctx_t  *rctx;
    wasm_val_vec_t           *rets;

    rctx = ngx_http_proxy_wasm_host_get_rctx(fctx->instance);

    if (rctx->resp_chunk_len || rctx->resp_chunk_eof) {
        rc = ngx_wavm_instance_call_funcref(fctx->instance,
                                            fctx->filter->proxy_on_http_response_body,
                                            &rets, fctx->id, rctx->resp_chunk_len,
                                            rctx->resp_chunk_eof);

        return ngx_http_proxy_wasm_next_action(fctx, rc, rets->data[0].of.i32);
    }

    return NGX_OK;
}
