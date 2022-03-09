#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_wasm_socket_tcp.h>
#include <ngx_wasm_socket_tcp_readers.h>


#if 0
static void
ngx_wasm_read_log(ngx_buf_t *src, ngx_chain_t *buf_in, char *fmt)
{
    size_t      chunk_len;
    ngx_buf_t  *buf;

    for (/* void */; buf_in; buf_in = buf_in->next) {
        buf = buf_in->buf;
        chunk_len = buf->last - buf->start;

        dd("buf_in %s: %p: %.*s", fmt, buf,
           (int) chunk_len, buf->start);
    }

    dd("src %s: %p: %.*s", fmt, src,
       (int) (src->last - src->pos), src->pos);
}
#endif


ngx_int_t
ngx_wasm_read_all(ngx_buf_t *src, ngx_chain_t *buf_in, ssize_t bytes)
{
    if (bytes == 0) {
        return NGX_OK;
    }

    buf_in->buf->last += bytes;
    src->pos += bytes;

    return NGX_AGAIN;
}


ngx_int_t
ngx_wasm_read_bytes(ngx_buf_t *src, ngx_chain_t *buf_in, ssize_t bytes,
    size_t *rest)
{
    if (bytes == 0) {
        return NGX_ERROR;
    }

    if ((size_t) bytes >= *rest) {
        buf_in->buf->last += *rest;
        src->pos += *rest;
        *rest = 0;
        return NGX_OK;
    }

    buf_in->buf->last += bytes;
    src->pos += bytes;
    *rest -= bytes;

    return NGX_AGAIN;
}


ngx_int_t
ngx_wasm_read_line(ngx_buf_t *src, ngx_chain_t *buf_in, ssize_t bytes)
{
    u_char  *dst;
    u_char   c;

    if (bytes == 0) {
        return NGX_ERROR;
    }

    dst = buf_in->buf->last;

    while (bytes--) {
        c = *src->pos++;

        switch (c) {
        case LF:
            buf_in->buf->last = dst;
            return NGX_OK;

        case CR:
            break;

        default:
            *dst++ = c;
            break;
        }
    }

    buf_in->buf->last = dst;

    return NGX_AGAIN;
}


