#ifndef DDEBUG
#define DDEBUG 1
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
    ngx_proxy_wasm_stream_ctx_t  *sctx = fctx->sctx;

    if (rc != NGX_OK) {
        return rc;
    }

    switch (ret) {
    case NGX_PROXY_WASM_ACTION_PAUSE:
        dd("next action: yield");
        sctx->next_action = ret;
        break;

    case NGX_PROXY_WASM_ACTION_CONTINUE:
        dd("next action: continue");
        sctx->next_action = ret;
        break;

    default:
        ngx_proxy_wasm_log_error(NGX_LOG_WASM_NYI, sctx->log, 0,
                                 "NYI - next_action: %l", ret);
        break;
    }

    return rc;
}


ngx_int_t
ngx_http_proxy_wasm_ecode(ngx_proxy_wasm_err_e ecode)
{
    return NGX_HTTP_INTERNAL_SERVER_ERROR;
}


ngx_proxy_wasm_stream_ctx_t *
ngx_http_proxy_wasm_stream_ctx(ngx_proxy_wasm_filter_t *filter, void *data)
{
    ngx_proxy_wasm_stream_ctx_t  *sctx;
    ngx_http_wasm_req_ctx_t      *rctx = data;
    ngx_http_request_t           *r = rctx->r;

    sctx = (ngx_proxy_wasm_stream_ctx_t *) rctx->data;
    if (sctx == NULL) {
        sctx = ngx_pcalloc(r->pool, sizeof(ngx_proxy_wasm_stream_ctx_t));
        if (sctx == NULL) {
            return NULL;
        }

        sctx->id = r->connection->number;
        sctx->main = r == r->main;
        sctx->pool = r == r->main ? r->connection->pool : r->pool;
        sctx->log = r->connection->log;
        sctx->n_filters = *filter->n_filters;
        sctx->data = data;

        ngx_array_init(&sctx->fctxs, sctx->pool, sctx->n_filters,
                       sizeof(ngx_proxy_wasm_filter_ctx_t *));

        ngx_proxy_wasm_store_init(&sctx->store, sctx->pool);

        /* for on_request_body retrieval */

        rctx->data = sctx;
    }

    return sctx;
}


/* phases */


ngx_int_t
ngx_http_proxy_wasm_resume(ngx_proxy_wasm_filter_ctx_t *fctx,
    ngx_wasm_phase_t *phase)
{
    size_t                        i;
    ngx_int_t                     rc;
    ngx_http_request_t           *r;
    ngx_http_wasm_req_ctx_t      *rctx;
    ngx_proxy_wasm_stream_ctx_t  *sctx;
    ngx_wavm_instance_t          *instance;

    instance = ngx_proxy_wasm_fctx2instance(fctx);
    sctx = fctx->sctx;
    rctx = ngx_http_proxy_wasm_get_rctx(instance);
    r = rctx->r;

    switch (phase->index) {

#define NGX_HTTP_PROXY_WASM_REQ_HEADERS_IN_REWRITE  0

    case NGX_HTTP_REWRITE_PHASE:
#if (NGX_HTTP_PROXY_WASM_REQ_HEADERS_IN_REWRITE)
        rc = ngx_http_proxy_wasm_on_request_headers(fctx);
        if (rc == NGX_OK) {
            /* next op */
            rc = NGX_DECLINED;
        }

        if (sctx->next_action == NGX_PROXY_WASM_ACTION_PAUSE) {
            if (r != r->main) {
                /* subrequest */
                ngx_wasm_log_error(NGX_LOG_WASM_NYI, fctx->log, 0,
                                   "NYI - proxy_wasm cannot yield "
                                   "after \"rewrite\" phase in subrequests");

                sctx->next_action = NGX_PROXY_WASM_ACTION_CONTINUE;

            } else {
                /* rewrite treats NGX_AGAIN as NGX_OK which would
                 * finalize the connection */
                ngx_log_debug0(NGX_LOG_DEBUG_WASM, r->connection->log, 0,
                               "proxy_wasm delaying yield "
                               "until \"access\" phase");
            }

            /* next phase */
            rc = NGX_DONE;
        }
#else
        if (r != r->main) {
            /* subrequest */
            rc = ngx_http_proxy_wasm_on_request_headers(fctx);
            if (rc == NGX_OK) {
                /* next op */
                rc = NGX_DECLINED;
            }

            /* TODO: yield */
            break;
        }
#endif
        rc = NGX_DECLINED;
        break;

    case NGX_HTTP_ACCESS_PHASE:
#if (NGX_HTTP_PROXY_WASM_REQ_HEADERS_IN_REWRITE)
        rc = NGX_DECLINED;
#else
        for (i = 0; i < sctx->fctxs.nelts; i++) {
            fctx = ((ngx_proxy_wasm_filter_ctx_t **) sctx->fctxs.elts)[i];

            rc = ngx_http_proxy_wasm_on_request_headers(fctx);
            if (rc == NGX_ERROR || rc == NGX_ABORT) {
                break;
            }
        }
#endif

        if (rc == NGX_OK && sctx->next_action == NGX_PROXY_WASM_ACTION_PAUSE) {
            ngx_log_debug0(NGX_LOG_DEBUG_WASM, r->connection->log, 0,
                           "proxy_wasm pausing during \"access\" phase");

            rc = NGX_AGAIN;
        }

        break;

    case NGX_HTTP_CONTENT_PHASE:
        rc = ngx_http_wasm_read_client_request_body(r,
                      ngx_http_proxy_wasm_on_request_body);

        if (sctx->next_action == NGX_PROXY_WASM_ACTION_PAUSE) {
            ngx_log_debug0(NGX_LOG_DEBUG_WASM, r->connection->log, 0,
                           "proxy_wasm pausing at \"content\" phase "
                           "(after body)");

            rc = NGX_AGAIN;
            break;
        }

        if (r != r->main) {
            /* subrequest */
            rc = NGX_OK;
        }

        break;

    case NGX_HTTP_WASM_HEADER_FILTER_PHASE:
        rc = ngx_http_proxy_wasm_on_response_headers(fctx);
        if (sctx->next_action == NGX_PROXY_WASM_ACTION_PAUSE) {
            ngx_wasm_log_error(NGX_LOG_WASM_NYI, fctx->log, 0,
                               "NYI - proxy_wasm cannot yield "
                               "after \"header_filter\" phase");

            sctx->next_action = NGX_PROXY_WASM_ACTION_CONTINUE;
        }

        break;

    case NGX_HTTP_WASM_BODY_FILTER_PHASE:
        rc = ngx_http_proxy_wasm_on_response_body(fctx);
        if (sctx->next_action == NGX_PROXY_WASM_ACTION_PAUSE) {
            ngx_wasm_log_error(NGX_LOG_WASM_NYI, fctx->log, 0,
                               "NYI - proxy_wasm cannot yield "
                               "after \"body_filter\" phase");

            sctx->next_action = NGX_PROXY_WASM_ACTION_CONTINUE;
        }

        break;

    case NGX_HTTP_LOG_PHASE:
        rc = ngx_proxy_wasm_on_log(fctx);
        break;

    case NGX_WASM_DONE_PHASE:
        ngx_proxy_wasm_on_done(fctx);
        rc = NGX_OK;
        break;

    default:
        /* next op */
        rc = NGX_DECLINED;
        break;

    }

    if (!rctx->resp_content_chosen && rctx->local_resp_stashed) {
        /* next phase */
        rc = NGX_DONE;
    }

    ngx_wasm_assert(rc == NGX_OK
                    || rc == NGX_DONE
                    || rc == NGX_AGAIN
                    || rc == NGX_ABORT
                    || rc == NGX_ERROR
                    || rc == NGX_DECLINED
                    || rc >= NGX_HTTP_SPECIAL_RESPONSE);

    return rc;
}


