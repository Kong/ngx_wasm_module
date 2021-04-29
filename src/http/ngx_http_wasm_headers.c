#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_http_wasm_headers.h>
#include <ngx_http_wasm_util.h>


static ngx_int_t ngx_http_wasm_remove_header_line(ngx_list_t *l,
    ngx_list_part_t *cur, ngx_uint_t i);


ngx_int_t
ngx_http_wasm_set_header(ngx_http_request_t *r, ngx_http_wasm_header_e htype,
    ngx_http_wasm_header_t *handlers, ngx_str_t key, ngx_str_t value,
    unsigned override)
{
    size_t                         i;
    ngx_http_wasm_header_val_t     hv;

    if (ngx_http_copy_escaped(&key, r->pool,
                              NGX_HTTP_WASM_ESCAPE_HEADER_NAME) == NULL
        ||
        ngx_http_copy_escaped(&value, r->pool,
                              NGX_HTTP_WASM_ESCAPE_HEADER_VALUE) == NULL)
    {
        return NGX_ERROR;
    }

    dd("wasm set %s header: '%.*s: %.*s'",
       htype == NGX_HTTP_WASM_HEADER_REQUEST ? "request" : "response",
       (int) key.len, key.data, (int) value.len, value.data);

    hv.hash = ngx_hash_key_lc(key.data, key.len);
    hv.key = key;
    hv.override = override;
    hv.roffset = 0;
    hv.offset = 0;
    hv.handler = NULL;

    for (i = 0; handlers[i].name.len; i++) {
        if (hv.key.len != handlers[i].name.len
            || ngx_strncasecmp(hv.key.data, handlers[i].name.data,
                               handlers[i].name.len) != 0)
        {
            continue;
        }

        hv.roffset = handlers[i].roffset;
        hv.offset = handlers[i].offset;
        hv.handler = handlers[i].handler;
        break;
    }

    if (handlers[i].name.len == 0 && handlers[i].handler) {
        /* ngx_http_set_header */
        hv.roffset = handlers[i].roffset;
        hv.offset = handlers[i].offset;
        hv.handler = handlers[i].handler;
    }

    ngx_wasm_assert(hv.handler);

    return hv.handler(r, &hv, &value);
}


ngx_int_t
ngx_http_wasm_set_header_helper(ngx_list_t *headers,
    ngx_http_wasm_header_val_t *hv, ngx_str_t *value, ngx_table_elt_t **out)
{
    unsigned          found = 0;
    size_t            i;
    ngx_uint_t        rc;
    ngx_table_elt_t  *h;
    ngx_list_part_t  *part;

    if (!hv->override) {
        goto new_header;
    }

again:

    dd("searching '%.*s' (found: %u)",
       (int) hv->key.len, hv->key.data, found);

    part = &headers->part;
    h = part->elts;

    for (i = 0; /* void */; i++) {

        if (i >= part->nelts) {
            if (part->next == NULL) {
                break;
            }

            part = part->next;
            h = part->elts;
            i = 0;
        }

        if (h[i].hash == hv->hash
            || (h[i].key.len == hv->key.len
                && ngx_strncasecmp(hv->key.data, h[i].key.data, h[i].key.len)
                   == 0))
        {
            if (value->len == 0 || found) {
                dd("clearing '%.*s: %.*s'",
                   (int) h[i].key.len, h[i].key.data,
                   (int) h[i].value.len, h[i].value.data);

                h[i].value.len = 0;
                h[i].hash = 0;

                rc = ngx_http_wasm_remove_header_line(headers, part, i);
                if (rc != NGX_OK) {
                    return NGX_ERROR;
                }

                if (out) {
                    *out = NULL;
                }

                ngx_wasm_assert(!(headers->part.next == NULL
                                  && headers->last != &headers->part));

                goto again;

            } else {
                dd("updating '%.*s: %.*s' to '%.*s: %.*s'",
                   (int) h[i].key.len, h[i].key.data,
                   (int) h[i].value.len, h[i].value.data,
                   (int) h[i].key.len, h[i].key.data,
                   (int) value->len, value->data);

                h[i].value = *value;
                h[i].hash = hv->hash;
            }

            if (out) {
                *out = &h[i];
            }

            found = 1;
        }
    }

new_header:

    if (found || hv->no_create || !value->len) {
        return NGX_OK;
    }

    dd("adding '%.*s: %.*s'",
       (int) hv->key.len, hv->key.data,
       (int) value->len, value->data);

    h = ngx_list_push(headers);
    if (h == NULL) {
        return NGX_ERROR;
    }

    h->hash = value->len ? hv->hash : 0;
    h->key = hv->key;
    h->value = *value;
    h->lowcase_key = ngx_pnalloc(headers->pool, h->key.len);
    if (h->lowcase_key == NULL) {
        return NGX_ERROR;
    }

    ngx_strlow(h->lowcase_key, h->key.data, h->key.len);

    if (out) {
        *out = h;
    }

    return NGX_OK;
}


