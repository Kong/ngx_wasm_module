#ifndef _NGX_HTTP_WASM_H_INCLUDED_
#define _NGX_HTTP_WASM_H_INCLUDED_


#include <ngx_http.h>

#include <ngx_wasm_ops.h>


typedef struct {
    ngx_http_request_t              *r;
    ngx_wasm_op_ctx_t                opctx;

    /* control flow */

    unsigned                         sent_last:1;
} ngx_http_wasm_req_ctx_t;


ngx_int_t ngx_http_wasm_send_header(ngx_http_request_t *r);

ngx_int_t ngx_http_wasm_send_chain_link(ngx_http_request_t *r, ngx_chain_t *in);

extern ngx_wavm_host_def_t  ngx_http_wasm_host_interface;


#endif /* _NGX_HTTP_WASM_H_INCLUDED_ */
