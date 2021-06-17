#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"


#include <nginx.h>
#include <ngx_http_wasm.h>


static ngx_int_t ngx_http_set_location_header_handler(
    ngx_http_wasm_header_set_ctx_t *hv);
static ngx_int_t ngx_http_set_last_modified_header_handler(
    ngx_http_wasm_header_set_ctx_t *hv);
static ngx_int_t ngx_http_set_content_length_header_handler(
    ngx_http_wasm_header_set_ctx_t *hv);
static ngx_int_t ngx_http_set_content_type_header_handler(
    ngx_http_wasm_header_set_ctx_t *hv);
static ngx_int_t ngx_http_wasm_set_builtin_multi_header_handler(
    ngx_http_wasm_header_set_ctx_t *hv);
static ngx_int_t ngx_http_wasm_set_connection_header_handler(
    ngx_http_wasm_header_set_ctx_t *hv);


static ngx_http_wasm_header_handler_t  ngx_http_wasm_resp_headers_handlers[] = {

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

    { ngx_string("Connection"), 0,
                 ngx_http_wasm_set_connection_header_handler },

    { ngx_null_string, 0, ngx_http_wasm_set_header_handler }

};


size_t
ngx_http_wasm_resp_headers_count(ngx_http_request_t *r)
{
    return ngx_wasm_list_nelts(&r->headers_out.headers);
}


ngx_int_t
ngx_http_wasm_set_resp_header(ngx_http_request_t *r, ngx_str_t *key,
    ngx_str_t *value, ngx_uint_t mode)
{
    return ngx_http_wasm_set_header(r, NGX_HTTP_WASM_HEADERS_RESPONSE,
                                    ngx_http_wasm_resp_headers_handlers,
                                    key, value, mode);
}


ngx_int_t
ngx_http_wasm_set_resp_content_length(ngx_http_request_t *r, off_t cl)
{
    u_char            *p;
    ngx_str_t          value;
    static ngx_str_t   key = ngx_string("Content-Length");

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

    return ngx_http_wasm_set_resp_header(r, &key, &value,
                                         NGX_HTTP_WASM_HEADERS_SET);
}


ngx_int_t
ngx_http_wasm_clear_resp_headers(ngx_http_request_t *r)
{
    return ngx_http_wasm_clear_headers_helper(r, NGX_HTTP_WASM_HEADERS_RESPONSE,
               ngx_http_wasm_resp_headers_handlers);
}


/* handlers */


static ngx_int_t
ngx_http_set_location_header_handler(ngx_http_wasm_header_set_ctx_t *hv)
{
    ngx_int_t            rc;
    ngx_table_elt_t     *h;
    ngx_http_request_t  *r;

    rc = ngx_http_wasm_set_builtin_header_handler(hv);
    if (rc != NGX_OK) {
        return rc;
    }

    r = hv->r;
    h = r->headers_out.location;

    if (h && h->value.len && h->value.data[0] == '/') {
        /* avoid local redirects without host names
         * in ngx_http_header_filter */
        r->headers_out.location = NULL;
    }

    return NGX_OK;
}


static ngx_int_t
ngx_http_set_last_modified_header_handler(ngx_http_wasm_header_set_ctx_t *hv)
{
    ngx_int_t            rc;
    ngx_str_t           *value;
    ngx_http_request_t  *r = hv->r;

    if (hv->mode == NGX_HTTP_WASM_HEADERS_REMOVE) {
        ngx_http_clear_last_modified(r);
        return NGX_OK;
    }

    rc = ngx_http_wasm_set_builtin_header_handler(hv);
    if (rc != NGX_OK) {
        return rc;
    }

    r = hv->r;
    value = hv->value;

    r->headers_out.last_modified_time = ngx_http_parse_time(value->data,
                                                            value->len);
    return NGX_OK;
}


static ngx_int_t
ngx_http_set_content_length_header_handler(ngx_http_wasm_header_set_ctx_t *hv)
{
    off_t                len;
    ngx_int_t            rc;
    ngx_str_t           *value;
    ngx_http_request_t  *r = hv->r;

    if (hv->mode == NGX_HTTP_WASM_HEADERS_REMOVE) {
        ngx_http_clear_content_length(r);
        return NGX_OK;
    }

    r = hv->r;
    value = hv->value;

    len = ngx_atoof(value->data, value->len);
    if (len == NGX_ERROR) {
        ngx_wasm_log_error(NGX_LOG_ERR, r->connection->log, 0,
                           "attempt to set invalid Content-Length "
                           "response header: \"%V\"", value);
        return NGX_DECLINED;
    }

    rc = ngx_http_wasm_set_builtin_header_handler(hv);
    if (rc != NGX_OK) {
        return rc;
    }

    r->headers_out.content_length_n = len;

    return NGX_OK;
}


static ngx_int_t
ngx_http_set_content_type_header_handler(ngx_http_wasm_header_set_ctx_t *hv)
{
    size_t               i;
    ngx_http_request_t  *r = hv->r;
    ngx_str_t           *value = hv->value;

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
ngx_http_wasm_set_builtin_multi_header_handler(ngx_http_wasm_header_set_ctx_t *hv)
{
    size_t               i;
    ngx_array_t         *headers;
    ngx_table_elt_t     *h, **ph;
    ngx_http_request_t  *r = hv->r;
    ngx_str_t           *key = hv->key;
    ngx_str_t           *value = hv->value;

    headers = (ngx_array_t *) ((char *) &r->headers_out + hv->handler->offset);

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

                ph[i]->hash = hv->hash;
                ph[i]->key = *key;
                ph[i]->value = *value;

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
        }

        if (hv->mode == NGX_HTTP_WASM_HEADERS_REMOVE) {
            ph[0]->hash = 0;

        } else {
            ph[0]->hash = hv->hash;
        }

        ph[0]->key = *key;
        ph[0]->value = *value;

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

    h->hash = hv->hash;
    h->key = *key;
    h->value = *value;

    *ph = h;

    return NGX_OK;
}


static ngx_int_t
ngx_http_wasm_set_connection_header_handler(ngx_http_wasm_header_set_ctx_t *hv)
{
    ngx_str_t                *value = hv->value;
    ngx_http_request_t       *r = hv->r;
    ngx_http_wasm_req_ctx_t  *rctx;

    if (ngx_http_wasm_rctx(r, &rctx) != NGX_OK) {
        return NGX_ERROR;
    }

    if (ngx_strcasestrn(value->data, "keep-alive", value->len - 1)) {
        r->keepalive = 1;
        rctx->req_keepalive = 1;
        return NGX_OK;

    } else if (ngx_strcasestrn(value->data, "close", value->len - 1)) {
        r->keepalive = 0;
        rctx->req_keepalive = 0;
        return NGX_OK;

    } else if (ngx_strcasestrn(value->data, "upgrade", value->len - 1)) {
        r->headers_out.status = NGX_HTTP_SWITCHING_PROTOCOLS;

        ngx_wasm_log_error(NGX_LOG_INFO, r->connection->log, 0,
                           "setting \"Connection: upgrade\" response "
                           "header, switching status code: %d",
                           r->headers_out.status);
        return NGX_OK;
    }

    ngx_wasm_log_error(NGX_LOG_ERR, r->connection->log, 0,
                       "attempt to set invalid Connection "
                       "response header: \"%V\"", value);

    return NGX_DECLINED;
}