/* handlers */


ngx_int_t
ngx_http_wasm_set_header_handler(ngx_http_request_t *r,
    ngx_http_wasm_header_val_t *hv, ngx_str_t *value)
{
    return ngx_http_wasm_set_header_helper(&r->headers_in.headers, hv,
                                           value, NULL);
}


static ngx_int_t
ngx_http_wasm_remove_header_line(ngx_list_t *l, ngx_list_part_t *cur,
    ngx_uint_t i)
{
    ngx_table_elt_t  *data;
    ngx_list_part_t  *new, *part;

    data = cur->elts;

    if (i == 0) {
        cur->elts = (char *) cur->elts + l->size;
        cur->nelts--;

        if (cur == l->last) {
            if (cur->nelts == 0) {
                part = &l->part;

                if (part == cur) {
                    cur->elts = (char *) cur->elts - l->size;
                    /* do nothing */

                } else {
                    while (part->next != cur) {
                        if (part->next == NULL) {
                            return NGX_ERROR;
                        }

                        part = part->next;
                    }

                    part->next = NULL;
                    l->last = part;
                    l->nalloc = part->nelts;
                }

            } else {
                l->nalloc--;
            }

            return NGX_OK;
        }

        if (cur->nelts == 0) {
            part = &l->part;

            if (part == cur) {
                ngx_wasm_assert(cur->next != NULL);

                if (l->last == cur->next) {
                    l->part = *(cur->next);
                    l->last = part;
                    l->nalloc = part->nelts;

                } else {
                    l->part = *(cur->next);
                }

            } else {
                while (part->next != cur) {
                    if (part->next == NULL) {
                        return NGX_ERROR;
                    }

                    part = part->next;
                }

                part->next = cur->next;
            }
        }

        return NGX_OK;
    }

    if (i == cur->nelts - 1) {
        cur->nelts--;

        if (cur == l->last) {
            l->nalloc--;
        }

        return NGX_OK;
    }

    new = ngx_palloc(l->pool, sizeof(ngx_list_part_t));
    if (new == NULL) {
        return NGX_ERROR;
    }

    new->elts = &data[i + 1];
    new->nelts = cur->nelts - i - 1;
    new->next = cur->next;

    cur->nelts = i;
    cur->next = new;

    if (cur == l->last) {
        l->last = new;
        l->nalloc = new->nelts;
    }

    return NGX_OK;
}


ngx_int_t
ngx_http_wasm_set_builtin_header_handler(ngx_http_request_t *r,
    ngx_http_wasm_header_val_t *hv, ngx_str_t *value)
{
    ngx_list_t       *headers;
    ngx_table_elt_t  *h, **old;

    headers = (ngx_list_t *) ((char *) r + hv->roffset);

    old = (ngx_table_elt_t **) ((char *) headers + hv->offset);
    if (*old == NULL) {
        return ngx_http_wasm_set_header_helper(headers, hv, value, old);
    }

    h = *old;

    if (value->len == 0) {
        dd("clearing existing '%.*s' builtin request header",
           (int) hv->key.len, hv->key.data);

        h->value = *value;
        hv->override = 1;

        return ngx_http_wasm_set_header_helper(headers, hv, value, old);
    }

    /* set */

    dd("updating existing '%.*s: %.*s' builtin request header "
       "to '%.*s: %.*s'",
       (int) h->key.len, h->key.data,
       (int) h->value.len, h->value.data,
       (int) h->key.len, h->key.data,
       (int) value->len, value->data);

    h->hash = hv->hash;
    h->key = hv->key;
    h->value = *value;

    return NGX_OK;
}
