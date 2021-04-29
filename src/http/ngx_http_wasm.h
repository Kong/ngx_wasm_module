#ifndef _NGX_HTTP_WASM_H_INCLUDED_
#define _NGX_HTTP_WASM_H_INCLUDED_


#include <ngx_http.h>
#include <ngx_http_wasm_util.h>
#include <ngx_http_wasm_headers.h>
#include <ngx_wasm_ops.h>


#define NGX_HTTP_WASM_MAX_REQ_HEADERS      100

#define NGX_HTTP_WASM_HEADER_FILTER_PHASE  (NGX_HTTP_LOG_PHASE + 1)


typedef struct {
    ngx_http_request_t                *r;
    ngx_wasm_op_ctx_t                  opctx;
    void                              *data;

    /* control flow */

    ngx_http_handler_pt                r_content_handler;

    ngx_int_t                          local_resp_status;
    ngx_str_t                          local_resp_reason;
    ngx_array_t                       *local_resp_headers;
    ngx_chain_t                       *local_resp_body;
    size_t                             local_resp_body_len;
    unsigned                           local_resp:1;
    unsigned                           sent_last:1;
    unsigned                           finalized:1;

    unsigned                           entered_content:1;
} ngx_http_wasm_req_ctx_t;


typedef struct {
    ngx_wavm_t                        *vm;
    ngx_wasm_ops_engine_t             *ops_engine;
} ngx_http_wasm_loc_conf_t;


typedef struct {
    ngx_queue_t                        ops_engines;
} ngx_http_wasm_main_conf_t;


ngx_int_t ngx_http_wasm_stash_local_response(ngx_http_wasm_req_ctx_t *rctx,
    ngx_int_t status, u_char *reason, size_t reason_len, ngx_array_t *headers,
    u_char *body, size_t body_len);
ngx_int_t ngx_http_wasm_flush_local_response(ngx_http_request_t *r,
    ngx_http_wasm_req_ctx_t *rctx);


/* directives */
char *ngx_http_wasm_call_directive(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
char *ngx_http_wasm_proxy_wasm_directive(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);


extern ngx_wasm_subsystem_t  ngx_http_wasm_subsystem;
extern ngx_wavm_host_def_t   ngx_http_wasm_host_interface;


#endif /* _NGX_HTTP_WASM_H_INCLUDED_ */
