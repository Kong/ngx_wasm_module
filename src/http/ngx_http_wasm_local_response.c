#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_http_wasm.h>


void
ngx_http_wasm_discard_local_response(ngx_http_wasm_req_ctx_t *rctx)
{
    ngx_http_request_t  *r;

    r = rctx->r;

    rctx->local_resp_stashed = 0;
    rctx->local_resp_status = 0;
    rctx->local_resp_reason.len = 0;
    rctx->local_resp_body_len = -1;

    if (rctx->local_resp_reason.data) {
        ngx_pfree(r->pool, rctx->local_resp_reason.data);
    }

    if (rctx->local_resp_headers) {
        ngx_array_destroy(rctx->local_resp_headers);
        rctx->local_resp_headers = NULL;
    }

    if (rctx->local_resp_body) {
        ngx_free_chain(r->pool, rctx->local_resp_body);
        rctx->local_resp_body = NULL;
    }
}


ngx_int_t
ngx_http_wasm_stash_local_response(ngx_http_wasm_req_ctx_t *rctx,
    ngx_int_t status, u_char *reason, size_t reason_len, ngx_array_t *headers,
    u_char *body, size_t body_len)
{
    size_t               len;
    u_char              *p = NULL;
    ngx_buf_t           *b = NULL;
    ngx_chain_t         *cl = NULL;
    ngx_http_request_t  *r = rctx->r;

    if (r->header_sent || rctx->local_resp_over) {
        /* response already sent, content produced */
        return NGX_ABORT;
    }

    if (rctx->local_resp_stashed) {
        /* local response already stashed */
        return NGX_BUSY;
    }

    /* status */

    if (status < 100 || status > 999) {
        return NGX_DECLINED;
    }

    rctx->local_resp_status = status;

    /* reason */

    if (reason_len) {
        reason_len += 5; /* "ddd <reason>\0" */
        p = ngx_pnalloc(r->pool, reason_len);
        if (p == NULL) {
            goto fail;
        }

        ngx_snprintf(p, reason_len, "%03ui %s", status, reason);

        rctx->local_resp_reason.data = p;
        rctx->local_resp_reason.len = reason_len;
    }

    /* headers */

    rctx->local_resp_headers = headers;

    /* body */

    if (body_len) {
        len = body_len + sizeof(LF);

        b = ngx_create_temp_buf(r->pool, len);
        if (b == NULL) {
            goto fail;
        }

        b->last = ngx_copy(b->last, body, body_len);
        *b->last++ = LF;

        b->last_in_chain = 1;

        if (r == r->main) {
            b->last_buf = 1;

        } else {
            b->sync = 1;
        }

        cl = ngx_alloc_chain_link(r->connection->pool);
        if (cl == NULL) {
            goto fail;
        }

        cl->buf = b;
        cl->next = NULL;

        rctx->local_resp_body = cl;
        rctx->local_resp_body_len = len;
    }

    rctx->local_resp_stashed = 1;

    return NGX_OK;

fail:

    ngx_http_wasm_discard_local_response(rctx);

    return NGX_ERROR;
}


ngx_int_t
ngx_http_wasm_flush_local_response(ngx_http_request_t *r,
    ngx_http_wasm_req_ctx_t *rctx)
{
    size_t                    i;
    ngx_int_t                 rc;
    ngx_table_elt_t          *elt;

    if (!rctx->local_resp_stashed
        || rctx->local_resp_over        /* flush already invoked */
        || r->header_sent)
    {
        return NGX_DECLINED;
    }

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "wasm flushing local_response");

    rc = ngx_http_discard_request_body(r);
    if (rc != NGX_OK) {
        return NGX_ERROR;
    }

    r->headers_out.status = rctx->local_resp_status;

    if (r->err_status) {
        r->err_status = 0;
    }

    if (rctx->local_resp_reason.len) {
        r->headers_out.status_line.data = rctx->local_resp_reason.data;
        r->headers_out.status_line.len = rctx->local_resp_reason.len;
    }

    if (rctx->local_resp_headers) {
        for (i = 0; i < rctx->local_resp_headers->nelts; i++) {
            elt = &((ngx_table_elt_t *) rctx->local_resp_headers->elts)[i];

            rc = ngx_http_wasm_set_resp_header(r, elt->key, elt->value,
                                               NGX_HTTP_WASM_HEADERS_SET);
            if (rc != NGX_OK) {
                return NGX_ERROR;
            }
        }
    }

    if (rctx->local_resp_body_len) {
        if (ngx_http_set_content_type(r) != NGX_OK) {
            return NGX_ERROR;
        }
    }

    if (rctx->local_resp_body_len >= 0
        && ngx_http_wasm_set_resp_content_length(r, rctx->local_resp_body_len)
           != NGX_OK)
    {
        return NGX_ERROR;
    }

    rctx->local_resp_stashed = 0;

    rc = ngx_http_wasm_send_chain_link(r, rctx->local_resp_body);

    rctx->sent_last = 1;

    return rc;
}
