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
    ngx_int_t                  rc;
    static ngx_str_t           date = ngx_string("Date");
    ngx_str_t                  date_val;
    static ngx_str_t           last_modified = ngx_string("Last-Modified");
    ngx_str_t                  last_mod_val;
    static ngx_str_t           server = ngx_string("Server");
    ngx_str_t                 *server_val = NULL;
    static ngx_str_t           server_full = ngx_string(NGINX_VER);
    static ngx_str_t           server_build = ngx_string(NGINX_VER_BUILD);
    static ngx_str_t           server_default = ngx_string("nginx");
    ngx_http_request_t        *r = rctx->r;
    ngx_http_core_loc_conf_t  *clcf;

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "wasm producing default response headers");

    if (r->headers_out.server == NULL) {
        /* Server */

        clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);

        if (clcf->server_tokens == NGX_HTTP_SERVER_TOKENS_ON) {
            server_val = &server_full;

        } else if (clcf->server_tokens == NGX_HTTP_SERVER_TOKENS_BUILD) {
            server_val = &server_build;

        } else {
            server_val = &server_default;
        }

        if (server_val) {
            if (ngx_http_wasm_set_resp_header(r, &server, server_val,
                                              NGX_HTTP_WASM_HEADERS_SET)
                != NGX_OK)
            {
                return NGX_ERROR;
            }
        }
    }

    if (r->headers_out.date == NULL) {
        /* Date */

        date_val.len = ngx_cached_http_time.len;
        date_val.data = ngx_cached_http_time.data;

        rc = ngx_http_wasm_set_resp_header(r, &date, &date_val, 0);
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

        rc = ngx_http_wasm_set_resp_header(r, &last_modified, &last_mod_val, 0);
        if (rc != NGX_OK) {
            return NGX_ERROR;
        }

        /* skip in ngx_http_header_filter, may affect stock logging */
        r->headers_out.last_modified_time = -1;
    }

    /* TODO: Location */

    return NGX_OK;
}
