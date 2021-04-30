#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_http_wasm_headers.h>
#include <ngx_http_wasm_util.h>


ngx_int_t
ngx_http_wasm_set_header(ngx_http_request_t *r,
    ngx_http_wasm_headers_type_e htype, ngx_http_wasm_header_t *handlers,
    ngx_str_t key, ngx_str_t value, ngx_uint_t mode)
{
    size_t                       i;
    ngx_http_wasm_header_val_t   hv;

    if (ngx_http_copy_escaped(&key, r->pool,
                              NGX_HTTP_WASM_ESCAPE_HEADER_NAME) == NULL
        ||
        ngx_http_copy_escaped(&value, r->pool,
                              NGX_HTTP_WASM_ESCAPE_HEADER_VALUE) == NULL)
    {
        return NGX_ERROR;
    }

    ngx_log_debug3(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "wasm setting %s header: \"%V: %V\"",
                   htype == NGX_HTTP_WASM_HEADERS_REQUEST ? "request" : "response",
                   &key, &value);

    hv.hash = ngx_hash_key_lc(key.data, key.len);
    hv.key = key;
    hv.mode = mode;
    hv.offset = 0;
    hv.handler = NULL;
    hv.list = NULL;

    for (i = 0; handlers[i].name.len; i++) {
        if (hv.key.len != handlers[i].name.len
            || ngx_strncasecmp(hv.key.data, handlers[i].name.data,
                               handlers[i].name.len) != 0)
        {
            continue;
        }

        hv.offset = handlers[i].offset;
        hv.handler = handlers[i].handler;
        break;
    }

    if (handlers[i].name.len == 0 && handlers[i].handler) {
        /* ngx_http_set_header */
        hv.offset = handlers[i].offset;
        hv.handler = handlers[i].handler;
    }

    switch (htype) {
    case NGX_HTTP_WASM_HEADERS_REQUEST:
        hv.list = &r->headers_in.headers;
        break;
    case NGX_HTTP_WASM_HEADERS_RESPONSE:
        hv.list = &r->headers_out.headers;
        break;
    }

    ngx_wasm_assert(hv.handler);
    ngx_wasm_assert(hv.list);

    return hv.handler(r, &hv, &value);
}


ngx_int_t
ngx_http_wasm_set_header_helper(ngx_http_wasm_header_val_t *hv,
    ngx_str_t *value, ngx_table_elt_t **out)
{
    unsigned          found = 0;
    size_t            i;
    ngx_table_elt_t  *h;
    ngx_list_t       *list = hv->list;
    ngx_list_part_t  *part;

    if (hv->mode == NGX_HTTP_WASM_HEADERS_APPEND) {
        goto new_header;
    }

again:

    dd("searching '%.*s' (found: %u)",
       (int) hv->key.len, hv->key.data, found);

    part = &list->part;
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
            && h[i].key.len == hv->key.len
            && ngx_strncasecmp(hv->key.data, h[i].key.data, h[i].key.len) == 0)
        {
            if (value->len == 0 || found) {
                dd("clearing '%.*s: %.*s'",
                   (int) h[i].key.len, h[i].key.data,
                   (int) h[i].value.len, h[i].value.data);

                h[i].value.len = 0;
                h[i].hash = 0;

                if (out) {
                    *out = NULL;
                }

                found = 1;

                goto again;

            } else {
                h[i].value = *value;
                h[i].hash = hv->hash;

                dd("updating '%.*s: %.*s' to '%.*s: %.*s'",
                   (int) h[i].key.len, h[i].key.data,
                   (int) h[i].value.len, h[i].value.data,
                   (int) h[i].key.len, h[i].key.data,
                   (int) value->len, value->data);
            }

            if (out) {
                *out = &h[i];
            }

            found = 1;
        }
    }

new_header:

    if (found
        || !value->len
        || hv->mode == NGX_HTTP_WASM_HEADERS_REPLACE_IF_SET)
    {
        return NGX_OK;
    }

    dd("adding '%.*s: %.*s'",
       (int) hv->key.len, hv->key.data,
       (int) value->len, value->data);

    h = ngx_list_push(list);
    if (h == NULL) {
        return NGX_ERROR;
    }

    h->hash = value->len ? hv->hash : 0;
    h->key = hv->key;
    h->value = *value;
    h->lowcase_key = ngx_pnalloc(list->pool, h->key.len);
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
    return ngx_http_wasm_set_header_helper(hv, value, NULL);
}


ngx_int_t
ngx_http_wasm_set_builtin_header_handler(ngx_http_request_t *r,
    ngx_http_wasm_header_val_t *hv, ngx_str_t *value)
{
    ngx_list_t       *headers = hv->list;
    ngx_table_elt_t  *h, **old;

    old = (ngx_table_elt_t **) ((char *) headers + hv->offset);
    if (*old == NULL) {
        return ngx_http_wasm_set_header_helper(hv, value, old);
    }

    h = *old;

    if (value->len == 0) {
        dd("clearing existing '%.*s' builtin header",
           (int) hv->key.len, hv->key.data);

        h->value = *value;
        hv->mode = NGX_HTTP_WASM_HEADERS_SET;

        return ngx_http_wasm_set_header_helper(hv, value, old);
    }

    dd("updating existing '%.*s: %.*s' builtin header to '%.*s: %.*s'",
       (int) h->key.len, h->key.data, (int) h->value.len, h->value.data,
       (int) h->key.len, h->key.data, (int) value->len, value->data);

    h->hash = hv->hash;
    h->key = hv->key;
    h->value = *value;

    return NGX_OK;
}
