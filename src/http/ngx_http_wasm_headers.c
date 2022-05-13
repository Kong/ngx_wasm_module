#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_http_wasm_headers.h>
#include <ngx_http_wasm_util.h>


static char *
ngx_http_wasm_htype_name(ngx_http_wasm_headers_type_e htype)
{
    switch (htype) {

    case NGX_HTTP_WASM_HEADERS_REQUEST:
        return (char *) "request";

    case NGX_HTTP_WASM_HEADERS_RESPONSE:
        return (char *) "response";

    default:
        return (char *) "[unknown]";

    }
}


static ngx_list_t *
ngx_http_wasm_htype_list(ngx_http_request_t *r,
    ngx_http_wasm_headers_type_e htype)
{
    switch (htype) {

    case NGX_HTTP_WASM_HEADERS_REQUEST:
        return &r->headers_in.headers;

    case NGX_HTTP_WASM_HEADERS_RESPONSE:
        return &r->headers_out.headers;

    default:
        ngx_wasm_assert(0);
        return NULL;

    }
}


ngx_int_t
ngx_http_wasm_set_header(ngx_http_request_t *r,
    ngx_http_wasm_headers_type_e htype, ngx_http_wasm_header_handler_t *handlers,
    ngx_str_t *key, ngx_str_t *value, ngx_http_wasm_headers_set_mode_e mode)
{
    size_t                           i;
    ngx_http_wasm_header_set_ctx_t   hv;
    static ngx_str_t                 null_str = ngx_null_string;

    if (ngx_http_copy_escaped(key, r->pool,
                              NGX_HTTP_WASM_ESCAPE_HEADER_NAME)
        == NULL)
    {
        return NGX_ERROR;
    }

    if (value && ngx_http_copy_escaped(value, r->pool,
                                       NGX_HTTP_WASM_ESCAPE_HEADER_VALUE)
        == NULL)
    {
        return NGX_ERROR;
    }

    ngx_log_debug3(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "wasm setting %s header: \"%V: %V\"",
                   ngx_http_wasm_htype_name(htype),
                   key, value ? value : &null_str);

    hv.r = r;
    hv.handler = NULL;
    hv.key = NULL;
    hv.value = NULL;
    hv.list = ngx_http_wasm_htype_list(r, htype);;
    hv.htype = htype;
    hv.mode = mode;
    hv.hash = ngx_hash_key_lc(key->data, key->len);

    for (i = 0; handlers[i].name.len; i++) {
        if (key->len != handlers[i].name.len
            || ngx_strncasecmp(key->data, handlers[i].name.data,
                               handlers[i].name.len) != 0)
        {
            continue;
        }

        hv.handler = &handlers[i];
        hv.key = &handlers[i].name;
        break;
    }

    if (handlers[i].name.len == 0 && handlers[i].handler_) {
        /* ngx_http_set_header */
        hv.handler = &handlers[i];
        hv.key = key;
    }

    switch (mode) {
    case NGX_HTTP_WASM_HEADERS_APPEND:
    case NGX_HTTP_WASM_HEADERS_SET:
        hv.value = value;
        break;
    case NGX_HTTP_WASM_HEADERS_REMOVE:
        hv.value = &null_str;
        break;
    }

    ngx_wasm_assert(hv.key);
    ngx_wasm_assert(hv.value);
    ngx_wasm_assert(hv.list);
    ngx_wasm_assert(hv.handler);

    return hv.handler->handler_(&hv);
}


