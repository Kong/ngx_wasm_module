#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_wasm.h>


#if 0
static void
ngx_wasm_chain_log_debug(ngx_log_t *log, ngx_chain_t *in, char *fmt)
{
#if (NGX_DEBUG)
    size_t        len;
    ngx_chain_t  *cl;
    ngx_buf_t    *buf;
    ngx_str_t     s;

    cl = in;

    while (cl) {
        buf = cl->buf;
        len = buf->last - buf->pos;

        s.len = len;
        s.data = buf->pos;

        ngx_log_debug7(NGX_LOG_DEBUG_WASM, log, 0,
                       "%s: \"%V\" (buf: %p, len: %d, last_buf: %d,"
                       " last_in_chain: %d, flush: %d)",
                       fmt, &s, buf, len, buf->last_buf,
                       buf->last_in_chain, buf->flush);

        cl = cl->next;
    }
#endif
}
#endif


size_t
ngx_wasm_chain_len(ngx_chain_t *in, unsigned *eof)
{
    size_t        len = 0;
    ngx_buf_t    *buf;
    ngx_chain_t  *cl;

    for (cl = in; cl; cl = cl->next) {
        buf = cl->buf;
        len += ngx_buf_size(buf);

        if (buf->last_buf || buf->last_in_chain) {
            if (eof) {
                *eof = 1;
            }

            break;
        }
    }

    return len;
}


ngx_uint_t
ngx_wasm_chain_clear(ngx_chain_t *in, size_t offset, unsigned *eof,
    unsigned *flush)
{
    size_t        pos = 0, len, n;
    ngx_uint_t    fill = 0;
    ngx_buf_t    *buf;
    ngx_chain_t  *cl;

    for (cl = in; cl; cl = cl->next) {
        buf = cl->buf;
        len = buf->last - buf->pos;

        if (eof && (buf->last_buf || buf->last_in_chain)) {
            *eof = 1;
        }

        if (flush && buf->flush) {
            *flush = 1;
        }

        if (offset && pos + len > offset) {
            /* reaching start offset */
            n = len - ((pos + len) - offset);  /* bytes left until offset */
            pos += n;
            buf->last = buf->pos + n;  /* partially consume buffer */

            ngx_wasm_assert(pos == offset);

        } else if (pos == offset) {
            /* past start offset, consume buffer */
            buf->pos = buf->last;

        } else {
            /* prior start offset, preserve buffer */
            pos += len;
        }

        buf->last_buf = 0;
        buf->last_in_chain = 0;
    }

    if (pos < offset) {
        fill = offset - pos;
        pos += fill;
    }

    ngx_wasm_assert(pos == offset);

    return fill;
}


ngx_chain_t *
ngx_wasm_chain_get_free_buf(ngx_pool_t *p, ngx_chain_t **free,
    size_t len, ngx_buf_tag_t tag, unsigned reuse)
{
    ngx_buf_t    *b;
    ngx_chain_t  *cl;
    u_char       *start, *end;

    if (reuse && *free) {
        cl = *free;
        *free = cl->next;
        cl->next = NULL;

        b = cl->buf;
        start = b->start;
        end = b->end;

        if (start && (size_t) (end - start) >= len) {
            ngx_log_debug4(NGX_LOG_DEBUG_WASM, ngx_cycle->log, 0,
                           "wasm reuse free buf memory %O >= %uz, cl:%p, p:%p",
                           (off_t) (end - start), len, cl, start);

            ngx_memzero(b, sizeof(ngx_buf_t));

            b->start = start;
            b->pos = start;
            b->last = start;
            b->end = end;
            b->tag = tag;

            if (len) {
                b->temporary = 1;
            }

            return cl;
        }

        ngx_log_debug4(NGX_LOG_DEBUG_WASM, ngx_cycle->log, 0,
                       "wasm reuse free buf chain, but reallocate memory "
                       "because %uz >= %O, cl:%p, p:%p", len,
                       (off_t) (b->end - b->start), cl, b->start);

        if (ngx_buf_in_memory(b) && b->start) {
            ngx_pfree(p, b->start);
        }

        ngx_memzero(b, sizeof(ngx_buf_t));

        if (len == 0) {
            return cl;
        }

        b->start = ngx_palloc(p, len);
        if (b->start == NULL) {
            return NULL;
        }

        b->end = b->start + len;
        b->pos = b->start;
        b->last = b->start;
        b->tag = tag;
        b->temporary = 1;

        return cl;
    }

    cl = ngx_alloc_chain_link(p);
    if (cl == NULL) {
        return NULL;
    }

    ngx_log_debug2(NGX_LOG_DEBUG_WASM, ngx_cycle->log, 0,
                   "wasm allocate new chainlink and new buf of size %uz, cl:%p",
                   len, cl);

    cl->buf = len ? ngx_create_temp_buf(p, len) : ngx_calloc_buf(p);
    if (cl->buf == NULL) {
        return NULL;
    }

    cl->buf->tag = tag;
    cl->next = NULL;

    return cl;
}


