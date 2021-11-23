#ifndef _NGX_HTTP_PROXY_WASM_DISPATCH_H_INCLUDED_
#define _NGX_HTTP_PROXY_WASM_DISPATCH_H_INCLUDED_


#include <ngx_proxy_wasm.h>
#include <ngx_http_wasm.h>
#include <ngx_wasm_socket_tcp.h>


typedef enum {
    NGX_HTTP_PROXY_WASM_DISPATCH_START = 0,
    NGX_HTTP_PROXY_WASM_DISPATCH_WRITE,
    NGX_HTTP_PROXY_WASM_DISPATCH_WRITING,
    NGX_HTTP_PROXY_WASM_DISPATCH_READ,
    NGX_HTTP_PROXY_WASM_DISPATCH_RECEIVED,
} ngx_http_proxy_wasm_dispatch_state_e;


typedef struct {
    uint32_t                                callout_id;
    ngx_str_t                               host;
    ngx_msec_t                              timeout;
    ngx_str_t                               method;
    ngx_str_t                               uri;
    ngx_str_t                               authority;
    ngx_array_t                             headers;
    ngx_array_t                             trailers;
    ngx_wasm_socket_tcp_t                   sock;
    ngx_wasm_http_reader_ctx_t              http_reader;
    ngx_http_proxy_wasm_dispatch_state_e    state;
    ngx_proxy_wasm_filter_ctx_t            *fctx;
    ngx_http_wasm_req_ctx_t                *rctx;
    ngx_http_request_t                     *r;
} ngx_http_proxy_wasm_dispatch_t;


ngx_int_t ngx_http_proxy_wasm_dispatch(ngx_http_proxy_wasm_dispatch_t *call);


#endif /* _NGX_HTTP_PROXY_WASM_DISPATCH_H_INCLUDED_ */
