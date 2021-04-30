#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_http_wasm.h>


static ngx_int_t ngx_http_set_header(ngx_http_request_t *r,
    ngx_http_wasm_header_val_t *hv, ngx_str_t *value);
static ngx_int_t ngx_http_wasm_set_header_helper_response(ngx_http_request_t *r,
    ngx_http_wasm_header_val_t *hv, ngx_str_t *value,
    ngx_table_elt_t **out, unsigned no_create);
static ngx_int_t ngx_http_set_builtin_header(ngx_http_request_t *r,
    ngx_http_wasm_header_val_t *hv, ngx_str_t *value);
static ngx_int_t ngx_http_set_builtin_multi_header(ngx_http_request_t *r,
    ngx_http_wasm_header_val_t *hv, ngx_str_t *value);
static ngx_int_t ngx_http_set_last_modified_header(ngx_http_request_t *r,
    ngx_http_wasm_header_val_t *hv, ngx_str_t *value);
static ngx_int_t ngx_http_set_content_length_header(ngx_http_request_t *r,
    ngx_http_wasm_header_val_t *hv, ngx_str_t *value);
static ngx_int_t ngx_http_set_content_type_header(ngx_http_request_t *r,
    ngx_http_wasm_header_val_t *hv, ngx_str_t *value);
static ngx_inline ngx_int_t  ngx_http_clear_builtin_header(
    ngx_http_request_t *r, ngx_http_wasm_header_val_t *hv);
static ngx_int_t ngx_http_set_location_header(ngx_http_request_t *r,
    ngx_http_wasm_header_val_t *hv, ngx_str_t *value);


static ngx_http_wasm_header_t  ngx_http_wasm_headers[] = {

    { ngx_string("Server"),
                 offsetof(ngx_http_request_t, headers_out),
                 offsetof(ngx_http_headers_out_t, server),
                 ngx_http_set_builtin_header },

    { ngx_string("Date"),
                 offsetof(ngx_http_request_t, headers_out),
                 offsetof(ngx_http_headers_out_t, date),
                 ngx_http_set_builtin_header },

    { ngx_string("Content-Length"),
                 offsetof(ngx_http_request_t, headers_out),
                 offsetof(ngx_http_headers_out_t, content_length),
                 ngx_http_set_content_length_header },

    { ngx_string("Content-Encoding"),
                 offsetof(ngx_http_request_t, headers_out),
                 offsetof(ngx_http_headers_out_t, content_encoding),
                 ngx_http_set_builtin_header },

    { ngx_string("Location"),
                 offsetof(ngx_http_request_t, headers_out),
                 offsetof(ngx_http_headers_out_t, location),
                 ngx_http_set_location_header },

    { ngx_string("Refresh"),
                 offsetof(ngx_http_request_t, headers_out),
                 offsetof(ngx_http_headers_out_t, refresh),
                 ngx_http_set_builtin_header },

    { ngx_string("Last-Modified"),
                 offsetof(ngx_http_request_t, headers_out),
                 offsetof(ngx_http_headers_out_t, last_modified),
                 ngx_http_set_last_modified_header },

    { ngx_string("Content-Range"),
                 offsetof(ngx_http_request_t, headers_out),
                 offsetof(ngx_http_headers_out_t, content_range),
                 ngx_http_set_builtin_header },

    { ngx_string("Accept-Ranges"),
                 offsetof(ngx_http_request_t, headers_out),
                 offsetof(ngx_http_headers_out_t, accept_ranges),
                 ngx_http_set_builtin_header },

    { ngx_string("WWW-Authenticate"),
                 offsetof(ngx_http_request_t, headers_out),
                 offsetof(ngx_http_headers_out_t, www_authenticate),
                 ngx_http_set_builtin_header },

    { ngx_string("Expires"),
                 offsetof(ngx_http_request_t, headers_out),
                 offsetof(ngx_http_headers_out_t, expires),
                 ngx_http_set_builtin_header },

    { ngx_string("E-Tag"),
                 offsetof(ngx_http_request_t, headers_out),
                 offsetof(ngx_http_headers_out_t, etag),
                 ngx_http_set_builtin_header },

    { ngx_string("ETag"),
                 offsetof(ngx_http_request_t, headers_out),
                 offsetof(ngx_http_headers_out_t, etag),
                 ngx_http_set_builtin_header },

    { ngx_string("Content-Type"),
                 offsetof(ngx_http_request_t, headers_out),
                 offsetof(ngx_http_headers_out_t, content_type),
                 ngx_http_set_content_type_header },

    { ngx_string("Cache-Control"),
                 offsetof(ngx_http_request_t, headers_out),
                 offsetof(ngx_http_headers_out_t, cache_control),
                 ngx_http_set_builtin_multi_header },

    { ngx_string("Link"),
                 offsetof(ngx_http_request_t, headers_out),
                 offsetof(ngx_http_headers_out_t, link),
                 ngx_http_set_builtin_multi_header },

    { ngx_null_string, 0, 0, ngx_http_set_header }
};