ngx_int_t
ngx_wasm_chain_append(ngx_pool_t *pool, ngx_chain_t **in, size_t at,
    ngx_str_t *str, ngx_chain_t **free, ngx_buf_tag_t tag)
{
    unsigned      eof = 0, flush = 0;
    ngx_uint_t    fill, rest;
    ngx_buf_t    *buf;
    ngx_chain_t  *cl, *nl, *ll = NULL;

    fill = ngx_wasm_chain_clear(*in, at, &eof, &flush);
    rest = str->len + fill;

    /* get tail */

    cl = *in;

    while (cl) {
        buf = cl->buf;

        if (ngx_buf_size(buf)) {
            ll = cl;
            cl = cl->next;
            continue;
        }

        /* zero size buf */

        if (buf->tag != tag) {
            if (ll) {
                //ngx_wasm_assert(ll->next == cl);
                ll->next = cl->next;
            }

            ngx_free_chain(pool, cl);
            cl = ll ? ll->next : NULL;
            continue;
        }

        buf->pos = buf->start;
        buf->last = buf->start;

        nl = cl;
        cl = cl->next;
        nl->next = *free;
        if (*free) {
            *free = nl;
        }
    }

    nl = ngx_wasm_chain_get_free_buf(pool, free, rest, tag, 1);
    if (nl == NULL) {
        return NGX_ERROR;
    }

    buf = nl->buf;

    if (fill) {
        /* spillover fill */
        ngx_memset(buf->last, ' ', fill);
        buf->last += fill;
        rest -= fill;
    }

    /* write */

    ngx_wasm_assert(rest == str->len);

    buf->last = ngx_cpymem(buf->last, str->data, rest);

    if (flush) {
        buf->flush = 1;
    }

    if (eof) {
        buf->last_buf = 1;
        buf->last_in_chain = 1;
    }

    if (ll) {
        ll->next = nl;

    } else {
        *in = nl;
    }

    return NGX_OK;
}


ngx_uint_t
ngx_wasm_list_nelts(ngx_list_t *list)
{
    ngx_uint_t        i, c = 0;
    ngx_list_part_t  *part;
    ngx_table_elt_t  *h;

    part = &list->part;

    while (part) {
        h = part->elts;

        for (i = 0; i < part->nelts; i++) {
            if (h[i].hash)
                c++;
        }

        part = part->next;
    }

    return c;
}


ngx_int_t
ngx_wasm_bytes_from_path(wasm_byte_vec_t *out, u_char *path, ngx_log_t *log)
{
    ssize_t      n, fsize;
    u_char      *file_bytes = NULL;
    ngx_fd_t     fd;
    ngx_file_t   file;
    ngx_int_t    rc = NGX_ERROR;

    fd = ngx_open_file(path, NGX_FILE_RDONLY, NGX_FILE_OPEN, 0);
    if (fd == NGX_INVALID_FILE) {
        ngx_wasm_log_error(NGX_LOG_EMERG, log, ngx_errno,
                           ngx_open_file_n " \"%s\" failed",
                           path);
        return NGX_ERROR;
    }

    ngx_memzero(&file, sizeof(ngx_file_t));

    file.fd = fd;
    file.log = log;
    file.name.len = ngx_strlen(path);;
    file.name.data = path;

    if (ngx_fd_info(fd, &file.info) == NGX_FILE_ERROR) {
        ngx_wasm_log_error(NGX_LOG_EMERG, log, ngx_errno,
                           ngx_fd_info_n " \"%V\" failed", &file.name);
        goto close;
    }

    fsize = ngx_file_size(&file.info);

    file_bytes = ngx_alloc(fsize, log);
    if (file_bytes == NULL) {
        ngx_wasm_log_error(NGX_LOG_EMERG, log, 0,
                           "failed to allocate file_bytes for \"%V\"",
                           path);
        goto close;
    }

    n = ngx_read_file(&file, file_bytes, fsize, 0);
    if (n == NGX_ERROR) {
        ngx_wasm_log_error(NGX_LOG_EMERG, log, ngx_errno,
                           ngx_read_file_n " \"%V\" failed",
                           &file.name);
        goto close;
    }

    if (n != fsize) {
        ngx_wasm_log_error(NGX_LOG_EMERG, log, 0,
                           ngx_read_file_n " \"%V\" returned only "
                           "%z file_bytes instead of %uiz", &file.name,
                           n, fsize);
        goto close;
    }

    wasm_byte_vec_new(out, fsize, (const char *) file_bytes);

    rc = NGX_OK;

close:

    if (ngx_close_file(fd) == NGX_FILE_ERROR) {
        ngx_wasm_log_error(NGX_LOG_ERR, log, ngx_errno,
                           ngx_close_file_n " \"%V\" failed", &file.name);
    }

    if (file_bytes) {
        ngx_free(file_bytes);
    }

    return rc;
}


