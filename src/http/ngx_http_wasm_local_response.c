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

    rctx->resp_content_chosen = 0;
    rctx->local_resp_status = 0;
    rctx->local_resp_reason.len = 0;
    rctx->local_resp_body_len = -1;

    if (rctx->local_resp_reason.data) {
        ngx_pfree(r->pool, rctx->local_resp_reason.data);
    }

    if (rctx->local_resp_headers.elts) {
        ngx_array_destroy(&rctx->local_resp_headers);
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
    size_t               len, i;
    u_char              *p, *buf = NULL;
    ngx_buf_t           *b = NULL;
    ngx_chain_t         *cl = NULL;
    ngx_table_elt_t     *elt, *elts, *eltp;
    ngx_http_request_t  *r = rctx->r;

    dd("enter");

    if (rctx->entered_header_filter) {
        return NGX_ABORT;
    }

    if (rctx->local_resp_status) {
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
        reason_len += 5;  /* "ddd <reason>\0" */
        p = ngx_pnalloc(r->pool, reason_len);
        if (p == NULL) {
            goto fail;
        }

        buf = p;

        ngx_snprintf(buf, 4, "%03ui ", status);
        buf += 4;
        buf = ngx_cpymem(buf, reason, (int) reason_len - 5);
        *buf++ = '\0';

        rctx->local_resp_reason.data = p;
        rctx->local_resp_reason.len = reason_len - 1;
    }

    /* headers */

    if (ngx_array_init(&rctx->local_resp_headers,
                       rctx->pool, headers ? headers->nelts : 0,
                       sizeof(ngx_table_elt_t)) != NGX_OK)
    {
        goto fail;
    }

    if (headers)  {

        elts = headers->elts;

        for (i = 0; i < headers->nelts; i++) {
            elt = &elts[i];

            eltp = ngx_array_push(&rctx->local_resp_headers);
            if (eltp == NULL) {
                goto fail;
            }

            ngx_memzero(eltp, sizeof(ngx_table_elt_t));

            eltp->value.len = elt->value.len;
            eltp->key.len = elt->key.len;
            eltp->value.data = ngx_pnalloc(headers->pool, eltp->value.len + 1);
            if (eltp->value.data == NULL) {
                goto fail;
            }

            eltp->key.data = ngx_pnalloc(headers->pool, eltp->key.len + 1);
            if (eltp->key.data == NULL) {
                goto fail;
            }

            ngx_memcpy(eltp->value.data, elt->value.data, elt->value.len);
            ngx_memcpy(eltp->key.data, elt->key.data, elt->key.len);

            eltp->value.data[eltp->value.len] = '\0';
            eltp->key.data[eltp->key.len] = '\0';
        }
    }

    /* body */

    if (body_len) {
        len = body_len + sizeof(LF);

        b = ngx_create_temp_buf(r->pool, len);
        if (b == NULL) {
            goto fail;
        }

        b->last = ngx_copy(b->last, body, body_len);
        *b->last++ = LF;

        if (r == r->main) {
            dd("main request setting last");
            b->last_buf = 1;

        } else {
            dd("subrequest setting last");
            b->last_in_chain = 1;
            b->sync = 1;
        }

        cl = ngx_wasm_chain_get_free_buf(rctx->pool,
                                         &rctx->free_bufs, len,
                                         buf_tag, 1);
        if (cl == NULL) {
            goto fail;
        }

        cl->buf = b;
        cl->next = NULL;

        rctx->local_resp_body = cl;
        rctx->local_resp_body_len = len;
    }

    rctx->resp_content_chosen = 1;

    return NGX_OK;

fail:

    ngx_http_wasm_discard_local_response(rctx);

    return NGX_ERROR;
}


ngx_int_t
ngx_http_wasm_flush_local_response(ngx_http_wasm_req_ctx_t *rctx)
{
    size_t               i;
    ngx_int_t            rc;
    ngx_table_elt_t     *elt;
    ngx_http_request_t  *r = rctx->r;

    if (!rctx->local_resp_status || r->header_sent) {
        return NGX_DECLINED;
    }

    if (rctx->local_resp_flushed) {
        ngx_wa_assert(0);
        return NGX_ABORT;
    }

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "wasm flushing local_response");

    rc = ngx_http_discard_request_body(r);
    if (rc != NGX_OK) {
        return NGX_ERROR;
    }

    ngx_http_wasm_set_resp_status(rctx,
                                  rctx->local_resp_status,
                                  rctx->local_resp_reason.data,
                                  rctx->local_resp_reason.len);

    for (i = 0; i < rctx->local_resp_headers.nelts; i++) {
        elt = &((ngx_table_elt_t *) rctx->local_resp_headers.elts)[i];

        rc = ngx_http_wasm_set_resp_header(r, &elt->key, &elt->value,
                                           NGX_HTTP_WASM_HEADERS_SET);
        if (rc != NGX_OK) {
            return NGX_ERROR;
        }
    }

    if (rctx->local_resp_body_len
        && ngx_http_set_content_type(r)
           != NGX_OK)
    {
        return NGX_ERROR;
    }

    if (rctx->local_resp_body_len >= 0
        && ngx_http_wasm_set_resp_content_length(r, rctx->local_resp_body_len)
           != NGX_OK)
    {
        return NGX_ERROR;
    }

    rctx->local_resp_status = 0;
    rctx->local_resp_flushed = 1;

    return ngx_http_wasm_send_chain_link(r, rctx->local_resp_body);
}
