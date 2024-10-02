#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_wa_readers.h>


#if 0
static void
ngx_wa_read_log(ngx_buf_t *src, ngx_chain_t *buf_in, char *fmt)
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


ngx_int_t
ngx_wa_read_all(ngx_buf_t *src, ngx_chain_t *buf_in, ssize_t bytes)
{
    if (bytes == 0) {
        return NGX_OK;
    }

    buf_in->buf->last += bytes;
    src->pos += bytes;

    return NGX_AGAIN;
}


ngx_int_t
ngx_wa_read_line(ngx_buf_t *src, ngx_chain_t *buf_in, ssize_t bytes)
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
ngx_wa_read_bytes(ngx_buf_t *src, ngx_chain_t *buf_in, ssize_t bytes,
    size_t *rest)
{
    if (bytes == 0) {
        return NGX_ERROR;
    }

    if ((size_t) bytes >= *rest) {
        src->pos += *rest;
        buf_in->buf->last = src->pos;
        *rest = 0;

        return NGX_OK;
    }

    buf_in->buf->last += bytes;
    src->pos += bytes;
    *rest -= bytes;

    return NGX_AGAIN;
}