size_t
ngx_http_wasm_resp_headers_count(ngx_http_request_t *r)
{
    ngx_uint_t  count = 1; /* Connection */

    /* add default headers injected by ngx_http_header_filter */

    if (r->headers_out.server == NULL) {
        count++;
    }

    if (r->headers_out.date == NULL) {
        count++;
    }

    if (r->headers_out.content_type.len) {
        count++;
    }

    if (r->headers_out.content_length == NULL) {
        count++;

    } else if (r->headers_out.content_length_n == -1
               || r->expect_trailers)
    {
        /* Transfer-Encoding, injected by ngx_http_chunked_header_filter */
        count++;
    }

    if (r->headers_out.last_modified) {
        count++;
    }

    /* TODO: Location */

#if (NGX_HTTP_GZIP)
    if (r->gzip_vary) {
        /* Vary */
        count++;
    }
#endif

    count += ngx_wasm_list_nelts(&r->headers_out.headers);

    return count;
}


ngx_int_t
ngx_http_wasm_set_resp_header(ngx_http_request_t *r, ngx_str_t key,
    ngx_str_t value, unsigned override)
{
    size_t                         i;
    ngx_http_wasm_header_val_t     hv;
    const ngx_http_wasm_header_t  *handlers = ngx_http_wasm_headers;

    if (ngx_http_copy_escaped(&key, r->pool,
                              NGX_HTTP_WASM_ESCAPE_HEADER_NAME) == NULL
        ||
        ngx_http_copy_escaped(&value, r->pool,
                              NGX_HTTP_WASM_ESCAPE_HEADER_VALUE) == NULL)
    {
        return NGX_ERROR;
    }

    dd("wasm set response header: '%.*s: %.*s'",
       (int) key.len, key.data, (int) value.len, value.data);

    hv.hash = ngx_hash_key_lc(key.data, key.len);
    hv.key = key;
    hv.override = override;
    hv.offset = 0;
    hv.handler = NULL;

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

    ngx_wasm_assert(hv.handler);

    return hv.handler(r, &hv, &value);
}


/* handlers */


ngx_int_t
ngx_http_wasm_set_resp_content_length(ngx_http_request_t *r, off_t cl)
{
    u_char            *p;
    ngx_str_t          value;
    static ngx_str_t   key = ngx_string("Content-Length");

    if (cl == 0) {
        ngx_http_clear_content_length(r);
        return NGX_OK;
    }

    p = ngx_pnalloc(r->pool, NGX_OFF_T_LEN);
    if (p == NULL) {
        return NGX_ERROR;
    }

    value.data = p;
    value.len = ngx_sprintf(p, "%O", cl) - p;

    return ngx_http_wasm_set_resp_header(r, key, value, 0);
}


