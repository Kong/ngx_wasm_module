#ifndef _NGX_WASM_SOCKET_TCP_READERS_H_INCLUDED_
#define _NGX_WASM_SOCKET_TCP_READERS_H_INCLUDED_


#include <ngx_wasm.h>
#ifdef NGX_WASM_HTTP
#include <ngx_http_wasm.h>
#endif


typedef struct ngx_wasm_socket_tcp_s  ngx_wasm_socket_tcp_t;


#ifdef NGX_WASM_HTTP
typedef struct {
    ngx_pool_t                       *pool;
    ngx_log_t                        *log;
    ngx_wasm_socket_tcp_t            *sock;
    ngx_http_wasm_req_ctx_t          *rctx;
    ngx_http_request_t                fake_r;
    ngx_http_status_t                 status;
    ngx_http_chunked_t                chunked;
    ngx_uint_t                        status_code;
    ngx_chain_t                      *chunks;
    ngx_chain_t                      *body;
    size_t                            headers_len;
    size_t                            body_len;
    size_t                            rest;
    unsigned                          header_done:1;
} ngx_wasm_http_reader_ctx_t;
#endif


#if 0
ngx_int_t ngx_wasm_read_all(ngx_buf_t *src, ngx_chain_t *buf_in,
    ssize_t bytes);
ngx_int_t ngx_wasm_read_line(ngx_buf_t *src, ngx_chain_t *buf_in,
    ssize_t bytes);
#endif
ngx_int_t ngx_wasm_read_bytes(ngx_buf_t *src, ngx_chain_t *buf_in,
    ssize_t bytes, size_t *rest);
#ifdef NGX_WASM_HTTP
ngx_int_t ngx_wasm_read_http_response(ngx_buf_t *src, ngx_chain_t *buf_in,
    ssize_t bytes, ngx_wasm_http_reader_ctx_t *in_ctx);
#endif


#endif /* _NGX_WASM_SOCKET_TCP_READERS_H_INCLUDED_ */
