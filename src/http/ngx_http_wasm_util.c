#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_http_wasm_util.h>


static const ngx_buf_tag_t   buf_tag = &ngx_http_wasm_module;


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
        goto done;
    }

    if (rc > NGX_OK || r->header_only) {
        goto sent_last;
    }

    ngx_wasm_assert(r->header_sent);
    ngx_wasm_assert(rc == NGX_OK || rc == NGX_AGAIN || rc == NGX_DONE);

    if (in == NULL) {
        rc = ngx_http_send_special(r, NGX_HTTP_LAST);
        if (rc == NGX_ERROR) {
            goto done;

        } else if (rc < NGX_HTTP_SPECIAL_RESPONSE) {
            /* special response >= 300 */
            rc = NGX_OK;
        }

        goto sent_last;
    }

    rc = ngx_http_output_filter(r, in);
    if (rc == NGX_ERROR) {
        goto done;
    }

    ngx_wasm_assert(rc == NGX_OK || rc == NGX_DONE);

sent_last:

    rctx->resp_sent_last = 1;

done:

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
    ngx_uint_t                fill = 0;
    ngx_buf_t                *buf;
    ngx_chain_t              *nl;
    ngx_http_request_t       *r;
    ngx_http_request_body_t  *rb;

    r = rctx->r;

    if (r->header_sent || rctx->resp_content_chosen) {
        return NGX_DECLINED;
    }

    rb = r->request_body;

    if (rb) {
        fill = ngx_wasm_chain_clear(rb->bufs, at, NULL, NULL);
    }

    body->len = ngx_min(body->len, max);

    if (!body->len) {
        goto done;
    }

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

    buf = ngx_create_temp_buf(r->pool, body->len + fill);
    if (buf == NULL) {
        return NGX_ERROR;
    }

    nl = ngx_alloc_chain_link(r->pool);
    if (nl == NULL) {
        return NGX_ERROR;
    }

    rb->buf = buf;

    nl->buf = buf;
    nl->next = NULL;

    /* set */

    if (fill) {
        ngx_memset(buf->last, ' ', fill);
        buf->last += fill;
    }

    buf->last = ngx_cpymem(buf->last, body->data, body->len);
    buf->last_buf = 1;

    ngx_wasm_chain_update_chains(r->pool, &rctx->free, &rb->bufs,
                                 &nl, buf_tag);

done:

    r->headers_in.content_length_n = body->len;

    return NGX_OK;
}


ngx_int_t
ngx_http_wasm_set_resp_body(ngx_http_wasm_req_ctx_t *rctx, ngx_str_t *body,
    size_t at, size_t max)
{
    unsigned                     eof = 0, flush = 0;
    ngx_uint_t                   fill;
    ngx_buf_t                   *buf = NULL;
    ngx_chain_t                 *in, *nl;
    ngx_http_request_t          *r;

    r = rctx->r;
    in = rctx->resp_chunk;
    if (in == NULL) {
        return NGX_DECLINED;
    }

    if (r->header_sent && !r->chunked) {
        ngx_wasm_log_error(NGX_LOG_WARN, r->connection->log, 0,
                           "overriding response body chunk while "
                           "Content-Length header already sent");
    }

    fill = ngx_wasm_chain_clear(in, at, &eof, &flush);

    body->len = ngx_min(body->len, max);

    if (body->len) {
        /* append new buffer */
        nl = ngx_chain_get_free_buf(r->pool, &rctx->free);
        if (nl == NULL) {
            return NGX_ERROR;
        }

        nl->buf = ngx_create_temp_buf(r->pool, body->len + fill);
        if (nl->buf == NULL) {
            return NGX_ERROR;
        }

        /* set */

        buf = nl->buf;

        if (fill) {
            ngx_memset(buf->last, ' ', fill);
            buf->last += fill;
        }

        buf->memory = 1;
        buf->tag = buf_tag;
        buf->last = ngx_cpymem(buf->last, body->data, body->len);

        if (flush) {
            buf->flush = 1;
        }

        if (eof) {
            if (r == r->main) {
                buf->last_buf = 1;

            } else {
                buf->last_in_chain = 1;
            }
        }

        ngx_wasm_chain_update_chains(r->pool, &rctx->free, &rctx->resp_chunk,
                                     &nl, buf_tag);

    } else {
        /* discard chunk */
        rctx->resp_chunk = NULL;
    }

    rctx->resp_chunk_len = ngx_wasm_chain_len(rctx->resp_chunk,
                                              &rctx->resp_chunk_eof);

    return NGX_OK;
}
