#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_http_proxy_wasm.h>


#define NGX_HTTP_PROXY_WASM_EOF  1


static ngx_int_t
ngx_http_proxy_wasm_on_request_headers(ngx_proxy_wasm_exec_t *pwexec,
    ngx_uint_t *ret)
{
    ngx_int_t                 rc;
    ngx_uint_t                nheaders;
    ngx_http_request_t       *r;
    ngx_http_wasm_req_ctx_t  *rctx;
    ngx_proxy_wasm_filter_t  *filter;
    wasm_val_vec_t           *rets;
    ngx_wavm_instance_t      *instance;

    instance = ngx_proxy_wasm_pwexec2instance(pwexec);
    filter = pwexec->filter;
    r = ngx_http_proxy_wasm_get_req(instance);
    rctx = ngx_http_proxy_wasm_get_rctx(instance);
    nheaders = ngx_http_wasm_req_headers_count(r);

    rctx->req_content_length_n = rctx->r->headers_in.content_length_n;

    if (filter->abi_version == NGX_PROXY_WASM_0_1_0) {
        /* 0.1.0 */
        rc = ngx_wavm_instance_call_funcref(instance,
                 filter->proxy_on_http_request_headers,
                 &rets, pwexec->id, nheaders);

    } else {
        /* 0.2.0+ */
        rc = ngx_wavm_instance_call_funcref(instance,
                 filter->proxy_on_http_request_headers,
                 &rets, pwexec->id, nheaders,
                 NGX_HTTP_PROXY_WASM_EOF);
    }

    if (rc == NGX_ERROR || rc == NGX_ABORT) {
        return rc;
    }

    /* rc == NGX_OK */

    *ret = rets->data[0].of.i32;

    return rc;
}


static ngx_int_t
ngx_http_proxy_wasm_on_request_body(ngx_proxy_wasm_exec_t *pwexec,
    ngx_uint_t *ret)
{
    ngx_int_t                 rc;
    ngx_proxy_wasm_filter_t  *filter;
    ngx_wavm_instance_t      *instance;
    wasm_val_vec_t           *rets;

    instance = ngx_proxy_wasm_pwexec2instance(pwexec);
    filter = pwexec->filter;

    rc = ngx_wavm_instance_call_funcref(instance,
                                        filter->proxy_on_http_request_body,
                                        &rets, pwexec->id,
                                        pwexec->parent->req_body_len,
                                        NGX_HTTP_PROXY_WASM_EOF);
    if (rc == NGX_ERROR || rc == NGX_ABORT) {
        return rc;
    }

    /* rc == NGX_OK */

    *ret = rets->data[0].of.i32;

    return rc;
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

        pwctx->exec_index = 0;
        pwctx->req_body_len = len;

        pwctx->action = NGX_PROXY_WASM_ACTION_CONTINUE;

        (void) ngx_proxy_wasm_resume(pwctx, pwctx->phase,
                                     NGX_PROXY_WASM_STEP_REQ_BODY);
    }
}


static ngx_int_t
ngx_http_proxy_wasm_on_response_headers(ngx_proxy_wasm_exec_t *pwexec,
    ngx_uint_t *ret)
{
    ngx_int_t                 rc;
    ngx_uint_t                nheaders;
    ngx_http_wasm_req_ctx_t  *rctx;
    ngx_http_request_t       *r;
    wasm_val_vec_t           *rets;
    ngx_wavm_instance_t      *instance;

    instance = ngx_proxy_wasm_pwexec2instance(pwexec);
    rctx = ngx_http_proxy_wasm_get_rctx(instance);
    r = rctx->r;

    /* rebuild shim headers */
    rctx->reset_resp_shims = 1;

    nheaders = ngx_http_wasm_resp_headers_count(r);
    nheaders += ngx_http_wasm_count_shim_headers(rctx);

    rctx->resp_content_length_n = rctx->r->headers_out.content_length_n;

    if (pwexec->filter->abi_version == NGX_PROXY_WASM_0_1_0) {
        /* 0.1.0 */
        rc = ngx_wavm_instance_call_funcref(instance,
                 pwexec->filter->proxy_on_http_response_headers,
                 &rets, pwexec->id, nheaders);

    } else {
        /* 0.2.0+ */
        rc = ngx_wavm_instance_call_funcref(instance,
                 pwexec->filter->proxy_on_http_response_headers,
                 &rets, pwexec->id, nheaders,
                 NGX_HTTP_PROXY_WASM_EOF);
    }

    if (rc == NGX_ERROR || rc == NGX_ABORT) {
        return rc;
    }

    /* rc == NGX_OK */

    *ret = rets->data[0].of.i32;

    return rc;
}