ngx_int_t
ngx_http_wasm_set_header_helper(ngx_http_wasm_header_set_ctx_t *hv,
    ngx_table_elt_t **out)
{
    size_t            i;
    ngx_table_elt_t  *h;
    ngx_list_part_t  *part;
    ngx_list_t       *list = hv->list;
    ngx_str_t        *key = hv->key;
    ngx_str_t        *value = hv->value;
    unsigned          found = 0;

    if (hv->mode == NGX_HTTP_WASM_HEADERS_APPEND) {
        goto new_header;
    }

again:

    dd("searching '%.*s' (found: %u)",
       (int) key->len, key->data, found);

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
            && h[i].key.len == key->len
            && ngx_strncasecmp(key->data, h[i].key.data, h[i].key.len) == 0)
        {
            if (hv->mode == NGX_HTTP_WASM_HEADERS_REMOVE || found) {
                dd("clearing '%.*s: %.*s'",
                   (int) h[i].key.len, h[i].key.data,
                   (int) h[i].value.len, h[i].value.data);

                h[i].hash = 0;

                if (out) {
                    *out = NULL;
                }

                found = 1;

                goto again;

            } else {
                ngx_wasm_assert(hv->mode == NGX_HTTP_WASM_HEADERS_SET);

                dd("updating '%.*s: %.*s' to '%.*s: %.*s'",
                   (int) h[i].key.len, h[i].key.data,
                   (int) h[i].value.len, h[i].value.data,
                   (int) key->len, key->data,
                   (int) value->len, value->data);

                h[i].key = *key;
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

    if (found
        || hv->mode == NGX_HTTP_WASM_HEADERS_REMOVE)
    {
        return NGX_OK;
    }

    dd("adding '%.*s: %.*s'",
       (int) key->len, key->data,
       (int) value->len, value->data);

    h = ngx_list_push(list);
    if (h == NULL) {
        return NGX_ERROR;
    }

    h->hash = hv->hash;
    h->key = *key;
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


ngx_int_t
ngx_http_wasm_clear_headers_helper(ngx_http_request_t *r,
    ngx_http_wasm_headers_type_e htype, ngx_http_wasm_header_handler_t *handlers)
{
    size_t            i;
    ngx_int_t         rc;
    ngx_table_elt_t  *h;
    ngx_list_part_t  *part;
    ngx_list_t       *list = ngx_http_wasm_htype_list(r, htype);

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

        if (h[i].hash != 0) {
            rc = ngx_http_wasm_set_header(r, htype, handlers,
                                          &h[i].key, NULL,
                                          NGX_HTTP_WASM_HEADERS_REMOVE);
            if (rc != NGX_OK) {
                return NGX_ERROR;
            }
        }
    }

    return NGX_OK;
}


/* handlers */


ngx_int_t
ngx_http_wasm_set_header_handler(ngx_http_wasm_header_set_ctx_t *hv)
{
    return ngx_http_wasm_set_header_helper(hv, NULL);
}


ngx_int_t
ngx_http_wasm_set_builtin_header_handler(ngx_http_wasm_header_set_ctx_t *hv)
{
    ngx_http_request_t  *r = hv->r;
    ngx_str_t           *key = hv->key;
    ngx_str_t           *value = hv->value;
    ngx_table_elt_t     *h, **builtin;

    builtin = (ngx_table_elt_t **) ((char *) hv->list + hv->handler->offset);
    if (*builtin == NULL
        && hv->mode != NGX_HTTP_WASM_HEADERS_REMOVE)
    {
        dd("creating '%.*s: %.*s' builtin header",
           (int) key->len, key->data, (int) value->len, value->data);

        return ngx_http_wasm_set_header_helper(hv, builtin);
    }

    h = *builtin;

    if (hv->mode == NGX_HTTP_WASM_HEADERS_REMOVE) {
        if (h) {
            dd("clearing existing '%.*s' builtin header",
               (int) key->len, key->data);

            h->hash = 0;
        }

       return NGX_OK;
    }

    if (h && hv->mode == NGX_HTTP_WASM_HEADERS_APPEND) {
        ngx_wasm_log_error(NGX_LOG_ERR, r->connection->log, 0,
                           "cannot add new \"%V\" builtin %s header",
                           key, ngx_http_wasm_htype_name(hv->htype));
        return NGX_DECLINED;
    }

    ngx_wasm_assert(hv->mode == NGX_HTTP_WASM_HEADERS_SET);

    dd("updating existing '%.*s: %.*s' builtin header to '%.*s: %.*s'",
       (int) h->key.len, h->key.data, (int) h->value.len, h->value.data,
       (int) key->len, key->data, (int) value->len, value->data);

    h->hash = hv->hash;
    h->key = *key;
    h->value = *value;

    return NGX_OK;
}
