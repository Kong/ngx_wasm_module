#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_http_proxy_wasm.h>


#define NGX_HTTP_PROXY_WASM_EOF  1


static ngx_int_t ngx_http_proxy_wasm_on_request_headers(
    ngx_proxy_wasm_filter_ctx_t *fctx);
static void ngx_http_proxy_wasm_on_request_body_handler(
    ngx_http_request_t *r);
static ngx_int_t ngx_http_proxy_wasm_on_request_body(
    ngx_proxy_wasm_filter_ctx_t *fctx);
static ngx_int_t ngx_http_proxy_wasm_on_response_headers(
    ngx_proxy_wasm_filter_ctx_t *fctx);
static ngx_int_t ngx_http_proxy_wasm_on_response_body(
    ngx_proxy_wasm_filter_ctx_t *fctx);


ngx_int_t
ngx_http_proxy_wasm_ecode(ngx_proxy_wasm_err_e ecode)
{
    return NGX_HTTP_INTERNAL_SERVER_ERROR;
}


static ngx_inline ngx_int_t
ngx_http_proxy_wasm_next_action(ngx_proxy_wasm_filter_ctx_t *fctx, ngx_int_t rc,
    ngx_uint_t ret)
{
    ngx_proxy_wasm_ctx_t  *pwctx = fctx->parent;

    if (rc != NGX_OK) {
        return rc;
    }

    if (pwctx->action == NGX_PROXY_WASM_ACTION_DONE) {
        return rc;
    }

    switch (ret) {
    case NGX_PROXY_WASM_ACTION_PAUSE:
        dd("next action: yield");
        pwctx->action = ret;
        break;

    case NGX_PROXY_WASM_ACTION_CONTINUE:
        dd("next action: continue");
        pwctx->action = ret;
        break;

    default:
        ngx_proxy_wasm_log_error(NGX_LOG_WASM_NYI, pwctx->log, 0,
                                 "NYI - action: %l", ret);
        break;
    }

    return rc;
}


ngx_proxy_wasm_ctx_t *
ngx_http_proxy_wasm_ctx(ngx_proxy_wasm_filter_t *filter, void *data)
{
    ngx_proxy_wasm_ctx_t     *pwctx;
    ngx_http_wasm_req_ctx_t  *rctx = data;
    ngx_http_request_t       *r = rctx->r;

    pwctx = (ngx_proxy_wasm_ctx_t *) rctx->data;
    if (pwctx == NULL) {
        pwctx = ngx_pcalloc(r->pool, sizeof(ngx_proxy_wasm_ctx_t));
        if (pwctx == NULL) {
            return NULL;
        }

        pwctx->main = r == r->main;
        pwctx->pool = pwctx->main ? r->connection->pool : r->pool;
        pwctx->log = r->connection->log;
        pwctx->id = r->connection->number;
        pwctx->type = NGX_PROXY_WASM_CONTEXT_HTTP;

        /* for on_request_body retrieval */

        rctx->data = pwctx;
    }

    return pwctx;
}


/* phases */


ngx_int_t
ngx_http_proxy_wasm_resume(ngx_proxy_wasm_filter_ctx_t *fctx,
    ngx_proxy_wasm_step_e step)
{
    ngx_int_t                 rc;
    ngx_http_request_t       *r;
    ngx_http_wasm_req_ctx_t  *rctx;
    ngx_proxy_wasm_ctx_t     *pwctx;
    ngx_wavm_instance_t      *instance;

    instance = ngx_proxy_wasm_fctx2instance(fctx);
    pwctx = fctx->parent;
    rctx = ngx_http_proxy_wasm_get_rctx(instance);
    r = rctx->r;

    switch (step) {
    case NGX_PROXY_WASM_STEP_REQ_HEADERS:
        rc = ngx_http_proxy_wasm_on_request_headers(fctx);

        break;
    case NGX_PROXY_WASM_STEP_REQ_BODY_READ:
        if (pwctx->cur_filter_idx + 1 == pwctx->n_filters) {
            /* last filter */
            rc = ngx_http_wasm_read_client_request_body(r,
                     ngx_http_proxy_wasm_on_request_body_handler);

        } else {
            rc = NGX_OK;
        }

        break;
    case NGX_PROXY_WASM_STEP_REQ_BODY:
        rc = ngx_http_proxy_wasm_on_request_body(fctx);
        break;
    case NGX_PROXY_WASM_STEP_RESP_HEADERS:
        rc = ngx_http_proxy_wasm_on_response_headers(fctx);
        break;
    case NGX_PROXY_WASM_STEP_RESP_BODY:
        rc = ngx_http_proxy_wasm_on_response_body(fctx);
        break;
    default:
        ngx_wasm_assert(0);
        rc = NGX_ERROR;
        break;
    }

    if (!rctx->resp_content_chosen && rctx->local_resp_stashed) {
        rc = NGX_DONE;
    }

    ngx_wasm_assert(rc == NGX_OK
                    || rc == NGX_AGAIN
                    || rc == NGX_DONE
                    || rc == NGX_ABORT
                    || rc == NGX_ERROR);

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


static ngx_int_t
ngx_http_proxy_wasm_on_request_body(ngx_proxy_wasm_filter_ctx_t *fctx)
{
    ngx_int_t                 rc;
    ngx_proxy_wasm_filter_t  *filter;
    ngx_wavm_instance_t      *instance;
    wasm_val_vec_t           *rets;

    instance = ngx_proxy_wasm_fctx2instance(fctx);
    filter = fctx->filter;

    rc = ngx_wavm_instance_call_funcref(instance,
                                        filter->proxy_on_http_request_body,
                                        &rets, fctx->id, fctx->parent->req_body_len,
                                        NGX_HTTP_PROXY_WASM_EOF);

    return ngx_http_proxy_wasm_next_action(fctx, rc, rets->data[0].of.i32);
}


static void
ngx_http_proxy_wasm_on_request_body_handler(ngx_http_request_t *r)
{
    size_t                    len = 0;
    ngx_int_t                 rc;
    ngx_file_t                file;
    ngx_http_wasm_req_ctx_t  *rctx;
    ngx_proxy_wasm_ctx_t     *pwctx;

    dd("enter");

    if (r->request_body == NULL
        || r->request_body->bufs == NULL)
    {
        /* no body */
        dd("no body");
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
        pwctx = (ngx_proxy_wasm_ctx_t *) rctx->data;

        pwctx->cur_filter_idx = 0;
        pwctx->req_body_len = len;

        (void) ngx_proxy_wasm_ctx_resume(pwctx, pwctx->phase,
                                         NGX_PROXY_WASM_STEP_REQ_BODY);
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
