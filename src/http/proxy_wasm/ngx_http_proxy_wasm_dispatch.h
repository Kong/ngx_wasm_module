#ifndef _NGX_HTTP_PROXY_WASM_DISPATCH_H_INCLUDED_
#define _NGX_HTTP_PROXY_WASM_DISPATCH_H_INCLUDED_


#include <ngx_proxy_wasm.h>
#include <ngx_http_wasm.h>
#include <ngx_wasm_socket_tcp.h>


typedef enum {
    NGX_HTTP_PROXY_WASM_DISPATCH_START = 0,
    NGX_HTTP_PROXY_WASM_DISPATCH_CONNECTING,
    NGX_HTTP_PROXY_WASM_DISPATCH_SENDING,
    NGX_HTTP_PROXY_WASM_DISPATCH_RECEIVING,
    NGX_HTTP_PROXY_WASM_DISPATCH_RECEIVED,
} ngx_http_proxy_wasm_dispatch_state_e;


typedef enum {
    NGX_HTTP_PROXY_WASM_DISPATCH_ERR_NONE = 0,
    NGX_HTTP_PROXY_WASM_DISPATCH_ERR_BAD_METHOD,
    NGX_HTTP_PROXY_WASM_DISPATCH_ERR_BAD_PATH,
    NGX_HTTP_PROXY_WASM_DISPATCH_ERR_BAD_AUTHORITY,
    NGX_HTTP_PROXY_WASM_DISPATCH_ERR_BAD_STEP,
    NGX_HTTP_PROXY_WASM_DISPATCH_ERR_NOMEM,
    NGX_HTTP_PROXY_WASM_DISPATCH_ERR_MARSHALLING,
    NGX_HTTP_PROXY_WASM_DISPATCH_ERR_UNKNOWN,
} ngx_http_proxy_wasm_dispatch_err_e;


struct ngx_http_proxy_wasm_dispatch_s {
    ngx_pool_t                             *pool;  /* owned */
    uint32_t                                id;
    ngx_msec_t                              timeout;
    ngx_wasm_socket_tcp_t                   sock;
    ngx_http_wasm_req_ctx_t                *rctx;
    ngx_proxy_wasm_exec_t                  *pwexec;
    ngx_proxy_wasm_instance_t              *ictx;
    ngx_http_proxy_wasm_dispatch_err_e      error;

    ngx_str_t                               host;
    ngx_str_t                               method;
    ngx_str_t                               uri;
    ngx_str_t                               authority;
    ngx_array_t                             headers;
    ngx_array_t                             trailers;

    size_t                                  req_body_len;
    ngx_chain_t                            *req_body;
    ngx_chain_t                            *req_out;

    ngx_wasm_http_reader_ctx_t              http_reader;
    ngx_http_proxy_wasm_dispatch_state_e    state;
    ngx_http_request_t                      fake_r;
};


ngx_http_proxy_wasm_dispatch_t *ngx_http_proxy_wasm_dispatch(
    ngx_proxy_wasm_exec_t *pwexec,
    ngx_http_wasm_req_ctx_t *rctx, ngx_str_t *host,
    ngx_proxy_wasm_marshalled_map_t *headers,
    ngx_proxy_wasm_marshalled_map_t *trailers,
    ngx_str_t *body, ngx_msec_t timeout);
void ngx_http_proxy_wasm_dispatch_destroy(ngx_http_proxy_wasm_dispatch_t *call);


#endif /* _NGX_HTTP_PROXY_WASM_DISPATCH_H_INCLUDED_ */
