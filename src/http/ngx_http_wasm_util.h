#ifndef _NGX_HTTP_WASM_UTIL_H_INCLUDED_
#define _NGX_HTTP_WASM_UTIL_H_INCLUDED_


#include <ngx_http_wasm.h>


typedef enum {
    NGX_HTTP_WASM_ESCAPE_URI = 0,
    NGX_HTTP_WASM_ESCAPE_URI_COMPONENT,
    NGX_HTTP_WASM_ESCAPE_ARGS,
    NGX_HTTP_WASM_ESCAPE_HEADER_NAME,
    NGX_HTTP_WASM_ESCAPE_HEADER_VALUE,
} ngx_http_wasm_escape_kind;


ngx_uint_t ngx_http_wasm_escape(u_char *dst, u_char *src, size_t size,
    ngx_http_wasm_escape_kind kind);
ngx_str_t *ngx_http_copy_escaped(ngx_str_t *dst, ngx_pool_t *pool,
    ngx_http_wasm_escape_kind kind);
ngx_int_t ngx_http_wasm_read_client_request_body(ngx_http_request_t *r,
    ngx_http_client_body_handler_pt post_handler);
ngx_int_t ngx_http_wasm_send_chain_link(ngx_http_request_t *r, ngx_chain_t *in);

/* proxy-wasm with wasm ops */
ngx_int_t ngx_http_wasm_ops_add_filter(ngx_wasm_ops_plan_t *plan,
    ngx_str_t *name, ngx_str_t *config, ngx_proxy_wasm_store_t *store,
    ngx_wavm_t *vm);

/* fake requests */
ngx_connection_t *ngx_http_wasm_create_fake_connection(ngx_pool_t *pool);
ngx_http_request_t *ngx_http_wasm_create_fake_request(ngx_connection_t *c);
void ngx_http_wasm_finalize_fake_request(ngx_http_request_t *r, ngx_int_t rc);


#endif /* _NGX_HTTP_WASM_UTIL_H_INCLUDED_ */
