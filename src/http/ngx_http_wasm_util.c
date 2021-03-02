#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_http_wasm.h>


ngx_uint_t
ngx_http_wasm_req_headers_count(ngx_http_request_t *r)
{
    ngx_uint_t        c;
    ngx_list_part_t  *part;

    part = &r->headers_in.headers.part;
    c = part->nelts;

    while (part->next != NULL) {
        part = part->next;
        c += part->nelts;
    }

    if (c > NGX_HTTP_WASM_MAX_REQ_HEADERS) {
        ngx_wasm_log_error(NGX_LOG_ERR, r->connection->log, 0,
                           "truncated request headers to %d",
                           NGX_HTTP_WASM_MAX_REQ_HEADERS);

        return NGX_HTTP_WASM_MAX_REQ_HEADERS;
    }

    return c;
}


ngx_int_t
ngx_http_wasm_send_header(ngx_http_request_t *r)
{
#define NGX_WASM_DEAD 0
    if (r->header_sent) {
        ngx_log_error(NGX_LOG_ALERT, r->connection->log, 0,
                      "NYI - http wasm: catch header sent");
        return NGX_ERROR;
    }

    if (!r->headers_out.status) {
        r->headers_out.status = NGX_HTTP_OK;
    }

    if (ngx_http_set_content_type(r) != NGX_OK) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

#if NGX_WASM_DEAD
    ngx_http_clear_content_length(r);
    ngx_http_clear_accept_ranges(r);
#endif

    return ngx_http_send_header(r);
#undef NGX_WASM_DEAD
}


ngx_int_t
ngx_http_wasm_send_chain_link(ngx_http_request_t *r, ngx_chain_t *in)
{
    ngx_int_t   rc;

    rc = ngx_http_wasm_send_header(r);
    if (rc == NGX_ERROR || rc > NGX_OK || r->header_only) {
        return rc;
    }

    if (in == NULL) {
        rc = ngx_http_send_special(r, NGX_HTTP_LAST);
        if (rc >= NGX_HTTP_SPECIAL_RESPONSE) {
            return rc;
        }

        return NGX_OK;
    }

    return ngx_http_output_filter(r, in);
}
