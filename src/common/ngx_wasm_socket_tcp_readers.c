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

    if (src) {
        dd("src %s: %p: %.*s (pos: %p, last: %p)",
           fmt, src, (int) ngx_buf_size(src), src->pos,
           src->pos, src->last);
    }

    for (/* void */; buf_in; buf_in = buf_in->next) {
        buf = buf_in->buf;
        chunk_len = buf->last - buf->pos;

        dd("buf_in %s: %p: %.*s", fmt, buf,
           (int) chunk_len, buf->pos);
    }
}
#endif


#if 0
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
#endif


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
    loc = ngx_http_get_module_loc_conf(in_ctx->rctx->r, ngx_http_wasm_module);

    if (!in_ctx->status_code && r->state == 0) {
        /* buffer filled with "\r\n" */
        r->header_in->pos = r->header_in->start;
        r->header_in->last = r->header_in->start;

        return NGX_OK;
    }

    if (r->state != 0
        && (size_t) (r->header_in->pos - old)
            >= loc->socket_large_buffers.size)
    {
        ngx_wasm_log_error(NGX_LOG_ERR, sock->log, 0,
                           "tcp socket - upstream response headers "
                           "too large, "
                           "increase wasm_socket_large_buffers size");

        return NGX_DECLINED;
    }

    sock = in_ctx->sock;

#if 0
    if (sock->free_large_bufs) {
        cl = sock->free_large_bufs;
        sock->free_large_bufs = cl->next;
        b = cl->buf;

        ngx_log_debug2(NGX_LOG_DEBUG_HTTP, sock->log, 0,
                       "wasm tcp socket large buffer free: %p %uz",
                       b->pos, b->end - b->last);

    } else
#endif
    if (sock->lbusy < loc->socket_large_buffers.num) {

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
                       "wasm tcp socket large buffer alloc: %p %uz",
                       b->pos, b->end - b->last);

    } else {
        ngx_wasm_log_error(NGX_LOG_ERR, sock->log, 0,
                           "tcp socket - not enough large buffers available, "
                           "increase wasm_socket_large_buffers number");

        return NGX_DECLINED;
    }

    cl->next = sock->busy_large_bufs;
    sock->busy_large_bufs = cl;
    sock->lbusy++;

    if (r->state == 0) {
        /**
         * r->state == 0, a header line was parsed successfully
         * no need to copy an incomplete header line or relocate the parser header
         * pointers
         */
        sock->buffer = *b;

        return NGX_OK;
    }

    ngx_log_debug2(NGX_LOG_DEBUG_HTTP, sock->log, 0,
                   "wasm tcp socket large buffer copy: %uz "
                   "(avail: %uz)",
                   r->header_in->pos - old, b->end - b->pos);

#if 1
    ngx_wasm_assert(r->header_in->pos - old <= b->end - b->start);

#else
    if (r->header_in->pos - old > b->end - b->start) {
        ngx_wasm_log_error(NGX_LOG_ERR, sock->log, 0,
                           "tcp socket - upstream response headers "
                           "too large to copy, "
                           "increase wasm_socket_large_buffer_size size");

        return NGX_ERROR;
    }
