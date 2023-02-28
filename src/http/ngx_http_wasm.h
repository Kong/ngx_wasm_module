#ifndef _NGX_HTTP_WASM_H_INCLUDED_
#define _NGX_HTTP_WASM_H_INCLUDED_


#include <ngx_wasm_ops.h>
#include <ngx_proxy_wasm.h>
#include <ngx_http.h>
#include <ngx_http_wasm_util.h>
#include <ngx_http_wasm_headers.h>
#ifdef NGX_WASM_RESPONSE_TRAILERS
#include <ngx_http_wasm_trailers.h>
#endif


#define NGX_HTTP_WASM_MAX_REQ_HEADERS      100

#define NGX_HTTP_WASM_HEADER_FILTER_PHASE  (NGX_HTTP_LOG_PHASE + 1)
#define NGX_HTTP_WASM_BODY_FILTER_PHASE    (NGX_HTTP_LOG_PHASE + 2)
#ifdef NGX_WASM_RESPONSE_TRAILERS
#define NGX_HTTP_WASM_TRAILER_FILTER_PHASE (NGX_HTTP_LOG_PHASE + 3)
#endif

#define ngx_http_wasm_req_yielded(rctx)                                      \
    (rctx->state == NGX_HTTP_WASM_REQ_STATE_YIELD)


typedef enum {
    NGX_HTTP_WASM_REQ_STATE_ERROR = -1,
    NGX_HTTP_WASM_REQ_STATE_CONTINUE = 0,
    NGX_HTTP_WASM_REQ_STATE_YIELD,
} ngx_http_wasm_req_state_e;


typedef struct {
    ngx_http_request_t                *r;
    ngx_connection_t                  *connection;
    ngx_pool_t                        *pool;                    /* r->pool */
    ngx_wasm_ops_t                    *ffi_engine;
    ngx_wasm_op_ctx_t                  opctx;
    void                              *data;                    /* per-stream extra context */
    ngx_http_wasm_req_state_e          state;                   /* determines next step on resume */

    ngx_chain_t                       *free_bufs;
    ngx_chain_t                       *busy_bufs;

    ngx_http_handler_pt                r_content_handler;
    ngx_array_t                        resp_shim_headers;
    ngx_chain_t                       *resp_chunk;
    off_t                              resp_chunk_len;
    unsigned                           resp_chunk_eof;          /* seen last buf flag */

    off_t                              req_content_length_n;
    off_t                              resp_content_length_n;

    /* local resp */

    ngx_int_t                          local_resp_status;
    ngx_str_t                          local_resp_reason;
    ngx_array_t                        local_resp_headers;
    ngx_chain_t                       *local_resp_body;
    off_t                              local_resp_body_len;

    /* flags */

    unsigned                           sock_buffer_reuse:1;     /* convenience alias to loc->socket_buffer_reuse */
    unsigned                           req_keepalive:1;         /* r->keepalive copy */
    unsigned                           reset_resp_shims:1;
    unsigned                           entered_content_phase:1; /* entered content handler */
    unsigned                           exited_content_phase:1;  /* executed content handler at least once */
    unsigned                           entered_header_filter:1; /* entered header_filter handler */

    unsigned                           resp_content_chosen:1;   /* content handler has an output to produce */
    unsigned                           resp_content_sent:1;     /* has started sending output (may have yielded) */
    unsigned                           resp_finalized:1;        /* finalized connection (ourselves) */
    unsigned                           ffi_attached:1;
    unsigned                           fake_request:1;
} ngx_http_wasm_req_ctx_t;


typedef struct {
    ngx_uint_t                         isolation;
    ngx_wasm_ops_plan_t               *plan;

    ngx_msec_t                         connect_timeout;
    ngx_msec_t                         send_timeout;
    ngx_msec_t                         recv_timeout;

    size_t                             socket_buffer_size;     /* wasm_socket_buffer_size */
    ngx_bufs_t                         socket_large_buffers;   /* wasm_socket_large_buffer_size */
    ngx_flag_t                         socket_buffer_reuse;    /* wasm_socket_buffer_reuse */

    ngx_flag_t                         pwm_req_headers_in_access;

    ngx_queue_t                        q;                      /* main_conf */
} ngx_http_wasm_loc_conf_t;


typedef struct {
    ngx_proxy_wasm_store_t             store;
    ngx_queue_t                        plans;
    ngx_wasm_ops_t                    *ops;
    ngx_wavm_t                        *vm;
} ngx_http_wasm_main_conf_t;


/* http */
ngx_int_t ngx_http_wasm_rctx(ngx_http_request_t *r,
    ngx_http_wasm_req_ctx_t **out);
ngx_int_t ngx_http_wasm_stash_local_response(ngx_http_wasm_req_ctx_t *rctx,
    ngx_int_t status, u_char *reason, size_t reason_len, ngx_array_t *headers,
    u_char *body, size_t body_len);
void ngx_http_wasm_discard_local_response(ngx_http_wasm_req_ctx_t *rctx);
ngx_int_t ngx_http_wasm_flush_local_response(ngx_http_wasm_req_ctx_t *rctx);
ngx_int_t ngx_http_wasm_produce_resp_headers(ngx_http_wasm_req_ctx_t *rctx);
void ngx_http_wasm_resume(ngx_http_wasm_req_ctx_t *rctx, unsigned main,
    unsigned wev);
void ngx_http_wasm_content_wev_handler(ngx_http_request_t *r);


/* directives */
char *ngx_http_wasm_call_directive(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
char *ngx_http_wasm_proxy_wasm_directive(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
char *ngx_http_wasm_proxy_wasm_isolation_directive(ngx_conf_t *cf,
    ngx_command_t *cmd, void *conf);
char *ngx_http_wasm_resolver_add_directive(ngx_conf_t *cf,
    ngx_command_t *cmd, void *conf);


/* shim headers */
typedef ngx_str_t * (*ngx_http_wasm_shim_header_handler_pt)(
    ngx_http_wasm_req_ctx_t *rctx);

typedef struct {
    ngx_str_t                             key;
    ngx_http_wasm_shim_header_handler_pt  handler;
} ngx_http_wasm_shim_header_t;


ngx_str_t *ngx_http_wasm_get_shim_header(ngx_http_wasm_req_ctx_t *rctx,
    u_char *key, size_t key_len);
ngx_array_t *ngx_http_wasm_get_shim_headers(ngx_http_wasm_req_ctx_t *rctx);
ngx_uint_t ngx_http_wasm_count_shim_headers(ngx_http_wasm_req_ctx_t *rctx);


/* payloads */
ngx_int_t ngx_http_wasm_set_req_body(ngx_http_wasm_req_ctx_t *rctx,
    ngx_str_t *body, size_t at, size_t max);
ngx_int_t ngx_http_wasm_set_resp_body(ngx_http_wasm_req_ctx_t *rctx,
    ngx_str_t *body, size_t at, size_t max);
ngx_int_t ngx_http_wasm_prepend_req_body(ngx_http_wasm_req_ctx_t *rctx,
    ngx_str_t *body);
ngx_int_t ngx_http_wasm_prepend_resp_body(ngx_http_wasm_req_ctx_t *rctx,
    ngx_str_t *body);


extern ngx_wasm_subsystem_t  ngx_http_wasm_subsystem;
extern ngx_wavm_host_def_t   ngx_http_wasm_host_interface;
extern ngx_module_t          ngx_http_wasm_module;

static const ngx_buf_tag_t   buf_tag = &ngx_http_wasm_module;


#endif /* _NGX_HTTP_WASM_H_INCLUDED_ */