static ngx_int_t
ngx_http_proxy_wasm_on_response_body(ngx_proxy_wasm_exec_t *pwexec,
    ngx_uint_t *ret)
{
    ngx_int_t                 rc;
    ngx_http_wasm_req_ctx_t  *rctx;
    wasm_val_vec_t           *rets;
    ngx_wavm_instance_t      *instance;

    instance = ngx_proxy_wasm_pwexec2instance(pwexec);
    rctx = ngx_http_proxy_wasm_get_rctx(instance);

    if (rctx->resp_content_length_n > 0) {
        rctx->resp_chunk_eof = (rctx->resp_chunk_len
                                == rctx->resp_content_length_n);
    }

    if (rctx->resp_chunk_len || rctx->resp_chunk_eof) {

        rc = ngx_wavm_instance_call_funcref(instance,
                 pwexec->filter->proxy_on_http_response_body,
                 &rets, pwexec->id,
                 rctx->resp_chunk_len,
                 rctx->resp_chunk_eof);

        if (rc == NGX_ERROR || rc == NGX_ABORT) {
            return rc;
        }

        /* rc == NGX_OK */

        *ret = rets->data[0].of.i32;

        return rc;
    }

    return NGX_OK;
}


static ngx_int_t
ngx_http_proxy_wasm_on_response_trailers(ngx_proxy_wasm_exec_t *pwexec,
    ngx_uint_t *ret)
{
    ngx_int_t                 rc;
    ngx_uint_t                ntrailers;
    ngx_http_wasm_req_ctx_t  *rctx;
    ngx_http_request_t       *r;
    wasm_val_vec_t           *rets;
    ngx_wavm_instance_t      *instance;

    instance = ngx_proxy_wasm_pwexec2instance(pwexec);
    rctx = ngx_http_proxy_wasm_get_rctx(instance);
    r = rctx->r;

    ntrailers = ngx_http_wasm_resp_trailers_count(r);

    if (pwexec->filter->abi_version == NGX_PROXY_WASM_0_1_0) {
        /* 0.1.0 */
        rc = ngx_wavm_instance_call_funcref(instance,
                 pwexec->filter->proxy_on_http_response_trailers,
                 &rets, pwexec->id, ntrailers);

    } else {
        /* 0.2.0+ */
        rc = ngx_wavm_instance_call_funcref(instance,
                 pwexec->filter->proxy_on_http_response_trailers,
                 &rets, pwexec->id, ntrailers,
                 NGX_HTTP_PROXY_WASM_EOF);
    }

    if (rc == NGX_ERROR || rc == NGX_ABORT) {
        return rc;
    }

    /* rc == NGX_OK */

    *ret = rets->data[0].of.i32;

    return rc;
}


static ngx_int_t
ngx_http_proxy_wasm_on_dispatch_response(ngx_proxy_wasm_exec_t *pwexec)
{
    size_t                           i;
    ngx_int_t                        rc;
    ngx_uint_t                       n_headers;
    ngx_list_part_t                 *part;
    ngx_proxy_wasm_filter_t         *filter = pwexec->filter;
    ngx_http_proxy_wasm_dispatch_t  *call = pwexec->call;

    part = &call->http_reader.fake_r.upstream->headers_in.headers.part;

    for (i = 0, n_headers = 0; /* void */; i++, n_headers++) {
        if (i >= part->nelts) {
            if (part->next == NULL) {
                break;
            }

            part = part->next;
            i = 0;
        }

        /* void */
    }

    ngx_log_debug3(NGX_LOG_DEBUG_ALL, pwexec->log, 0,
                   "proxy_wasm http dispatch response received "
                   "(pwexec->id: %ld, token_id: %ld, n_headers: %ld)",
                   pwexec->id, call->id, n_headers);

    rc = ngx_wavm_instance_call_funcref(pwexec->ictx->instance,
                                        filter->proxy_on_http_call_response,
                                        NULL, filter->id, call->id,
                                        n_headers,
                                        call->http_reader.body_len, 0);

    return rc;
}


