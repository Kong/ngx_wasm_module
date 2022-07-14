#ifndef _NGX_WASM_SOCKET_TCP_H_INCLUDED_
#define _NGX_WASM_SOCKET_TCP_H_INCLUDED_


#include <ngx_event_connect.h>
#if (NGX_SSL)
#include <ngx_event_openssl.h>
#endif
#include <ngx_wasm.h>
#include <ngx_wasm_socket_tcp_readers.h>
#if (NGX_WASM_HTTP)
#include <ngx_http_wasm.h>
#endif
#if (NGX_WASM_STREAM)
#include <ngx_stream_wasm.h>
#endif


typedef void (*ngx_wasm_socket_tcp_handler_pt)(
    ngx_wasm_socket_tcp_t *sock);
typedef ngx_int_t (*ngx_wasm_socket_tcp_reader_pt)(
    ngx_wasm_socket_tcp_t *sock, ssize_t bytes, void *ctx);
typedef ngx_int_t (*ngx_wasm_socket_tcp_resume_handler_pt)(
    ngx_wasm_socket_tcp_t *sock);


typedef struct {
    ngx_str_t                                host;
    in_port_t                                port;

    ngx_uint_t                               naddrs;
    ngx_resolver_addr_t                     *addrs;

    struct sockaddr                         *sockaddr;
    socklen_t                                socklen;
    ngx_str_t                                name;

    ngx_resolver_ctx_t                      *ctx;
} ngx_wasm_upstream_resolved_t;


typedef enum {
    NGX_WASM_SOCKET_TCP_KIND_NONE = 0,
    NGX_WASM_SOCKET_TCP_KIND_HTTP,
    NGX_WASM_SOCKET_TCP_KIND_STREAM,
} ngx_wasm_socket_tcp_env_e;


typedef struct {
    ngx_connection_t                        *connection;
    ngx_buf_tag_t                           *buf_tag;
    ngx_wasm_socket_tcp_env_e                kind;

    union {
#if (NGX_WASM_HTTP)
        ngx_http_wasm_req_ctx_t             *request;
#endif
#if (NGX_WASM_STREAM)
        ngx_stream_wasm_ctx_t               *session;
#endif
    } ctx;

} ngx_wasm_socket_tcp_env_t;


struct ngx_wasm_socket_tcp_s {
    ngx_pool_t                              *pool;
    ngx_log_t                               *log;
    ngx_wasm_socket_tcp_env_t                env;
    ngx_wasm_socket_tcp_env_e                kind;

    ngx_wasm_socket_tcp_resume_handler_pt    resume;
    void                                    *data;

    ngx_str_t                                host;
    ngx_wasm_upstream_resolved_t             resolved;
    in_port_t                                port;
    ngx_msec_t                               read_timeout;
    ngx_msec_t                               send_timeout;
    ngx_msec_t                               connect_timeout;
    size_t                                   buffer_size;
    ngx_flag_t                               buffer_reuse;
    ngx_chain_t                             *free_bufs;
    ngx_chain_t                             *busy_bufs;

    /* swap */

    u_char                                  *err;
    size_t                                   errlen;
    ngx_err_t                                socket_errno;
    ngx_url_t                                url;
    ngx_peer_connection_t                    peer;

    ngx_chain_t                             *bufs_in;  /* input data buffers */
    ngx_chain_t                             *buf_in;   /* last input data buffer */
    ngx_buf_t                                buffer;   /* receive buffer */

    ngx_wasm_socket_tcp_handler_pt           read_event_handler;
    ngx_wasm_socket_tcp_handler_pt           write_event_handler;

    /* large buffers */

    ngx_int_t                                lbusy;    /* large buffers counters in busy */
    ngx_chain_t                             *busy_large_bufs;
    ngx_chain_t                             *free_large_bufs;

    /* ssl */

#if (NGX_SSL)
    ngx_wasm_ssl_conf_t                     *ssl_conf;
#endif

    /* flags */

    unsigned                                 timedout:1;
    unsigned                                 connected:1;
    unsigned                                 eof:1;
    unsigned                                 closed:1;
    unsigned                                 read_closed:1;
    unsigned                                 write_closed:1;

#if (NGX_SSL)
    unsigned                                 ssl_ready:1;
#endif
};


ngx_int_t ngx_wasm_socket_tcp_init(ngx_wasm_socket_tcp_t *sock,
    ngx_str_t *host, ngx_wasm_socket_tcp_env_t *env,
    in_port_t default_port);
ngx_int_t ngx_wasm_socket_tcp_connect(ngx_wasm_socket_tcp_t *sock);
ngx_int_t ngx_wasm_socket_tcp_ssl_handshake(ngx_wasm_socket_tcp_t *sock);
ngx_int_t ngx_wasm_socket_tcp_send(ngx_wasm_socket_tcp_t *sock,
    ngx_chain_t *cl);
ngx_int_t ngx_wasm_socket_tcp_read(ngx_wasm_socket_tcp_t *sock,
    ngx_wasm_socket_tcp_reader_pt reader, void *reader_ctx);
void ngx_wasm_socket_tcp_close(ngx_wasm_socket_tcp_t *sock);
void ngx_wasm_socket_tcp_destroy(ngx_wasm_socket_tcp_t *sock);

#if 0
ngx_int_t ngx_wasm_socket_reader_read_all(ngx_wasm_socket_tcp_t *sock,
    ssize_t bytes);
ngx_int_t ngx_wasm_socket_reader_read_line(ngx_wasm_socket_tcp_t *sock,
    ssize_t bytes);
#endif
#ifdef NGX_WASM_HTTP
ngx_int_t ngx_wasm_socket_read_http_response(ngx_wasm_socket_tcp_t *sock,
    ssize_t bytes, void *ctx);
#endif


#endif /* _NGX_WASM_SOCKET_TCP_H_INCLUDED_ */
