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
