#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_http_wasm.h>


#define NGX_HTTP_WASM_HEADER_HASH_SKIP  0


typedef struct ngx_http_wasm_header_val_s  ngx_http_wasm_header_val_t;

typedef ngx_int_t (*ngx_http_wasm_header_set_pt)(ngx_http_request_t *r,
    ngx_http_wasm_header_val_t *hv, ngx_str_t *value);


struct ngx_http_wasm_header_val_s {
    ngx_http_complex_value_t                 value;
    ngx_uint_t                               hash;
    ngx_str_t                                key;
    ngx_http_wasm_header_set_pt              handler;
    ngx_uint_t                               offset;
    unsigned                                 no_override;
};


typedef struct {
    ngx_str_t                                name;
    ngx_uint_t                               offset;
    ngx_http_wasm_header_set_pt              handler;
} ngx_http_wasm_header_t;


static ngx_int_t ngx_http_set_header(ngx_http_request_t *r,
    ngx_http_wasm_header_val_t *hv, ngx_str_t *value);
static ngx_int_t ngx_http_set_header_helper(ngx_http_request_t *r,
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
static ngx_str_t *ngx_http_copy_escaped(ngx_str_t *dst, ngx_pool_t *pool,
    ngx_http_wasm_escape_kind kind);


static ngx_http_wasm_header_t  ngx_http_wasm_headers[] = {

    { ngx_string("Server"),
                 offsetof(ngx_http_headers_out_t, server),
                 ngx_http_set_builtin_header },

    { ngx_string("Date"),
                 offsetof(ngx_http_headers_out_t, date),
                 ngx_http_set_builtin_header },

    { ngx_string("Content-Length"),
                 offsetof(ngx_http_headers_out_t, content_length),
                 ngx_http_set_content_length_header },

    { ngx_string("Content-Encoding"),
                 offsetof(ngx_http_headers_out_t, content_encoding),
                 ngx_http_set_builtin_header },

    { ngx_string("Location"),
                 offsetof(ngx_http_headers_out_t, location),
                 ngx_http_set_location_header },

    { ngx_string("Refresh"),
                 offsetof(ngx_http_headers_out_t, refresh),
                 ngx_http_set_builtin_header },

    { ngx_string("Last-Modified"),
                 offsetof(ngx_http_headers_out_t, last_modified),
                 ngx_http_set_last_modified_header },

    { ngx_string("Content-Range"),
                 offsetof(ngx_http_headers_out_t, content_range),
                 ngx_http_set_builtin_header },

    { ngx_string("Accept-Ranges"),
                 offsetof(ngx_http_headers_out_t, accept_ranges),
                 ngx_http_set_builtin_header },

    { ngx_string("WWW-Authenticate"),
                 offsetof(ngx_http_headers_out_t, www_authenticate),
                 ngx_http_set_builtin_header },

    { ngx_string("Expires"),
                 offsetof(ngx_http_headers_out_t, expires),
                 ngx_http_set_builtin_header },

    { ngx_string("E-Tag"),
                 offsetof(ngx_http_headers_out_t, etag),
                 ngx_http_set_builtin_header },

    { ngx_string("ETag"),
                 offsetof(ngx_http_headers_out_t, etag),
                 ngx_http_set_builtin_header },

    { ngx_string("Content-Type"),
                 offsetof(ngx_http_headers_out_t, content_type),
                 ngx_http_set_content_type_header },

    /* charset */

    { ngx_string("Cache-Control"),
                 offsetof(ngx_http_headers_out_t, cache_control),
                 ngx_http_set_builtin_multi_header },

    { ngx_string("Link"),
                 offsetof(ngx_http_headers_out_t, link),
                 ngx_http_set_builtin_multi_header },

    { ngx_null_string, 0, ngx_http_set_header }
};


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
    hv.no_override = !override;
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
    return ngx_http_set_header_helper(r, hv, value, NULL, 0);
}


