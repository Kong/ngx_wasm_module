#ifndef _NGX_HTTP_WASM_H_INCLUDED_
#define _NGX_HTTP_WASM_H_INCLUDED_


#include <ngx_http.h>
#include <ngx_wasm_ops.h>


#define NGX_HTTP_WASM_MAX_REQ_HEADERS  100

#define NGX_HTTP_WASM_HEADER_FILTER_PHASE  (NGX_HTTP_LOG_PHASE + 1)


typedef enum {
    NGX_HTTP_WASM_ESCAPE_URI = 0,
    NGX_HTTP_WASM_ESCAPE_URI_COMPONENT,
    NGX_HTTP_WASM_ESCAPE_ARGS,
    NGX_HTTP_WASM_ESCAPE_HEADER_NAME,
    NGX_HTTP_WASM_ESCAPE_HEADER_VALUE,
} ngx_http_wasm_escape_kind;


typedef struct {
    ngx_http_request_t              *r;
    ngx_wasm_op_ctx_t                opctx;

    /* control flow */

    ngx_http_handler_pt              r_content_handler;

    ngx_int_t                        local_resp_status;
    ngx_str_t                        local_resp_reason;
    ngx_array_t                     *local_resp_headers;
    ngx_chain_t                     *local_resp_body;
    size_t                           local_resp_body_len;
    unsigned                         local_resp:1;
    unsigned                         sent_last:1;
    unsigned                         finalized:1;

    unsigned                         entered_content:1;
} ngx_http_wasm_req_ctx_t;


/* ops */
ngx_int_t ngx_http_wasm_stash_local_response(ngx_http_wasm_req_ctx_t *rctx,
    ngx_int_t status, u_char *reason, size_t reason_len, ngx_array_t *headers,
    u_char *body, size_t body_len);
ngx_int_t ngx_http_wasm_flush_local_response(ngx_http_request_t *r,
    ngx_http_wasm_req_ctx_t *rctx);


/* utils */
ngx_int_t ngx_http_wasm_set_resp_header(ngx_http_request_t *r, ngx_str_t key,
    ngx_str_t value, unsigned override);
ngx_int_t ngx_http_wasm_set_resp_content_length(ngx_http_request_t *r,
    off_t cl);
uintptr_t ngx_http_wasm_escape(u_char *dst, u_char *src, size_t size,
    ngx_http_wasm_escape_kind kind);
ngx_int_t ngx_http_wasm_send_chain_link(ngx_http_request_t *r, ngx_chain_t *in);


static ngx_inline ngx_uint_t
ngx_http_wasm_req_headers_count(ngx_http_request_t *r)
{
    return ngx_wasm_list_nelts(&r->headers_in.headers);
}


static ngx_inline ngx_uint_t
ngx_http_wasm_resp_headers_count(ngx_http_request_t *r)
{
    return ngx_wasm_list_nelts(&r->headers_out.headers);
}


extern ngx_wavm_host_def_t  ngx_http_wasm_host_interface;


#endif /* _NGX_HTTP_WASM_H_INCLUDED_ */