static ngx_int_t
ngx_http_set_header(ngx_http_request_t *r, ngx_http_wasm_header_val_t *hv,
    ngx_str_t *value)
{
    return ngx_http_wasm_set_header_helper_response(r, hv, value, NULL, 0);
}


static ngx_int_t
ngx_http_wasm_set_header_helper_response(ngx_http_request_t *r,
    ngx_http_wasm_header_val_t *hv, ngx_str_t *value, ngx_table_elt_t **out,
    unsigned no_create)
{
    unsigned          found = 0;
    size_t            i;
    ngx_table_elt_t  *h;
    ngx_list_part_t  *part;

    if (!hv->override) {
        /* append */
        goto new_header;
    }

    part = &r->headers_out.headers.part;
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

        if (h[i].hash != NGX_HTTP_WASM_HEADERS_HASH_SKIP
            && h[i].key.len == hv->key.len
            && ngx_strncasecmp(hv->key.data, h[i].key.data, h[i].key.len) == 0)
        {
            if (value->len == 0 || found) {
                dd("clearing response header line '%.*s: %.*s'",
                   (int) h[i].key.len, h[i].key.data,
                   (int) h[i].value.len, h[i].value.data);

                h[i].value.len = 0;
                h[i].hash = NGX_HTTP_WASM_HEADERS_HASH_SKIP;

            } else {
                dd("updating '%.*s' header line",
                   (int) hv->key.len, hv->key.data);

                h[i].value = *value;
                h[i].hash = hv->hash;
            }

            if (out) {
                *out = &h[i];
            }

            found = 1;
        }
    }

    if (found || (no_create && value->len == 0)) {
        return NGX_OK;
    }

new_header:

    dd("adding '%.*s' response header line",
       (int) hv->key.len, hv->key.data);

    h = ngx_list_push(&r->headers_out.headers);
    if (h == NULL) {
        return NGX_ERROR;
    }

    h->hash = value->len ? hv->hash : NGX_HTTP_WASM_HEADERS_HASH_SKIP;
    h->key = hv->key;
    h->value = *value;
    h->lowcase_key = ngx_pnalloc(r->pool, h->key.len);
    if (h->lowcase_key == NULL) {
        return NGX_ERROR;
    }

    ngx_strlow(h->lowcase_key, h->key.data, h->key.len);

    if (out) {
        *out = h;
    }

    return NGX_OK;
}


static ngx_int_t
ngx_http_set_builtin_header(ngx_http_request_t *r,
    ngx_http_wasm_header_val_t *hv, ngx_str_t *value)
{
    ngx_table_elt_t  *h, **old;

    ngx_wasm_assert(hv->offset);

    old = (ngx_table_elt_t **) ((char *) &r->headers_out + hv->offset);
    if (*old == NULL) {
        return ngx_http_wasm_set_header_helper_response(r, hv, value, old, 0);
    }

    h = *old;

    if (value->len == 0) {
        dd("clearing existing '%.*s' builtin response header",
           (int) hv->key.len, hv->key.data);

        h->hash = NGX_HTTP_WASM_HEADERS_HASH_SKIP;

        return NGX_OK;
    }

    dd("updating existing '%.*s' builtin response header",
       (int) hv->key.len, hv->key.data);

    h->hash = hv->hash;
    h->key = hv->key;
    h->value = *value;

    return NGX_OK;
}


