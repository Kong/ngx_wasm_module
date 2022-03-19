#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_http_wasm_util.h>


ngx_str_t *
ngx_http_copy_escaped(ngx_str_t *dst, ngx_pool_t *pool,
    ngx_http_wasm_escape_kind kind)
{
    size_t   escape, len;
    u_char  *data;

    data = dst->data;
    len = dst->len;

    escape = ngx_http_wasm_escape(NULL, data, len, kind);
    if (escape > 0) {
        /* null-terminated for ngx_http_header_filter */
        dst->len = len + 2 * escape;
        dst->data = ngx_pnalloc(pool, dst->len + 1);
        if (dst->data == NULL) {
            return NULL;
        }

        (void) ngx_http_wasm_escape(dst->data, data, len, kind);

        dst->data[dst->len] = '\0';
    }

    return dst;
}


ngx_int_t
ngx_http_wasm_read_client_request_body(ngx_http_request_t *r,
    ngx_http_client_body_handler_pt post_handler)
{
    ngx_int_t   rc;

    r->request_body_in_single_buf = 1;

    rc = ngx_http_read_client_request_body(r, post_handler);

    ngx_wasm_assert(rc != NGX_AGAIN);

    if (rc < NGX_HTTP_SPECIAL_RESPONSE
        && rc != NGX_ERROR)
    {
        r->main->count--;
    }

    return rc;
}


ngx_int_t
ngx_http_wasm_send_chain_link(ngx_http_request_t *r, ngx_chain_t *in)
{
    ngx_int_t                 rc;
    ngx_http_wasm_req_ctx_t  *rctx;

    if (ngx_http_wasm_rctx(r, &rctx) != NGX_OK) {
        return NGX_ERROR;
    }

    if (!r->headers_out.status) {
        r->headers_out.status = NGX_HTTP_OK;
    }

    rctx->resp_content_chosen = 1;

    rc = ngx_http_send_header(r);
    if (rc == NGX_ERROR) {
        return NGX_ERROR;
    }

    if (rc > NGX_OK || r->header_only) {
        goto done;
    }

    ngx_wasm_assert(r->header_sent);
    ngx_wasm_assert(rc == NGX_OK || rc == NGX_AGAIN || rc == NGX_DONE);

    if (in == NULL) {
        rc = ngx_http_send_special(r, NGX_HTTP_LAST);
        if (rc == NGX_ERROR) {
            return NGX_ERROR;

        } else if (rc < NGX_HTTP_SPECIAL_RESPONSE) {
            /* special response >= 300 */
            rc = NGX_OK;
        }

        goto done;
    }

    rc = ngx_http_output_filter(r, in);
    if (rc == NGX_ERROR) {
        return NGX_ERROR;
    }

    ngx_wasm_assert(rc == NGX_OK || rc == NGX_AGAIN || rc == NGX_DONE);

done:

    if (rc != NGX_AGAIN) {
        rctx->resp_sent_last = 1;
    }

    return rc;
}