static ngx_int_t
ngx_http_set_header_helper(ngx_http_request_t *r, ngx_http_wasm_header_val_t *hv,
    ngx_str_t *value, ngx_table_elt_t **out, unsigned no_create)
{
    size_t            i;
    unsigned          found = 0;
    ngx_table_elt_t  *h;
    ngx_list_part_t  *part;

    if (hv->no_override) {
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

        if (h[i].hash != 0
            && h[i].key.len == hv->key.len
            && ngx_strncasecmp(hv->key.data, h[i].key.data, h[i].key.len) == 0)
        {
            if (value->len == 0 || found) {
                dd("clearing '%.*s' header line",
                   (int) hv->key.len, hv->key.data);

                h[i].value.len = 0;
                h[i].hash = NGX_HTTP_WASM_HEADER_HASH_SKIP;

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

    /* XXX we still need to create header slot even if the value
     * is empty because some builtin headers like Last-Modified
     * relies on this to get cleared */

    dd("adding '%.*s' header line",
       (int) hv->key.len, hv->key.data);

    h = ngx_list_push(&r->headers_out.headers);
    if (h == NULL) {
        return NGX_ERROR;
    }

    h->hash = value->len ? hv->hash : NGX_HTTP_WASM_HEADER_HASH_SKIP;
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
        return ngx_http_set_header_helper(r, hv, value, old, 0);
    }

    h = *old;

    if (value->len == 0) {
        dd("clearing existing '%.*s' builtin header",
           (int) hv->key.len, hv->key.data);

        h->hash = NGX_HTTP_WASM_HEADER_HASH_SKIP;

        return NGX_OK;
    }

    dd("updating existing '%.*s' builtin header",
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
    ngx_array_t      *pa;
    ngx_table_elt_t  *ho, **ph;

    pa = (ngx_array_t *) ((char *) &r->headers_out + hv->offset);
    if (pa->elts == NULL) {
        if (ngx_array_init(pa, r->pool, 2, sizeof(ngx_table_elt_t *))
            != NGX_OK)
        {
            return NGX_ERROR;
        }
    }

    if (hv->no_override) {
        ph = pa->elts;

        for (i = 0; i < pa->nelts; i++) {
            if (!ph[i]->hash) {
                ph[i]->value = *value;
                ph[i]->hash = hv->hash;
                return NGX_OK;
            }
        }

        goto create;
    }

    /* override old values (if any) */

    if (pa->nelts > 0) {
        ph = pa->elts;
        for (i = 1; i < pa->nelts; i++) {
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

    ph = ngx_array_push(pa);
    if (ph == NULL) {
        return NGX_ERROR;
    }

    ho = ngx_list_push(&r->headers_out.headers);
    if (ho == NULL) {
        return NGX_ERROR;
    }

    ho->value = *value;

    if (value->len == 0) {
        ho->hash = 0;

    } else {
        ho->hash = hv->hash;
    }

    ho->key = hv->key;
    *ph = ho;

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

    value->len = 0;

    return ngx_http_set_header_helper(r, hv, value, NULL, 1);
}


static ngx_inline ngx_int_t
ngx_http_clear_builtin_header(ngx_http_request_t *r,
    ngx_http_wasm_header_val_t *hv)
{
    static ngx_str_t   null = ngx_null_string;

    return ngx_http_set_builtin_header(r, hv, &null);
}


static ngx_str_t *
ngx_http_copy_escaped(ngx_str_t *dst, ngx_pool_t *pool,
    ngx_http_wasm_escape_kind kind)
{
    size_t   escape, len;
    u_char  *data;

    data = dst->data;
    len = dst->len;

    escape = ngx_http_wasm_escape(NULL, data, len, kind);
    if (escape > 0) {
        /* null-terminated for ngx_http_header_filter */
        dst->data = ngx_pnalloc(pool, len + 2 * escape + 1);
        if (dst->data == NULL) {
            return NULL;
        }

        ngx_http_wasm_escape(dst->data, data, len, kind);
        dst->len = len + 2 * escape;
        dst->data[dst->len] = '\0';
    }

    return dst;
}


ngx_int_t
ngx_http_wasm_send_chain_link(ngx_http_request_t *r, ngx_chain_t *in)
{
    ngx_int_t   rc;

    if (!r->headers_out.status) {
        r->headers_out.status = NGX_HTTP_OK;
    }

    rc = ngx_http_send_header(r);

    if (rc == NGX_ERROR || rc > NGX_OK || r->header_only) {
        /* NGX_ERROR, NGX_HTTP_* */
        return rc;
    }

    /* NGX_OK, NGX_AGAIN */

    if (in == NULL) {
        rc = ngx_http_send_special(r, NGX_HTTP_LAST);
        if (rc >= NGX_HTTP_SPECIAL_RESPONSE) {
            /* >= 300 */
            return rc;
        }

        return NGX_OK;
    }

    return ngx_http_output_filter(r, in);
}


/* ops */


void
ngx_http_wasm_discard_local_response(ngx_http_wasm_req_ctx_t *rctx)
{
    ngx_http_request_t  *r;

    r = rctx->r;

    rctx->local_resp = 0;
    rctx->local_resp_status = 0;
    rctx->local_resp_body_len = 0;
    rctx->local_resp_reason.len = 0;

    if (rctx->local_resp_reason.data) {
        ngx_pfree(r->pool, rctx->local_resp_reason.data);
    }

    if (rctx->local_resp_headers) {
        ngx_array_destroy(rctx->local_resp_headers);
        rctx->local_resp_headers = NULL;
    }

    if (rctx->local_resp_body) {
        ngx_free_chain(r->pool, rctx->local_resp_body);
        rctx->local_resp_body = NULL;
    }
}


ngx_int_t
ngx_http_wasm_stash_local_response(ngx_http_wasm_req_ctx_t *rctx,
    ngx_int_t status, u_char *reason, size_t reason_len, ngx_array_t *headers,
    u_char *body, size_t body_len)
{
    u_char              *p = NULL;
    ngx_buf_t           *b = NULL;
    ngx_chain_t         *cl = NULL;
    ngx_http_request_t  *r = rctx->r;

    if (rctx->local_resp) {
        ngx_wasm_log_error(NGX_LOG_ERR, r->connection->log, 0,
                           "local response already stashed");
        return NGX_ABORT;
    }

    /* status */

    if (status < 100 || status > 999) {
        return NGX_DECLINED;
    }

    rctx->local_resp_status = status;

    /* reason */

    if (reason_len) {
        reason_len += 5; /* "ddd <reason>\0" */
        p = ngx_pnalloc(r->pool, reason_len);
        if (p == NULL) {
            goto fail;
        }

        ngx_snprintf(p, reason_len, "%03ui %s", status, reason);

        rctx->local_resp_reason.data = p;
        rctx->local_resp_reason.len = reason_len;
    }

    /* headers */

    rctx->local_resp_headers = headers;

    /* body */

    if (body_len) {
        b = ngx_create_temp_buf(r->pool, body_len + sizeof(LF));
        if (b == NULL) {
            goto fail;
        }

        b->sync = 1;
        b->last = ngx_copy(b->last, body, body_len);
        //*b->last++ = LF;

        b->last_buf = 1;
        b->last_in_chain = 1;

        cl = ngx_alloc_chain_link(r->connection->pool);
        if (cl == NULL) {
            goto fail;
        }

        cl->buf = b;
        cl->next = NULL;

        rctx->local_resp_body = cl;
        rctx->local_resp_body_len = body_len;
    }

    rctx->local_resp = 1;

    return NGX_OK;

fail:

    ngx_http_wasm_discard_local_response(rctx);

    return NGX_ERROR;
}


ngx_int_t
ngx_http_wasm_flush_local_response(ngx_http_request_t *r,
    ngx_http_wasm_req_ctx_t *rctx)
{
    size_t                    i;
    ngx_int_t                 rc;
    ngx_table_elt_t          *elt;

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "wasm flushing local_response: %d", rctx->local_resp);

    if (!rctx->local_resp) {
        return NGX_DECLINED;
    }

    rc = ngx_http_discard_request_body(r);
    if (rc != NGX_OK) {
        return NGX_ERROR;
    }

    r->headers_out.status = rctx->local_resp_status;

    if (r->err_status) {
        r->err_status = 0;
    }

    if (rctx->local_resp_reason.len) {
        r->headers_out.status_line.data = rctx->local_resp_reason.data;
        r->headers_out.status_line.len = rctx->local_resp_reason.len;
    }

    if (rctx->local_resp_headers) {
        for (i = 0; i < rctx->local_resp_headers->nelts; i++) {
            elt = &((ngx_table_elt_t *) rctx->local_resp_headers->elts)[i];

            rc = ngx_http_wasm_set_resp_header(r, elt->key, elt->value, 1);
            if (rc != NGX_OK) {
                return NGX_ERROR;
            }
        }
    }

    if (rctx->local_resp_body_len) {
        if (ngx_http_set_content_type(r) != NGX_OK) {
            return NGX_ERROR;
        }

        if (ngx_http_wasm_set_resp_content_length(r, rctx->local_resp_body_len)
            != NGX_OK)
        {
            return NGX_ERROR;
        }
    }

    rc = ngx_http_wasm_send_chain_link(r, rctx->local_resp_body);

    rctx->sent_last = 1;

    return rc;
}