static ngx_int_t
ngx_http_set_builtin_multi_header(ngx_http_request_t *r,
    ngx_http_wasm_header_val_t *hv, ngx_str_t *value)
{
    size_t            i;
    ngx_array_t      *headers;
    ngx_table_elt_t  *h, **ph;

    headers = (ngx_array_t *) ((char *) &r->headers_out + hv->offset);
    if (headers->elts == NULL) {
        if (ngx_array_init(headers, r->pool, 2, sizeof(ngx_table_elt_t *))
            != NGX_OK)
        {
            return NGX_ERROR;
        }
    }

    if (!hv->override) {
        /* reset, append */
        ph = headers->elts;

        for (i = 0; i < headers->nelts; i++) {
            if (!ph[i]->hash) {
                ph[i]->value = *value;
                ph[i]->hash = hv->hash;
                return NGX_OK;
            }
        }

        goto create;
    }

    /* override old values (if any) */

    if (headers->nelts) {
        ph = headers->elts;
        for (i = 1; i < headers->nelts; i++) {
            ph[i]->hash = 0;
            ph[i]->value.len = 0;
        }

        ph[0]->value = *value;

        if (value->len == 0) {
            ph[0]->hash = 0;

        } else {
            ph[0]->hash = hv->hash;
        }

        return NGX_OK;
    }

create:

    ph = ngx_array_push(headers);
    if (ph == NULL) {
        return NGX_ERROR;
    }

    h = ngx_list_push(&r->headers_out.headers);
    if (h == NULL) {
        return NGX_ERROR;
    }

    h->value = *value;

    if (value->len == 0) {
        h->hash = 0;

    } else {
        h->hash = hv->hash;
    }

    h->key = hv->key;
    *ph = h;

    return NGX_OK;
}


static ngx_int_t
ngx_http_set_location_header(ngx_http_request_t *r,
    ngx_http_wasm_header_val_t *hv, ngx_str_t *value)
{
    ngx_int_t         rc;
    ngx_table_elt_t  *h;

    rc = ngx_http_set_builtin_header(r, hv, value);
    if (rc != NGX_OK) {
        return rc;
    }

    h = r->headers_out.location;
    if (h && h->value.len && h->value.data[0] == '/') {
        /* avoid local redirects without host names in ngx_http_header_filter */
        r->headers_out.location = NULL;
    }

    return NGX_OK;
}


static ngx_int_t
ngx_http_set_last_modified_header(ngx_http_request_t *r,
    ngx_http_wasm_header_val_t *hv, ngx_str_t *value)
{
    if (value->len == 0) {
        /* clear */
        r->headers_out.last_modified_time = -1;
        return ngx_http_clear_builtin_header(r, hv);
    }

    r->headers_out.last_modified_time = ngx_http_parse_time(value->data,
                                                            value->len);

    return ngx_http_set_builtin_header(r, hv, value);
}


static ngx_int_t
ngx_http_set_content_length_header(ngx_http_request_t *r,
    ngx_http_wasm_header_val_t *hv, ngx_str_t *value)
{
    off_t   len;

    if (value->len == 0) {
        /* clear */
        r->headers_out.content_length_n = -1;
        return ngx_http_clear_builtin_header(r, hv);
    }

    len = ngx_atoof(value->data, value->len);
    if (len == NGX_ERROR) {
        return NGX_ERROR;
    }

    r->headers_out.content_length_n = len;

    return ngx_http_set_builtin_header(r, hv, value);
}


static ngx_int_t
ngx_http_set_content_type_header(ngx_http_request_t *r,
    ngx_http_wasm_header_val_t *hv, ngx_str_t *value)
{
    size_t   i;

    r->headers_out.content_type_len = value->len;

    for (i = 0; i < value->len; i++) {
        if (value->data[i] == ';') {
            r->headers_out.content_type_len = i;
            break;
        }
    }

    r->headers_out.content_type = *value;
    r->headers_out.content_type_hash = hv->hash;
    r->headers_out.content_type_lowcase = NULL;

    /* both mean: do not add if not exists */
    value->len = 0;
    hv->no_create = 1;

    return ngx_http_wasm_set_header_helper_response(r, hv, value, NULL, 1);
}


static ngx_inline ngx_int_t
ngx_http_clear_builtin_header(ngx_http_request_t *r,
    ngx_http_wasm_header_val_t *hv)
{
    static ngx_str_t   null = ngx_null_string;

    return ngx_http_set_builtin_header(r, hv, &null);
}