ngx_int_t
ngx_http_wasm_produce_resp_headers(ngx_http_wasm_req_ctx_t *rctx)
{
    ngx_int_t                  rc;
    static ngx_str_t           date = ngx_string("Date");
    ngx_str_t                  date_val;
    static ngx_str_t           last_modified = ngx_string("Last-Modified");
    ngx_str_t                  last_mod_val;
    static ngx_str_t           server = ngx_string("Server");
    ngx_str_t                 *server_val = NULL;
    static ngx_str_t           server_full = ngx_string(NGINX_VER);
    static ngx_str_t           server_build = ngx_string(NGINX_VER_BUILD);
    static ngx_str_t           server_default = ngx_string("nginx");
    ngx_http_request_t        *r = rctx->r;
    ngx_http_core_loc_conf_t  *clcf;

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "wasm producing default response headers");

    if (r->headers_out.server == NULL) {
        /* Server */

        clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);

        if (clcf->server_tokens == NGX_HTTP_SERVER_TOKENS_ON) {
            server_val = &server_full;

        } else if (clcf->server_tokens == NGX_HTTP_SERVER_TOKENS_BUILD) {
            server_val = &server_build;

        } else {
            server_val = &server_default;
        }

        if (server_val) {
            if (ngx_http_wasm_set_resp_header(r, &server, server_val,
                                              NGX_HTTP_WASM_HEADERS_SET)
                != NGX_OK)
            {
                return NGX_ERROR;
            }
        }
    }

    if (r->headers_out.date == NULL) {
        /* Date */

        date_val.len = ngx_cached_http_time.len;
        date_val.data = ngx_cached_http_time.data;

        rc = ngx_http_wasm_set_resp_header(r, &date, &date_val, 0);
        if (rc != NGX_OK) {
            return NGX_ERROR;
        }
    }

    if (r->headers_out.last_modified == NULL
        && r->headers_out.last_modified_time != -1)
    {
        /* Last-Modified */

        last_mod_val.len = sizeof("Mon, 28 Sep 1970 06:00:00 GMT") - 1;
        last_mod_val.data = ngx_pnalloc(r->pool, last_mod_val.len);
        if (last_mod_val.data == NULL) {
            return NGX_ERROR;
        }

        ngx_http_time(last_mod_val.data, r->headers_out.last_modified_time);

        rc = ngx_http_wasm_set_resp_header(r, &last_modified, &last_mod_val, 0);
        if (rc != NGX_OK) {
            return NGX_ERROR;
        }

        /* skip in ngx_http_header_filter, may affect stock logging */
        r->headers_out.last_modified_time = -1;
    }

    /* TODO: Location */

    return NGX_OK;
}


ngx_int_t
ngx_http_wasm_set_req_body(ngx_http_wasm_req_ctx_t *rctx, ngx_str_t *body,
    size_t at, size_t max)
{
    ngx_http_request_t       *r;
    ngx_http_request_body_t  *rb;

    r = rctx->r;

    if (r->header_sent || rctx->resp_content_chosen) {
        return NGX_DECLINED;
    }

    rb = r->request_body;

    body->len = ngx_min(body->len, max);

    if (rb == NULL) {
        rb = ngx_pcalloc(r->pool, sizeof(ngx_http_request_body_t));
        if (rb == NULL) {
            return NGX_ERROR;
        }

        /*
         * set by ngx_pcalloc():
         *
         *     rb->bufs = NULL;
         *     rb->buf = NULL;
         *     rb->free = NULL;
         *     rb->busy = NULL;
         *     rb->chunked = NULL;
         *     rb->post_handler = NULL;
         */

        rb->rest = -1;

        r->request_body = rb;
    }

    if (ngx_wasm_chain_append(r->connection->pool, &rb->bufs, at, body,
                              &rctx->free_bufs, buf_tag)
        != NGX_OK)
    {
        return NGX_ERROR;
    }

    r->headers_in.content_length_n = ngx_wasm_chain_len(rb->bufs, NULL);

    return NGX_OK;
}


ngx_int_t
ngx_http_wasm_set_resp_body(ngx_http_wasm_req_ctx_t *rctx, ngx_str_t *body,
    size_t at, size_t max)
{
    ngx_http_request_t  *r = rctx->r;

    if (rctx->resp_chunk == NULL) {
        return NGX_DECLINED;
    }

    if (r->header_sent && !r->chunked) {
        ngx_wasm_log_error(NGX_LOG_WARN, r->connection->log, 0,
                           "overriding response body chunk while "
                           "Content-Length header already sent");
    }

    body->len = ngx_min(body->len, max);

    if (ngx_wasm_chain_append(r->connection->pool, &rctx->resp_chunk, at, body,
                              &rctx->free_bufs, buf_tag)
        != NGX_OK)
    {
        return NGX_ERROR;
    }

    rctx->resp_chunk_len = ngx_wasm_chain_len(rctx->resp_chunk,
                                              &rctx->resp_chunk_eof);

#if 0
    if (!rctx->resp_chunk_len) {
        /* discard chunk */
        rctx->resp_chunk = NULL;
    }
#endif

    return NGX_OK;
}