#ifdef NGX_WASM_HTTP
static ngx_int_t
ngx_wasm_http_alloc_large_buffer(ngx_wasm_http_reader_ctx_t *in_ctx)
{
    u_char                          *old, *new;
    ngx_buf_t                       *b;
    ngx_chain_t                     *cl;
    ngx_http_request_t              *r;
    ngx_http_status_t               *status;
    ngx_http_wasm_loc_conf_t        *loc;
    ngx_http_upstream_headers_in_t  *headers_in;
    ngx_wasm_socket_tcp_t           *sock = in_ctx->sock;

    r = &in_ctx->fake_r;
    old = in_ctx->status_code ? r->header_name_start : r->request_start;
    loc = ngx_http_get_module_loc_conf(in_ctx->r, ngx_http_wasm_module);

    if (!in_ctx->status_code && r->state == 0) {
        /* the client fills up the buffer with "\r\n" */
        r->header_in->pos = r->header_in->start;
        r->header_in->last = r->header_in->start;

        return NGX_OK;
    }

    if (r->state != 0
        && (size_t) (r->header_in->pos - old)
            >= loc->socket_large_buffers.size)
    {
        ngx_log_error(NGX_LOG_ERR, sock->log, 0,
                      "wasm socket - upstream response headers too large");
        return NGX_DECLINED;
    }

    sock = in_ctx->sock;

    if (sock->free) {
        cl = sock->free;
        sock->free = cl->next;
        b = cl->buf;

        ngx_log_debug2(NGX_LOG_DEBUG_HTTP, sock->log, 0,
                       "wasm socket large buffer free: %p %uz",
                       b->pos, b->end - b->last);

    } else if (sock->nbusy < loc->socket_large_buffers.num) {
        b = ngx_create_temp_buf(sock->pool, loc->socket_large_buffers.size);
        if (b == NULL) {
            return NGX_ERROR;
        }

        cl = ngx_alloc_chain_link(sock->pool);
        if (cl == NULL) {
            return NGX_ERROR;
        }

        cl->buf = b;

        ngx_log_debug2(NGX_LOG_DEBUG_HTTP, sock->log, 0,
                       "wasm socket large buffer alloc: %p %uz",
                       b->pos, b->end - b->last);

    } else {
        ngx_log_error(NGX_LOG_ERR, sock->log, 0,
                      "wasm socket - large buffers limit reached");
        return NGX_DECLINED;
    }

    cl->next = sock->busy;
    sock->busy = cl;
    sock->nbusy++;

    if (r->state == 0) {
        /*
         * r->state == 0 means that a header line was parsed successfully
         * and we do not need to copy incomplete header line and
         * to relocate the parser header pointers
         */

        sock->buffer = *b;

        return NGX_OK;
    }

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, sock->log, 0,
                   "wasm socket large buffer copy: %uz",
                   r->header_in->pos - old);

    if (r->header_in->pos - old > b->end - b->start) {
        ngx_log_error(NGX_LOG_ERR, sock->log, 0,
                      "wasm socket - upstream response headers "
                      "too large to copy");
        return NGX_ERROR;
    }

    new = b->start;

    ngx_memcpy(new, old, r->header_in->pos - old);

    b->pos = new + (r->header_in->pos - old);
    b->last = new + (r->header_in->pos - old);

    if (!in_ctx->status_code) {
        r->request_start = new;

        if (r->request_end) {
            r->request_end = new + (r->request_end - old);
        }

        r->method_end = new + (r->method_end - old);
        r->uri_start = new + (r->uri_start - old);
        r->uri_end = new + (r->uri_end - old);

        if (r->schema_start) {
            r->schema_start = new + (r->schema_start - old);
            r->schema_end = new + (r->schema_end - old);
        }

        if (r->host_start) {
            r->host_start = new + (r->host_start - old);
            if (r->host_end) {
                r->host_end = new + (r->host_end - old);
            }
        }

        if (r->port_start) {
            r->port_start = new + (r->port_start - old);
            r->port_end = new + (r->port_end - old);
        }

        if (r->uri_ext) {
            r->uri_ext = new + (r->uri_ext - old);
        }

        if (r->args_start) {
            r->args_start = new + (r->args_start - old);
        }

        if (r->http_protocol.data) {
            r->http_protocol.data = new + (r->http_protocol.data - old);
        }

        status = &in_ctx->status;

        if (status->start) {
            status->start = new + (status->start - old);
            if (status->end) {
                status->end = new + (status->end - old);
            }
        }

        headers_in = &r->upstream->headers_in;

        if (headers_in->status_line.data) {
            headers_in->status_line.data = new + (headers_in->status_line.data
                                                  - old);
        }

    } else {
        r->header_name_start = new;
        r->header_name_end = new + (r->header_name_end - old);
        r->header_start = new + (r->header_start - old);
        r->header_end = new + (r->header_end - old);
    }

    sock->buf_in = cl;
    sock->buffer = *b;

    return NGX_OK;
}


