#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_wasm_socket_tcp_readers.h>


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
ngx_int_t
ngx_wasm_read_http_response(ngx_buf_t *src, ngx_chain_t *buf_in, ssize_t bytes,
    ngx_wasm_http_reader_ctx_t *in_ctx)
{
    u_char                          *pos;
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

    umcf = ngx_http_get_module_main_conf(in_ctx->r, ngx_http_upstream_module);

    r = &in_ctx->fake_r;

    if (!r->signature) {
        ngx_memzero(r, sizeof(ngx_http_request_t));

        r->connection = in_ctx->r->connection;
        r->pool = in_ctx->pool;
        r->signature = NGX_HTTP_MODULE;
        r->header_in = src;

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
                return NGX_ERROR;
            }

            buf_in->buf->last = src->pos;

            if (rc == NGX_AGAIN) {
                return NGX_AGAIN;
            }

            ngx_wasm_assert(rc == NGX_OK);

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

            in_ctx->headers_len += headers_in->status_line.len;

            return NGX_AGAIN;
        }

        /* headers */

        pos = src->pos;

        rc = ngx_http_parse_header_line(r, src, 1);
        if (rc == NGX_HTTP_PARSE_INVALID_HEADER) {
            return NGX_ERROR;
        }

        buf_in->buf->last = src->pos;

        if (rc == NGX_AGAIN) {
            return NGX_AGAIN;
        }

        /* header */

        if (rc == NGX_OK) {
            h = ngx_list_push(&headers_in->headers);
            if (h == NULL) {
                return NGX_ERROR;
            }

            h->hash = r->header_hash;
            h->key.len = r->header_name_end - r->header_name_start;
            h->value.len = r->header_end - r->header_start;
            h->key.data = ngx_pnalloc(in_ctx->pool,
                                      h->key.len + 1 + h->value.len + 1 + h->key.len);
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

            in_ctx->headers_len += buf_in->buf->last - pos;;

            return NGX_AGAIN;
        }

        ngx_wasm_assert(rc == NGX_HTTP_PARSE_HEADER_DONE);

        ngx_log_debug2(NGX_LOG_DEBUG_WASM, in_ctx->log, 0,
                       "wasm http read all headers "
                       "(content-length: %O, chunked: %d)",
                       headers_in->content_length_n,
                       headers_in->chunked);

        in_ctx->headers_len += buf_in->buf->last - pos;;
        in_ctx->headers_len += sizeof(CRLF) - 1;
        in_ctx->header_done = 1;

        return NGX_AGAIN;
    }

    /* body */

    if (!in_ctx->body_done) {

        if (headers_in->chunked) {

            /* Transfer-Encoding: chunked */

            for ( ;; ) {

                if (in_ctx->rest == 0) {
                    rc = ngx_http_parse_chunked(r, src, &in_ctx->chunked);
                    if (rc == NGX_ERROR) {
                       return NGX_ERROR;
                    }

                    buf_in->buf->last = src->pos;

                    if (rc == NGX_AGAIN) {
                        return NGX_AGAIN;
                    }

                    if (rc == NGX_DONE) {
                        break;
                    }

                    ngx_wasm_assert(rc == NGX_OK);

                    /* incoming chunk */

                    cl = ngx_chain_get_free_buf(in_ctx->pool, &in_ctx->rctx->free_bufs);
                    if (cl == NULL) {
                        return NGX_ERROR;
                    }

                    b = cl->buf;

                    ngx_memzero(b, sizeof(ngx_buf_t));

                    if (in_ctx->body == NULL) {
                        in_ctx->body = cl;

                    } else {
                        in_ctx->cl->next = cl;
                    }

                    in_ctx->cl = cl;
                    in_ctx->rest = in_ctx->chunked.size;
                }

                ngx_wasm_assert(in_ctx->cl);

                cl = in_ctx->cl;
                b = cl->buf;

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

            rc = ngx_wasm_read_bytes(src, buf_in, bytes, &in_ctx->rest);
            if (rc == NGX_ERROR || rc == NGX_AGAIN) {
                return rc;
            }

            ngx_wasm_assert(rc == NGX_OK);
            ngx_wasm_assert(in_ctx->rest == 0);

            in_ctx->body_len = headers_in->content_length_n;
        }

        ngx_log_debug0(NGX_LOG_DEBUG_WASM, in_ctx->log, 0,
                       "wasm http read body finished");

        in_ctx->body_done = 1;
#if 0
        {
            ngx_buf_t    *buf;
            ngx_chain_t  *bufs_in = in_ctx->body;
            size_t        chunk_len;

            for (/* void */; bufs_in; bufs_in = bufs_in->next) {
                buf = bufs_in->buf;
                chunk_len = buf->last - buf->pos;

                dd("body: %p: %.*s", buf,
                   (int) chunk_len, buf->pos);
            }
        }
#endif
    }

    return NGX_OK;
}
#endif
