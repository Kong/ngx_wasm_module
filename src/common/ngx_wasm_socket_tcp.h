#ifndef _NGX_WASM_SOCKET_TCP_H_INCLUDED_
#define _NGX_WASM_SOCKET_TCP_H_INCLUDED_


#include <ngx_wasm.h>
#include <ngx_wasm_socket_tcp_readers.h>
#ifdef NGX_WASM_HTTP
#include <ngx_http_wasm.h>
#endif


#define NGX_WASM_SOCKET_TCP_DEFAULT_TIMEOUT  60000


typedef struct ngx_wasm_socket_tcp_s  ngx_wasm_socket_tcp_t;
typedef void (*ngx_wasm_socket_tcp_handler_pt)(ngx_wasm_socket_tcp_t *sock);
typedef ngx_int_t (*ngx_wasm_socket_tcp_reader_pt)(
    ngx_wasm_socket_tcp_t *sock, ssize_t bytes);
typedef void (*ngx_wasm_socket_tcp_resume_handler_pt)(
    ngx_wasm_socket_tcp_t *sock);


struct ngx_wasm_socket_tcp_s {

    /* parent */

#ifdef NGX_WASM_HTTP
    ngx_http_wasm_req_ctx_t               *rctx;
#endif
    ngx_pool_t                            *pool;
    ngx_log_t                             *log;
    void                                  *data;
    ngx_wasm_socket_tcp_resume_handler_pt  resume;

    /* upstream */

    ngx_str_t                              host;
    in_port_t                              port;

    ngx_msec_t                             read_timeout;
    ngx_msec_t                             send_timeout;
    ngx_msec_t                             connect_timeout;

    /* data */

#ifdef NGX_WASM_HTTP
    ngx_http_upstream_resolved_t          *resolved;
#endif
    u_char                                *err;
    size_t                                 errlen;
    ngx_err_t                              socket_errno;
    ngx_url_t                              url;
    ngx_peer_connection_t                  peer;

    ngx_wasm_socket_tcp_reader_pt          reader;
    void                                  *reader_ctx;

    ngx_wasm_socket_tcp_handler_pt         read_event_handler;
    ngx_wasm_socket_tcp_handler_pt         write_event_handler;

    ngx_chain_t                           *bufs_in;  /* input data buffers */
    ngx_chain_t                           *buf_in;   /* last input data buffer */
    ngx_buf_t                              buffer;   /* receive buffer */

    /* flags */

    unsigned                               timedout:1;
    unsigned                               connected:1;
    unsigned                               eof:1;
    unsigned                               closed:1;
};


ngx_wasm_socket_tcp_t *ngx_wasm_socket_tcp_new(ngx_pool_t *pool,
    ngx_log_t *log);
ngx_int_t ngx_wasm_socket_tcp_init(ngx_wasm_socket_tcp_t *sock,
    ngx_str_t *host);
ngx_int_t ngx_wasm_socket_tcp_connect(ngx_wasm_socket_tcp_t *sock,
    ngx_http_wasm_req_ctx_t *rctx);
ngx_int_t ngx_wasm_socket_tcp_send(ngx_wasm_socket_tcp_t *sock,
    ngx_chain_t *cl);
ngx_int_t ngx_wasm_socket_tcp_read(ngx_wasm_socket_tcp_t *sock,
    ngx_wasm_socket_tcp_reader_pt reader, void *reader_ctx);
ngx_int_t ngx_wasm_socket_reader_read_all(ngx_wasm_socket_tcp_t *sock,
    ssize_t bytes);
ngx_int_t ngx_wasm_socket_reader_read_line(ngx_wasm_socket_tcp_t *sock,
    ssize_t bytes);
#ifdef NGX_WASM_HTTP
ngx_int_t ngx_wasm_socket_read_http_response(ngx_wasm_socket_tcp_t *sock,
    ssize_t bytes);
#endif
void ngx_wasm_socket_tcp_close(ngx_wasm_socket_tcp_t *sock);
void ngx_wasm_socket_tcp_destroy(ngx_wasm_socket_tcp_t *sock);


#endif /* _NGX_WASM_SOCKET_TCP_H_INCLUDED_ */