#endif

    new = b->start;

    ngx_memcpy(new, old, r->header_in->pos - old);

    b->pos = new + (r->header_in->pos - old);
    b->last = new + (r->header_in->pos - old);

    if (!in_ctx->status_code) {
        r->request_start = new;

        if (r->request_end) {
            r->request_end = new + (r->request_end - old);
        }

        if (r->schema_start) {
            r->schema_start = new + (r->schema_start - old);
            r->schema_end = new + (r->schema_end - old);
        }

        if (r->http_protocol.data) {
            r->http_protocol.data = new + (r->http_protocol.data - old);
        }

        r->method_end = new + (r->method_end - old);
#if 0
        r->uri_start = new + (r->uri_start - old);
        r->uri_end = new + (r->uri_end - old);

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
#endif

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
    off_t                            chunk_len;
    ngx_int_t                        rc;
    u_char                          *p;
    ngx_buf_t                       *b, *buf;
    ngx_chain_t                     *cl, *ll;
    ngx_table_elt_t                 *h;
    ngx_http_request_t              *r;
    ngx_http_status_t               *status;
    ngx_http_upstream_header_t      *hh;
    ngx_http_upstream_headers_in_t  *headers_in;
    ngx_http_upstream_main_conf_t   *umcf;
    ngx_http_wasm_req_ctx_t         *rctx;

    ngx_wasm_assert(bytes);

    r = &in_ctx->fake_r;
    rctx = in_ctx->rctx;
    umcf = ngx_http_get_module_main_conf(rctx->r, ngx_http_upstream_module);

    if (!r->request_start) {
        r->header_in = src;
        r->request_start = src->pos;
    }

    headers_in = &r->upstream->headers_in;

    if (!in_ctx->header_done) {

        if (!headers_in->status_n) {

            /* status line */

            status = &in_ctx->status;

            rc = ngx_http_parse_status_line(r, src, status);
            if (rc == NGX_ERROR) {
                return NGX_ERROR;
            }

            if (rc == NGX_AGAIN) {
                if (src->last == src->end) {
                    rc = ngx_wasm_http_alloc_large_buffer(in_ctx);
                    if (rc == NGX_ERROR || rc == NGX_DECLINED) {
                        return NGX_ERROR;
                    }
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
                if (src->last == src->end) {
                    rc = ngx_wasm_http_alloc_large_buffer(in_ctx);
                    if (rc == NGX_ERROR) {
                        return NGX_ERROR;
                    }

                    if (rc == NGX_DECLINED) {
                        return NGX_ERROR;
                    }
                }

                return NGX_AGAIN;
            }

            buf_in->buf->last = src->pos;

            if (rc == NGX_HTTP_PARSE_HEADER_DONE) {
                in_ctx->header_done = 1;
                in_ctx->headers_len = buf_in->buf->last - buf_in->buf->start;

                ngx_log_debug3(NGX_LOG_DEBUG_WASM, in_ctx->log, 0,
                               "wasm http read all headers "
                               "(content-length: %O, chunked: %d, "
                               "headers_len: %d)",
                               headers_in->content_length_n,
                               headers_in->chunked, in_ctx->headers_len);

                if ((ssize_t) (src->pos - src->start) >= bytes) {

                    if (!headers_in->content_length_n) {
                        /* no body */
                        return NGX_OK;
                    }

                    return NGX_AGAIN;
                }

                /* more to parse in src */
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

        }  /* for ( ;; ) */
    }

    /* body */

    if (headers_in->content_length_n >= 0 || headers_in->chunked) {

        for ( ;; ) {

            /* incoming chunk */

            ll = in_ctx->chunks;

            for (/* void */; ll && ll->next; ll = ll->next) { /* void */ }

#if 0
            if (ll && ll->buf->last == NULL) {
                cl = ll;

            } else {
#endif
                cl = ngx_chain_get_free_buf(in_ctx->pool, &rctx->free_bufs);
                if (cl == NULL) {
                    return NGX_ERROR;
                }

                ngx_memzero(cl->buf, sizeof(ngx_buf_t));

                cl->buf->start = buf_in->buf->start;
                cl->buf->end = buf_in->buf->end;

                if (ll == NULL) {
                    in_ctx->chunks = cl;

                } else {
                    ll->next = cl;
                }
#if 0
            }
#endif

            b = cl->buf;

            if (headers_in->chunked && !in_ctx->rest) {

                /* Transfer-Encoding: chunked */

                rc = ngx_http_parse_chunked(r, src, &in_ctx->chunked);
                if (rc == NGX_ERROR || rc == NGX_AGAIN) {
                    return rc;
                }

                buf_in->buf->last = src->pos;

                if (rc == NGX_DONE) {
                    break;
                }

                ngx_wasm_assert(rc == NGX_OK);

                in_ctx->body_len += in_ctx->chunked.size;
                in_ctx->rest = in_ctx->chunked.size;

            } else if (headers_in->content_length_n >= 0) {

                /* Content-Length */

                if (!in_ctx->body_len) {
                    in_ctx->body_len = headers_in->content_length_n;
                    in_ctx->rest = headers_in->content_length_n;
                }

                if (!in_ctx->rest) {
                    break;
                }
            }

            b->pos = src->pos;

            ngx_log_debug1(NGX_LOG_DEBUG_WASM, in_ctx->log, 0,
                           "wasm http reading body chunk (rest: %O)",
                           in_ctx->rest);

            bytes = ngx_buf_size(src);

            rc = ngx_wasm_read_bytes(src, buf_in, bytes,
                                     (size_t *) &in_ctx->rest);

            b->last = buf_in->buf->last;

            if (rc == NGX_ERROR || rc == NGX_AGAIN) {
                return rc;
            }

            ngx_wasm_assert(rc == NGX_OK);
            ngx_wasm_assert(in_ctx->rest == 0);

            in_ctx->chunked.size = 0;

        }  /* for ( ;; ) */

        ngx_log_debug1(NGX_LOG_DEBUG_WASM, in_ctx->log, 0,
                       "wasm http read body finished (body_len: %O)",
                       in_ctx->body_len);
    }

    if (in_ctx->body_len) {

        /* copy body to close socket */

        in_ctx->body = ngx_wasm_chain_get_free_buf(in_ctx->pool,
                                                   &rctx->free_bufs,
                                                   in_ctx->body_len,
                                                   in_ctx->sock->env.buf_tag,
                                                   in_ctx->sock->buffer_reuse);
        if (in_ctx->body == NULL) {
            return NGX_ERROR;
        }

        cl = in_ctx->chunks;
        buf = in_ctx->body->buf;

        for (/* void */; cl; cl = cl->next) {
            b = cl->buf;
            p = b->pos;
            chunk_len = b->last - p;

            if (!chunk_len) {
                continue;
            }

            /* body */

            buf->last = ngx_cpymem(buf->last, p, chunk_len);
        }

        ngx_wasm_assert((size_t) ngx_buf_size(buf) == in_ctx->body_len);

#if 0
        dd("body_len: %ld", in_ctx->body_len);

        for (ll = in_ctx->body; ll; ll = ll->next) {
            b = ll->buf;
            dd("body: %p: %.*s", b,
               (int) (b->last - b->start), b->start);
        }
#endif

    }

    return NGX_OK;
}
#endif
