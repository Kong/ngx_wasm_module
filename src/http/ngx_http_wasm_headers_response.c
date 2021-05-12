#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"


#include <nginx.h>
#include <ngx_http_wasm.h>


static ngx_int_t ngx_http_set_location_header_handler(ngx_http_request_t *r,
    ngx_http_wasm_header_val_t *hv, ngx_str_t *value);
static ngx_int_t ngx_http_set_last_modified_header_handler(
    ngx_http_request_t *r, ngx_http_wasm_header_val_t *hv, ngx_str_t *value);
static ngx_int_t ngx_http_set_content_length_header_handler(
    ngx_http_request_t *r, ngx_http_wasm_header_val_t *hv, ngx_str_t *value);
static ngx_int_t ngx_http_set_content_type_header_handler(ngx_http_request_t *r,
    ngx_http_wasm_header_val_t *hv, ngx_str_t *value);
static ngx_int_t ngx_http_wasm_set_builtin_multi_header_handler(
    ngx_http_request_t *r, ngx_http_wasm_header_val_t *hv, ngx_str_t *value);


static ngx_http_wasm_header_t  ngx_http_wasm_special_resp_headers[] = {

    { ngx_string("Server"),
                 offsetof(ngx_http_headers_out_t, server),
                 ngx_http_wasm_set_builtin_header_handler },

    { ngx_string("Date"),
                 offsetof(ngx_http_headers_out_t, date),
                 ngx_http_wasm_set_builtin_header_handler },

    { ngx_string("Content-Length"),
                 offsetof(ngx_http_headers_out_t, content_length),
                 ngx_http_set_content_length_header_handler },

    { ngx_string("Content-Encoding"),
                 offsetof(ngx_http_headers_out_t, content_encoding),
                 ngx_http_wasm_set_builtin_header_handler },

    { ngx_string("Location"),
                 offsetof(ngx_http_headers_out_t, location),
                 ngx_http_set_location_header_handler },

    { ngx_string("Refresh"),
                 offsetof(ngx_http_headers_out_t, refresh),
                 ngx_http_wasm_set_builtin_header_handler },

    { ngx_string("Last-Modified"),
                 offsetof(ngx_http_headers_out_t, last_modified),
                 ngx_http_set_last_modified_header_handler },

    { ngx_string("Content-Range"),
                 offsetof(ngx_http_headers_out_t, content_range),
                 ngx_http_wasm_set_builtin_header_handler },

    { ngx_string("Accept-Ranges"),
                 offsetof(ngx_http_headers_out_t, accept_ranges),
                 ngx_http_wasm_set_builtin_header_handler },

    { ngx_string("WWW-Authenticate"),
                 offsetof(ngx_http_headers_out_t, www_authenticate),
                 ngx_http_wasm_set_builtin_header_handler },

    { ngx_string("Expires"),
                 offsetof(ngx_http_headers_out_t, expires),
                 ngx_http_wasm_set_builtin_header_handler },

    { ngx_string("E-Tag"),
                 offsetof(ngx_http_headers_out_t, etag),
                 ngx_http_wasm_set_builtin_header_handler },

    { ngx_string("ETag"),
                 offsetof(ngx_http_headers_out_t, etag),
                 ngx_http_wasm_set_builtin_header_handler },

    { ngx_string("Content-Type"),
                 offsetof(ngx_http_headers_out_t, content_type),
                 ngx_http_set_content_type_header_handler },

    { ngx_string("Cache-Control"),
                 offsetof(ngx_http_headers_out_t, cache_control),
                 ngx_http_wasm_set_builtin_multi_header_handler },

    { ngx_string("Link"),
                 offsetof(ngx_http_headers_out_t, link),
                 ngx_http_wasm_set_builtin_multi_header_handler },

    { ngx_null_string, 0, ngx_http_wasm_set_header_handler }

};


size_t
ngx_http_wasm_resp_headers_count(ngx_http_request_t *r)
{
    return ngx_wasm_list_nelts(&r->headers_out.headers);
}


ngx_int_t
ngx_http_wasm_set_resp_header(ngx_http_request_t *r, ngx_str_t key,
    ngx_str_t value, ngx_uint_t mode)
{
    return ngx_http_wasm_set_header(r, NGX_HTTP_WASM_HEADERS_RESPONSE,
                                    ngx_http_wasm_special_resp_headers,
                                    key, value, mode);
}


ngx_int_t
ngx_http_wasm_set_resp_content_length(ngx_http_request_t *r, off_t cl)
{
    u_char            *p;
    ngx_str_t          value;
    static ngx_str_t   key = ngx_string("Content-Length");

    dd("wasm set response header: '%.*s: %f'",
       (int) key.len, key.data, (double) cl);

    if (r->headers_out.content_length_n >= 0
        && r->headers_out.content_length_n == cl)
    {
        return NGX_OK;
    }

    p = ngx_pnalloc(r->pool, NGX_OFF_T_LEN);
    if (p == NULL) {
        return NGX_ERROR;
    }

    value.data = p;
    value.len = ngx_sprintf(p, "%O", cl) - p;

    return ngx_http_wasm_set_resp_header(r, key, value,
                                         NGX_HTTP_WASM_HEADERS_SET);
}


/* handlers */


static ngx_int_t
ngx_http_set_location_header_handler(ngx_http_request_t *r,
    ngx_http_wasm_header_val_t *hv, ngx_str_t *value)
{
    ngx_int_t         rc;
    ngx_table_elt_t  *h;

    rc = ngx_http_wasm_set_builtin_header_handler(r, hv, value);
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
ngx_http_set_last_modified_header_handler(ngx_http_request_t *r,
    ngx_http_wasm_header_val_t *hv, ngx_str_t *value)
{
    if (value->len == 0) {
        ngx_http_clear_last_modified(r);
        return NGX_OK;
    }

    r->headers_out.last_modified_time = ngx_http_parse_time(value->data,
                                                            value->len);

    return ngx_http_wasm_set_builtin_header_handler(r, hv, value);
}


static ngx_int_t
ngx_http_set_content_length_header_handler(ngx_http_request_t *r,
    ngx_http_wasm_header_val_t *hv, ngx_str_t *value)
{
    off_t   len;

    if (value->len == 0) {
        ngx_http_clear_content_length(r);
        return NGX_OK;
    }

    len = ngx_atoof(value->data, value->len);
    if (len == NGX_ERROR) {
        ngx_wasm_log_error(NGX_LOG_ERR, r->connection->log, 0,
                           "attempt to set invalid Content-Length "
                           "response header: \"%V\"",
                           value);
        return NGX_ERROR;
    }

    r->headers_out.content_length_n = len;

    return ngx_http_wasm_set_builtin_header_handler(r, hv, value);
}


static ngx_int_t
ngx_http_set_content_type_header_handler(ngx_http_request_t *r,
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

    /* Not injected into r->headers_out.headers, injected as shim header
     * if requested */

    return NGX_OK;
}


static ngx_int_t
ngx_http_wasm_set_builtin_multi_header_handler(ngx_http_request_t *r,
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

    if (hv->mode == NGX_HTTP_WASM_HEADERS_APPEND) {
        ph = headers->elts;

        for (i = 0; i < headers->nelts; i++) {
            if (ph[i]->hash == 0) {
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
        ph[0]->hash = value->len ? hv->hash : 0;

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
    h->hash = value->len ? hv->hash : 0;
    h->key = hv->key;
    *ph = h;

    return NGX_OK;
}