static ngx_int_t
ngx_http_proxy_wasm_on_request_headers(ngx_proxy_wasm_filter_ctx_t *fctx)
{
    ngx_int_t                 rc;
    ngx_uint_t                nheaders;
    ngx_http_request_t       *r;
    ngx_proxy_wasm_filter_t  *filter;
    wasm_val_vec_t           *rets;
    ngx_wavm_instance_t      *instance;

    instance = ngx_proxy_wasm_fctx2instance(fctx);
    filter = fctx->filter;
    r = ngx_http_proxy_wasm_get_req(instance);
    nheaders = ngx_http_wasm_req_headers_count(r);

    if (filter->abi_version == NGX_PROXY_WASM_0_1_0) {
        /* 0.1.0 */
        rc = ngx_wavm_instance_call_funcref(instance,
                                            filter->proxy_on_http_request_headers,
                                            &rets, fctx->id,
                                            nheaders);

    } else {
        /* 0.2.0+ */
        rc = ngx_wavm_instance_call_funcref(instance,
                                            filter->proxy_on_http_request_headers,
                                            &rets, fctx->id,
                                            nheaders, NGX_HTTP_PROXY_WASM_EOF);
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
    ngx_proxy_wasm_stream_ctx_t  *sctx;
    ngx_proxy_wasm_filter_ctx_t  *fctx;
    wasm_val_vec_t               *rets;
    ngx_wavm_instance_t          *instance;

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
                   "proxy_wasm client request body size: %d bytes", len);

    if (len) {
        sctx = (ngx_proxy_wasm_stream_ctx_t *) rctx->data;

        for (i = 0; i < sctx->fctxs.nelts; i++) {
            fctx = ((ngx_proxy_wasm_filter_ctx_t **) sctx->fctxs.elts)[i];
            instance = ngx_proxy_wasm_fctx2instance(fctx);

            rc = ngx_wavm_instance_call_funcref(instance,
                     fctx->filter->proxy_on_http_request_body,
                     &rets, fctx->id, len,
                     NGX_HTTP_PROXY_WASM_EOF);

            (void) ngx_http_proxy_wasm_next_action(fctx, rc,
                                                   rets->data[0].of.i32);
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
    ngx_wavm_instance_t      *instance;

    instance = ngx_proxy_wasm_fctx2instance(fctx);
    rctx = ngx_http_proxy_wasm_get_rctx(instance);
    r = rctx->r;

    /* rebuild shim headers */
    rctx->reset_resp_shims = 1;

    nheaders = ngx_http_wasm_resp_headers_count(r);
    nheaders += ngx_http_wasm_count_shim_headers(rctx);

    if (fctx->filter->abi_version == NGX_PROXY_WASM_0_1_0) {
        /* 0.1.0 */
        rc = ngx_wavm_instance_call_funcref(instance,
                                            fctx->filter->proxy_on_http_response_headers,
                                            &rets, fctx->id, nheaders);

    } else {
        /* 0.2.0+ */
        rc = ngx_wavm_instance_call_funcref(instance,
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
    ngx_wavm_instance_t      *instance;

    instance = ngx_proxy_wasm_fctx2instance(fctx);
    rctx = ngx_http_proxy_wasm_get_rctx(instance);

    if (rctx->resp_chunk_len || rctx->resp_chunk_eof) {
        rc = ngx_wavm_instance_call_funcref(instance,
                                            fctx->filter->proxy_on_http_response_body,
                                            &rets, fctx->id,
                                            rctx->resp_chunk_len,
                                            rctx->resp_chunk_eof);

        return ngx_http_proxy_wasm_next_action(fctx, rc, rets->data[0].of.i32);
    }

    return NGX_OK;
}