ngx_int_t
ngx_wasm_read_http_response(ngx_buf_t *src, ngx_chain_t *buf_in, ssize_t bytes,
    ngx_wasm_http_reader_ctx_t *in_ctx)
{
    ngx_int_t                        rc;
    ngx_buf_t                       *b;
    ngx_chain_t                     *cl;
    ngx_table_elt_t                 *h;
    ngx_http_request_t              *r;
    ngx_http_status_t               *status;
    ngx_http_upstream_header_t      *hh;
    ngx_http_upstream_headers_in_t  *headers_in;
    ngx_http_upstream_main_conf_t   *umcf;

    if (bytes == 0) {
        return NGX_ERROR;
    }

    r = &in_ctx->fake_r;
    umcf = ngx_http_get_module_main_conf(in_ctx->r, ngx_http_upstream_module);

    if (!r->signature) {
        ngx_memzero(r, sizeof(ngx_http_request_t));

        r->signature = NGX_HTTP_MODULE;
        r->connection = in_ctx->r->connection;
        r->pool = in_ctx->pool;
        r->header_in = src;
        r->request_start = src->pos;

        if (ngx_list_init(&r->headers_out.headers, r->pool, 20,
                          sizeof(ngx_table_elt_t))
            != NGX_OK)
        {
            return NGX_ERROR;
        }

        if (ngx_list_init(&r->headers_out.trailers, r->pool, 4,
                          sizeof(ngx_table_elt_t))
            != NGX_OK)
        {
            return NGX_ERROR;
        }

        if (ngx_http_upstream_create(r) != NGX_OK) {
            return NGX_ERROR;
        }

        if (ngx_list_init(&r->upstream->headers_in.headers, r->pool, 4,
                          sizeof(ngx_table_elt_t))
            != NGX_OK)
        {
            return NGX_ERROR;
        }

        r->headers_out.content_length_n = -1;
        r->headers_out.last_modified_time = -1;
    }

    headers_in = &r->upstream->headers_in;

    if (!in_ctx->header_done) {

        if (!headers_in->status_n) {

            /* status line */

            status = &in_ctx->status;

            rc = ngx_http_parse_status_line(r, src, status);
            if (rc == NGX_ERROR) {
                return rc;
            }

            if (rc == NGX_AGAIN) {
                if (ngx_wasm_http_alloc_large_buffer(in_ctx) != NGX_OK) {
                    return NGX_ERROR;
                }

                return NGX_AGAIN;
            }

            ngx_wasm_assert(rc == NGX_OK);

            buf_in->buf->last = src->pos;

            headers_in->status_n = status->code;
            headers_in->status_line.len = status->end - status->start;
            headers_in->status_line.data = ngx_pnalloc(in_ctx->pool,
                                                       headers_in->status_line.len);
            if (headers_in->status_line.data == NULL) {
                return NGX_ERROR;
            }

            ngx_memcpy(headers_in->status_line.data, status->start,
                       headers_in->status_line.len);

            if (status->http_version < NGX_HTTP_VERSION_11) {
                headers_in->connection_close = 1;
            }

            ngx_log_debug2(NGX_LOG_DEBUG_HTTP, in_ctx->log, 0,
                           "wasm http reader status %ui \"%V\"",
                           headers_in->status_n, &headers_in->status_line);

            in_ctx->status_code = headers_in->status_n;

            return NGX_AGAIN;
        }

        /* headers */

        for ( ;; ) {

            rc = ngx_http_parse_header_line(r, src, 1);
            if (rc == NGX_HTTP_PARSE_INVALID_HEADER) {
                return NGX_ERROR;
            }

            if (rc == NGX_AGAIN) {
                if (ngx_wasm_http_alloc_large_buffer(in_ctx) != NGX_OK) {
                    return NGX_ERROR;
                }

                return NGX_AGAIN;
            }

            buf_in->buf->last = src->pos;

            if (rc == NGX_HTTP_PARSE_HEADER_DONE) {
                ngx_log_debug2(NGX_LOG_DEBUG_WASM, in_ctx->log, 0,
                               "wasm http read all headers "
                               "(content-length: %O, chunked: %d)",
                               headers_in->content_length_n,
                               headers_in->chunked);

                in_ctx->header_done = 1;
                break;
            }

            ngx_wasm_assert(rc == NGX_OK);

            /* header */

            h = ngx_list_push(&headers_in->headers);
            if (h == NULL) {
                return NGX_ERROR;
            }

            h->hash = r->header_hash;
            h->key.len = r->header_name_end - r->header_name_start;
            h->value.len = r->header_end - r->header_start;
            h->key.data = ngx_pnalloc(in_ctx->pool, h->key.len + 1
                                      + h->value.len + 1
                                      + h->key.len);
            if (h->key.data == NULL) {
                return NGX_ERROR;
            }

            h->value.data = h->key.data + h->key.len + 1;
            h->lowcase_key = h->key.data + h->key.len + 1 + h->value.len + 1;

            ngx_memcpy(h->key.data, r->header_name_start, h->key.len);
            h->key.data[h->key.len] = '\0';
            ngx_memcpy(h->value.data, r->header_start, h->value.len);
            h->value.data[h->value.len] = '\0';

            if (h->key.len == r->lowcase_index) {
                ngx_memcpy(h->lowcase_key, r->lowcase_header, h->key.len);

            } else {
                ngx_strlow(h->lowcase_key, h->key.data, h->key.len);
            }

            hh = ngx_hash_find(&umcf->headers_in_hash, h->hash,
                               h->lowcase_key, h->key.len);

            if (hh && hh->handler(r, h, hh->offset) != NGX_OK) {
                return NGX_ERROR;
            }

            ngx_log_debug2(NGX_LOG_DEBUG_HTTP, in_ctx->log, 0,
                           "wasm http reader header: \"%V: %V\"",
                           &h->key, &h->value);
        }
    }

    /* body */

    if (!in_ctx->body_done) {

        /* incoming chunk */

        cl = ngx_chain_get_free_buf(in_ctx->pool, &in_ctx->rctx->free_bufs);
        if (cl == NULL) {
            return NGX_ERROR;
        }

        if (in_ctx->body == NULL) {
            in_ctx->body = cl;

        } else {
            in_ctx->body->next = cl;
        }

        b = cl->buf;

        ngx_memzero(b, sizeof(ngx_buf_t));

        if (headers_in->chunked) {

            /* Transfer-Encoding: chunked */

            for ( ;; ) {

                if (in_ctx->rest == 0) {
                    rc = ngx_http_parse_chunked(r, src, &in_ctx->chunked);
                    if (rc == NGX_ERROR) {
                        return NGX_ERROR;
                    }

                    if (rc == NGX_AGAIN) {
                        return NGX_AGAIN;
                    }

                    buf_in->buf->last = src->pos;

                    if (rc == NGX_DONE) {
                        return NGX_OK;
                    }

                    ngx_wasm_assert(rc == NGX_OK);

                    in_ctx->rest = in_ctx->chunked.size;
                }

                b->start = buf_in->buf->start;
                b->end = buf_in->buf->end;
                b->pos = buf_in->buf->last;

                rc = ngx_wasm_read_bytes(src, buf_in, src->last - src->pos,
                                         (size_t *) &in_ctx->chunked.size);
                if (rc == NGX_ERROR || rc == NGX_AGAIN) {
                    return rc;
                }

                ngx_wasm_assert(rc == NGX_OK);
                ngx_wasm_assert(in_ctx->chunked.size == 0);

                b->last = buf_in->buf->last;

                in_ctx->body_len += in_ctx->rest;
                in_ctx->rest = 0;
            }

        } else if (headers_in->content_length_n) {

            /* Content-Length */

            if (headers_in->content_length_n == 0) {
                ngx_log_debug0(NGX_LOG_DEBUG_WASM, in_ctx->log, 0,
                               "wasm http read finished (no body)");
                return NGX_OK;
            }

            if (in_ctx->rest == 0) {
                in_ctx->rest = headers_in->content_length_n;
            }

            ngx_log_debug1(NGX_LOG_DEBUG_WASM, in_ctx->log, 0,
                           "wasm http reading body (rest: %d)",
                           in_ctx->rest);

            b->start = src->pos;
            b->end = buf_in->buf->end;
            b->pos = buf_in->buf->last;

            rc = ngx_wasm_read_bytes(src, buf_in, bytes, &in_ctx->rest);
            if (rc == NGX_ERROR || rc == NGX_AGAIN) {
                return rc;
            }

            ngx_wasm_assert(rc == NGX_OK);
            ngx_wasm_assert(in_ctx->rest == 0);

            b->last = buf_in->buf->last;

            in_ctx->body_len = headers_in->content_length_n;
            in_ctx->rest = 0;
        }

        ngx_log_debug0(NGX_LOG_DEBUG_WASM, in_ctx->log, 0,
                       "wasm http read body finished");

        in_ctx->body_done = 1;
    }

    return NGX_OK;
}
#endif
