#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_http_wasm.h>


static ngx_str_t *ngx_http_wasm_shim_content_type(ngx_http_wasm_req_ctx_t *rctx);
static ngx_str_t *ngx_http_wasm_shim_content_len(ngx_http_wasm_req_ctx_t *rctx);
static ngx_str_t *ngx_http_wasm_shim_connection(ngx_http_wasm_req_ctx_t *rctx);
static ngx_str_t *ngx_http_wasm_shim_keep_alive(ngx_http_wasm_req_ctx_t *rctx);
static ngx_str_t *ngx_http_wasm_shim_transfer_encoding(
    ngx_http_wasm_req_ctx_t *rctx);
static ngx_str_t *ngx_http_wasm_shim_vary(ngx_http_wasm_req_ctx_t *rctx);


static ngx_http_wasm_shim_header_t  ngx_http_wasm_shim_headers[] = {

    { ngx_string("Content-Type"),
                 ngx_http_wasm_shim_content_type },

    { ngx_string("Content-Length"),
                 ngx_http_wasm_shim_content_len },

    { ngx_string("Transfer-Encoding"),
                 ngx_http_wasm_shim_transfer_encoding },

    { ngx_string("Connection"),
                 ngx_http_wasm_shim_connection },

    { ngx_string("Keep-Alive"),
                 ngx_http_wasm_shim_keep_alive },

    { ngx_string("Vary"),
                 ngx_http_wasm_shim_vary },

    { ngx_null_string, NULL }

};


ngx_str_t *
ngx_http_wasm_get_shim_header(ngx_http_wasm_req_ctx_t *rctx, u_char *key,
    size_t key_len)
{
    size_t                        i;
    ngx_http_wasm_shim_header_t  *sh;

    for (i = 0; ngx_http_wasm_shim_headers[i].key.len; i++) {
        sh = &ngx_http_wasm_shim_headers[i];

        if (sh->key.len != key_len
            || ngx_strncasecmp(sh->key.data, key, key_len) != 0)
        {
            continue;
        }

        return sh->handler(rctx);
    }

    return NULL;
}


ngx_array_t *
ngx_http_wasm_get_shim_headers(ngx_http_wasm_req_ctx_t *rctx)
{
    size_t                        i;
    ngx_str_t                    *value;
    ngx_table_elt_t              *h;
    ngx_http_wasm_shim_header_t  *sh;
    ngx_http_request_t           *r = rctx->r;

    if (rctx->resp_shim_headers.elts) {
        if (!rctx->reset_resp_shims) {
            goto cached;
        }

        ngx_array_destroy(&rctx->resp_shim_headers);
    }

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "wasm producing shim headers");

    if (ngx_array_init(&rctx->resp_shim_headers, r->pool, 4,
                       sizeof(ngx_table_elt_t))
        != NGX_OK)
    {
        return NULL;
    }

    for (i = 0; ngx_http_wasm_shim_headers[i].key.len; i++) {
        sh = &ngx_http_wasm_shim_headers[i];

        value = sh->handler(rctx);
        if (value == NULL) {
            continue;
        }

        ngx_log_debug2(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                       "wasm shim header: \"%V: %V\"",
                       &sh->key, value);

        h = ngx_array_push(&rctx->resp_shim_headers);
        if (h == NULL) {
            return NULL;
        }

        h->key = sh->key;
        h->value = *value;
        h->lowcase_key = ngx_pnalloc(r->pool, h->key.len);
        if (h->lowcase_key == NULL) {
            return NULL;
        }

        ngx_strlow(h->lowcase_key, h->key.data, h->key.len);
    }

    rctx->reset_resp_shims = 0;

cached:

    return &rctx->resp_shim_headers;
}


ngx_uint_t
ngx_http_wasm_count_shim_headers(ngx_http_wasm_req_ctx_t *rctx)
{
    if (ngx_http_wasm_get_shim_headers(rctx) == NULL) {
        return 0;
    }

    return rctx->resp_shim_headers.nelts;
}


static ngx_str_t *
ngx_http_wasm_shim_content_type(ngx_http_wasm_req_ctx_t *rctx)
{
    ngx_http_request_t  *r = rctx->r;

    if (r->headers_out.content_type.len) {
        return &r->headers_out.content_type;
    }

    return NULL;
}


static ngx_str_t *
ngx_http_wasm_shim_content_len(ngx_http_wasm_req_ctx_t *rctx)
{
    u_char              *p;
    ngx_str_t           *value = NULL;
    ngx_http_request_t  *r = rctx->r;

    if (r->headers_out.content_length == NULL
        && r->headers_out.content_length_n >= 0)
    {
        value = ngx_palloc(r->pool, sizeof(ngx_str_t));
        if (value == NULL) {
            return NULL;
        }

        p = ngx_pnalloc(r->pool, NGX_OFF_T_LEN);
        if (p == NULL) {
            return NULL;
        }

        value->data = p;
        value->len = ngx_sprintf(p, "%O", r->headers_out.content_length_n) - p;
    }

    return value;
}


static ngx_str_t *
ngx_http_wasm_shim_connection(ngx_http_wasm_req_ctx_t *rctx)
{
    ngx_http_request_t        *r = rctx->r;
    static ngx_str_t           val_upgrade = ngx_string("upgrade");
    static ngx_str_t           val_keepalive = ngx_string("keep-alive");
    static ngx_str_t           val_close = ngx_string("close");

    if (r->headers_out.status == NGX_HTTP_SWITCHING_PROTOCOLS) {
        return &val_upgrade;

    } else if (rctx->req_keepalive || r->keepalive) {
        return &val_keepalive;

    } else {
        return &val_close;
    }
}


static ngx_str_t *
ngx_http_wasm_shim_keep_alive(ngx_http_wasm_req_ctx_t *rctx)
{
    size_t                     len;
    ngx_http_request_t        *r = rctx->r;
    ngx_http_core_loc_conf_t  *clcf;
    ngx_str_t                 *val_timeout = NULL;

    if (rctx->req_keepalive) {
        clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);

        if (clcf->keepalive_header) {
            val_timeout = ngx_palloc(r->pool, sizeof(ngx_str_t));
            if (val_timeout == NULL) {
                return NULL;
            }

            len = sizeof("timeout=") - 1 + NGX_TIME_T_LEN;
            val_timeout->data = ngx_pnalloc(r->pool, len);
            if (val_timeout->data == NULL) {
                return NULL;
            }

            val_timeout->len = ngx_sprintf(val_timeout->data, "timeout=%T",
                                           clcf->keepalive_header)
                               - val_timeout->data;
        }
    }

    return val_timeout;
}


static ngx_str_t *
ngx_http_wasm_shim_transfer_encoding(ngx_http_wasm_req_ctx_t *rctx)
{
    ngx_http_request_t  *r = rctx->r;
    static ngx_str_t     chunked = ngx_string("chunked");

    if (r->chunked) {
        return &chunked;
    }

    return NULL;
}


static ngx_str_t *
ngx_http_wasm_shim_vary(ngx_http_wasm_req_ctx_t *rctx)
{
#if (NGX_HTTP_GZIP)
    ngx_http_request_t  *r = rctx->r;
    static ngx_str_t     value = ngx_string("Accept-Encoding");

    if (r->gzip_vary) {
        return &value;
    }
#endif

    return NULL;
}
