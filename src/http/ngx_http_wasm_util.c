#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_http_wasm_util.h>


ngx_str_t *
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


ngx_int_t
ngx_http_wasm_produce_resp_headers(ngx_http_wasm_req_ctx_t *rctx)
{
    size_t                     len;
    u_char                    *p;
    ngx_int_t                  rc;
    ngx_str_t                  server_val, date_val, ct_val, last_mod_val;
    static ngx_str_t           server = ngx_string("Server"),
                               date = ngx_string("Date"),
                               content_type = ngx_string("Content-Type"),
                               last_modified = ngx_string("Last-Modified");
    static u_char              ngx_http_server_string[] = "nginx",
                               ngx_http_server_full_string[] = NGINX_VER,
                               ngx_http_server_build_string[] = NGINX_VER_BUILD;
    ngx_http_core_loc_conf_t  *clcf;
    ngx_http_request_t        *r;

    r = rctx->r;
    clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "wasm producing default response headers");

    if (r->headers_out.server == NULL) {
        /* Server */

        if (clcf->server_tokens == NGX_HTTP_SERVER_TOKENS_ON) {
            p = ngx_http_server_full_string;
            len = sizeof(ngx_http_server_full_string) - 1;

        } else if (clcf->server_tokens == NGX_HTTP_SERVER_TOKENS_BUILD) {
            p = ngx_http_server_build_string;
            len = sizeof(ngx_http_server_build_string) - 1;

        } else {
            p = ngx_http_server_string;
            len = sizeof(ngx_http_server_string) - 1;
        }

        server_val.len = len;
        server_val.data = ngx_pnalloc(r->pool, server_val.len);
        if (server_val.data == NULL) {
            return NGX_ERROR;
        }

        ngx_memcpy(server_val.data, p, server_val.len);

        rc = ngx_http_wasm_set_resp_header(r, server, server_val, 0);
        if (rc != NGX_OK) {
            return NGX_ERROR;
        }
    }

    if (r->headers_out.date == NULL) {
        /* Date */

        date_val.len = ngx_cached_http_time.len;
        date_val.data = ngx_cached_http_time.data;

        rc = ngx_http_wasm_set_resp_header(r, date, date_val, 0);
        if (rc != NGX_OK) {
            return NGX_ERROR;
        }
    }

    if (r->headers_out.content_type_len) {
        /* Content-Type */

        len = 0;
        ct_val.len = r->headers_out.content_type.len;

        if (r->headers_out.charset.len) {
            len += sizeof("; charset=") - 1 + r->headers_out.charset.len;
        }

        ct_val.len += len;
        ct_val.data = ngx_pnalloc(r->pool, ct_val.len);
        if (ct_val.data == NULL) {
            return NGX_ERROR;
        }

        p = ngx_cpymem(ct_val.data, r->headers_out.content_type.data,
                       r->headers_out.content_type.len);

        if (len) {
            p = ngx_cpymem(p, "; charset=", sizeof("; charset=") - 1);
            ngx_memcpy(p, r->headers_out.charset.data,
                       r->headers_out.charset.len);
        }

        ngx_log_debug2(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                       "wasm setting response header: \"%V: %V\"",
                       &content_type, &ct_val);

        /* make a copy in r->headers_out.headers since we will
         * clear r->headers_out.content_type.len */
        rc = ngx_wasm_add_list_elem(r->pool, &r->headers_out.headers,
                                    content_type.data, content_type.len,
                                    ct_val.data, ct_val.len);
        if (rc != NGX_OK) {
            return NGX_ERROR;
        }

        /* skip in ngx_http_header_filter, may affect stock logging */
        r->headers_out.content_type.len = 0;
        r->headers_out.content_type_len = 0; /* re-entrency */
    }

    if (r->headers_out.content_length == NULL
        && (rctx->local_resp_body_len || r->headers_out.content_length_n))
    {
        /* Content-Length */

        rc = ngx_http_wasm_set_resp_content_length(r, rctx->local_resp_body_len
                                            ? (off_t) rctx->local_resp_body_len
                                            : r->headers_out.content_length_n);
        if (rc != NGX_OK) {
            return NGX_ERROR;
        }
    }

    if (r->headers_out.last_modified == NULL
        && r->headers_out.last_modified_time != -1)
    {
        /* Last-Modified */

        last_mod_val.len = sizeof("Mon, 28 Sep 1970 06:00:00 GMT") - 1;
        last_mod_val.data = ngx_pnalloc(r->pool, last_mod_val.len);
        if (last_mod_val.data == NULL) {
            return NGX_ERROR;
        }

        ngx_http_time(last_mod_val.data, r->headers_out.last_modified_time);

        rc = ngx_http_wasm_set_resp_header(r, last_modified, last_mod_val, 0);
        if (rc != NGX_OK) {
            return NGX_ERROR;
        }

        /* skip in ngx_http_header_filter, may affect stock logging */
        r->headers_out.last_modified_time = -1;
    }

    /* TODO: Location */

    /*
     * Note: no way to prevent these from being injected
     * by ngx_http_header_filter:
     *
     *  - Connection
     *  - Transfer-Encoding
     *  - Vary
     */

    rctx->produced_default_headers = 1;

    return NGX_OK;
}