static ngx_int_t
ngx_http_proxy_wasm_ecode(ngx_proxy_wasm_err_e ecode)
{
    return NGX_HTTP_INTERNAL_SERVER_ERROR;
}


static ngx_proxy_wasm_ctx_t *
ngx_http_proxy_wasm_ctx(void *data)
{
    ngx_proxy_wasm_ctx_t      *pwctx;
    ngx_http_wasm_req_ctx_t   *rctx = data;
    ngx_http_request_t        *r = rctx->r;
    ngx_http_wasm_loc_conf_t  *loc;

    loc = ngx_http_get_module_loc_conf(r, ngx_http_wasm_module);
    rctx = ngx_http_get_module_ctx(r, ngx_http_wasm_module);

    pwctx = (ngx_proxy_wasm_ctx_t *) rctx->data;
    if (pwctx == NULL) {
        pwctx = ngx_pcalloc(r->pool, sizeof(ngx_proxy_wasm_ctx_t));
        if (pwctx == NULL) {
            return NULL;
        }

        pwctx->type = NGX_PROXY_WASM_CONTEXT_HTTP;
        pwctx->id = r->connection->number;
        pwctx->pool = pwctx->main ? r->connection->pool : r->pool;
        pwctx->log = r->connection->log;
        pwctx->main = r == r->main;
        pwctx->data = rctx;
        pwctx->req_headers_in_access = loc->pwm_req_headers_in_access;

        /* for on_request_body retrieval */
        rctx->data = pwctx;
    }

    return pwctx;
}


static ngx_int_t
ngx_http_proxy_wasm_resume(ngx_proxy_wasm_exec_t *pwexec,
    ngx_proxy_wasm_step_e step, ngx_uint_t *ret)
{
    ngx_int_t                 rc = NGX_ERROR;
    ngx_http_request_t       *r;
    ngx_http_wasm_req_ctx_t  *rctx;
    ngx_proxy_wasm_ctx_t     *pwctx;
    ngx_wavm_instance_t      *instance;

    instance = ngx_proxy_wasm_pwexec2instance(pwexec);
    pwctx = pwexec->parent;

    switch (step) {
    case NGX_PROXY_WASM_STEP_REQ_HEADERS:
        rc = ngx_http_proxy_wasm_on_request_headers(pwexec, ret);
        break;
    case NGX_PROXY_WASM_STEP_REQ_BODY_READ:
        rctx = ngx_http_proxy_wasm_get_rctx(instance);
        r = rctx->r;

        if (pwctx->exec_index + 1 == pwctx->nfilters) {

            if (rctx->req_content_length_n > 0) {
                r->headers_in.content_length_n = rctx->req_content_length_n;
            }

            /* last filter */
            rc = ngx_http_wasm_read_client_request_body(r,
                     ngx_http_proxy_wasm_on_request_body_handler);

        } else {
            rc = NGX_OK;
        }

        break;
    case NGX_PROXY_WASM_STEP_REQ_BODY:
        rc = ngx_http_proxy_wasm_on_request_body(pwexec, ret);
        break;
    case NGX_PROXY_WASM_STEP_RESP_HEADERS:
        rc = ngx_http_proxy_wasm_on_response_headers(pwexec, ret);
        break;
    case NGX_PROXY_WASM_STEP_RESP_BODY:
        rc = ngx_http_proxy_wasm_on_response_body(pwexec, ret);
        break;
    case NGX_PROXY_WASM_STEP_RESP_TRAILERS:
        rc = ngx_http_proxy_wasm_on_response_trailers(pwexec, ret);
        break;
    case NGX_PROXY_WASM_STEP_DISPATCH_RESPONSE:
        rc = ngx_http_proxy_wasm_on_dispatch_response(pwexec);
        break;
    default:
        ngx_proxy_wasm_log_error(NGX_LOG_WASM_NYI, pwexec->log, 0,
                                 "NYI - http_proxy_wasm step: %d", step);
        break;
    }

    ngx_wasm_assert(rc == NGX_OK
                    || rc == NGX_AGAIN
                    || rc == NGX_ABORT
                    || rc == NGX_ERROR);

    return rc;
}


ngx_proxy_wasm_subsystem_t  ngx_http_proxy_wasm = {
    ngx_http_proxy_wasm_ctx,
    ngx_http_proxy_wasm_resume,
    ngx_http_proxy_wasm_ecode,
};
