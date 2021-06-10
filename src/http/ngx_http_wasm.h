#ifndef _NGX_HTTP_WASM_H_INCLUDED_
#define _NGX_HTTP_WASM_H_INCLUDED_


#include <ngx_http.h>
#include <ngx_http_wasm_util.h>
#include <ngx_http_wasm_headers.h>
#include <ngx_wasm_ops.h>


#define NGX_HTTP_WASM_MAX_REQ_HEADERS      100

#define NGX_HTTP_WASM_HEADER_FILTER_PHASE  (NGX_HTTP_LOG_PHASE + 1)
#define NGX_HTTP_WASM_BODY_FILTER_PHASE    (NGX_HTTP_LOG_PHASE + 2)


typedef struct {
    ngx_http_request_t                *r;
    ngx_wasm_op_ctx_t                  opctx;
    void                              *data;

    ngx_chain_t                       *resp_body_out;

    ngx_chain_t                       *free;
    ngx_chain_t                       *busy;

    /* control flow */

    ngx_http_handler_pt                r_content_handler;
    ngx_array_t                        resp_shim_headers;
    unsigned                           reset_resp_shims;

    /* local resp */

    ngx_int_t                          local_resp_status;
    ngx_str_t                          local_resp_reason;
    ngx_array_t                       *local_resp_headers;
    ngx_chain_t                       *local_resp_body;
    off_t                              local_resp_body_len;
    unsigned                           local_resp_stashed:1;
    unsigned                           local_resp_over:1;

    /* flags */

    unsigned                           req_keepalive:1;        /* r->keepalive copy */
    unsigned                           header_filter:1;        /* entered header_filter */
    unsigned                           sent_last:1;            /* sent last byte (ourselves) */
    unsigned                           finalized:1;            /* finalized connection (ourselves) */
} ngx_http_wasm_req_ctx_t;


typedef struct {
    ngx_wavm_t                        *vm;
    ngx_wasm_ops_engine_t             *ops_engine;
} ngx_http_wasm_loc_conf_t;


typedef struct {
    ngx_queue_t                        ops_engines;
} ngx_http_wasm_main_conf_t;


ngx_int_t ngx_http_wasm_rctx(ngx_http_request_t *r,
    ngx_http_wasm_req_ctx_t **out);
ngx_int_t ngx_http_wasm_stash_local_response(ngx_http_wasm_req_ctx_t *rctx,
    ngx_int_t status, u_char *reason, size_t reason_len, ngx_array_t *headers,
    u_char *body, size_t body_len);
ngx_int_t ngx_http_wasm_flush_local_response(ngx_http_request_t *r,
    ngx_http_wasm_req_ctx_t *rctx);
ngx_int_t ngx_http_wasm_produce_resp_headers(ngx_http_wasm_req_ctx_t *rctx);
ngx_int_t ngx_http_wasm_check_finalize(ngx_http_request_t *r,
    ngx_http_wasm_req_ctx_t *rctx, ngx_int_t rc);


/* directives */
char *ngx_http_wasm_call_directive(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
char *ngx_http_wasm_proxy_wasm_directive(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);


/* shims */
typedef struct ngx_http_wasm_shim_header_s  ngx_http_wasm_shim_header_t;

typedef ngx_str_t * (*ngx_http_wasm_shim_header_handler_pt)(
    ngx_http_wasm_req_ctx_t *rctx);

struct ngx_http_wasm_shim_header_s {
    ngx_str_t                             key;
    ngx_http_wasm_shim_header_handler_pt  handler;
};


ngx_str_t *ngx_http_wasm_get_shim_header(ngx_http_wasm_req_ctx_t *rctx, u_char *key,
    size_t key_len);
ngx_array_t *ngx_http_wasm_get_shim_headers(ngx_http_wasm_req_ctx_t *rctx);
ngx_uint_t ngx_http_wasm_count_shim_headers(ngx_http_wasm_req_ctx_t *rctx);


/* payloads */
ngx_int_t ngx_http_wasm_set_req_body(ngx_http_request_t *r, ngx_str_t *body,
    size_t at, size_t max);
ngx_int_t ngx_http_wasm_set_resp_body(ngx_http_request_t *r, ngx_str_t *body);


extern ngx_wasm_subsystem_t  ngx_http_wasm_subsystem;
extern ngx_wavm_host_def_t   ngx_http_wasm_host_interface;
extern ngx_module_t          ngx_http_wasm_module;


#endif /* _NGX_HTTP_WASM_H_INCLUDED_ */