void
ngx_wasm_log_error(ngx_uint_t level, ngx_log_t *log, ngx_err_t err,
    const char *fmt, ...)
{
    va_list   args;
    u_char   *p, errstr[NGX_MAX_ERROR_STR];

    va_start(args, fmt);
    p = ngx_vsnprintf(errstr, NGX_MAX_ERROR_STR, fmt, args);
    va_end(args);

    ngx_log_error_core(level, log, err, "[wasm] %*s", p - errstr, errstr);
}


ngx_str_t *
ngx_wasm_get_list_elem(ngx_list_t *map, u_char *key, size_t key_len)
{
    size_t            i;
    ngx_table_elt_t  *elt;
    ngx_list_part_t  *part;

    part = &map->part;
    elt = part->elts;

    for (i = 0; /* void */; i++) {

        if (i >= part->nelts) {
            if (part->next == NULL) {
                break;
            }

            part = part->next;
            elt = part->elts;
            i = 0;
        }

#if 0
        dd("key: %.*s, value: %.*s",
           (int) elt[i].key.len, elt[i].key.data,
           (int) elt[i].value.len, elt[i].value.data);
#endif

        if (key_len == elt[i].key.len
            && ngx_strncasecmp(elt[i].key.data, key, key_len) == 0)
        {
            return &elt[i].value;
        }
    }

    return NULL;
}


#if 0
ngx_int_t
ngx_wasm_add_list_elem(ngx_pool_t *pool, ngx_list_t *map, u_char *key,
    size_t key_len, u_char *value, size_t val_len)
{
    ngx_table_elt_t  *h;

    h = ngx_list_push(map);
    if (h == NULL) {
        return NGX_ERROR;
    }

    h->hash = ngx_hash_key(key, key_len);
    h->key.len = key_len;
    h->key.data = ngx_pnalloc(pool, h->key.len);
    if (h->key.data == NULL) {
        return NGX_ERROR;
    }

    ngx_memcpy(h->key.data, key, key_len);

    h->value.len = val_len;
    h->value.data = ngx_pnalloc(pool, h->value.len);
    if (h->value.data == NULL) {
        return NGX_ERROR;
    }

    ngx_memcpy(h->value.data, value, val_len);

    h->lowcase_key = ngx_pnalloc(pool, h->key.len);
    if (h->lowcase_key == NULL) {
        return NGX_ERROR;
    }

    ngx_strlow(h->lowcase_key, h->key.data, h->key.len);

    return NGX_OK;
}
#endif


#if 0
ngx_connection_t *
ngx_wasm_connection_create(ngx_pool_t *pool)
{
    ngx_log_t         *log;
    ngx_connection_t  *c, *orig = NULL;

    if (ngx_cycle->files) {
        orig = ngx_cycle->files[0];
    }

    c = ngx_get_connection(0, ngx_cycle->log);

    if (orig) {
        ngx_cycle->files[0] = orig;
    }

    if (c == NULL) {
        return NULL;
    }

    c->fd = NGX_WASM_BAD_FD;
    c->number = ngx_atomic_fetch_add(ngx_connection_counter, 1);
    c->pool = pool;

    log = ngx_pcalloc(c->pool, sizeof(ngx_log_t));
    if (log == NULL) {
        goto failed;
    }

    c->log = log;
    c->log_error = NGX_ERROR_INFO;
    c->log->connection = c->number;
    c->log->action = NULL;
    c->log->data = NULL;
    c->error = 1;

    return c;

failed:

    ngx_wasm_connection_destroy(c);
    return NULL;
}


void
ngx_wasm_connection_destroy(ngx_connection_t *c)
{
    ngx_connection_t  *orig = NULL;

    if (c->read->timer_set) {
        ngx_del_timer(c->read);
    }

    if (c->write->timer_set) {
        ngx_del_timer(c->write);
    }

    c->read->closed = 1;
    c->write->closed = 1;
    c->destroyed = 1;

    if (ngx_cycle->files) {
        orig = ngx_cycle->files[0];
    }

    c->fd = 0;
    ngx_free_connection(c);

    if (orig) {
        ngx_cycle->files[0] = orig;
    }
}
#endif
