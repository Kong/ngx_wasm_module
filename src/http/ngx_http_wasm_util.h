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


uintptr_t ngx_http_wasm_escape(u_char *dst, u_char *src, size_t size,
    ngx_http_wasm_escape_kind kind);
ngx_str_t *ngx_http_copy_escaped(ngx_str_t *dst, ngx_pool_t *pool,
    ngx_http_wasm_escape_kind kind);
ngx_int_t ngx_http_wasm_send_chain_link(ngx_http_request_t *r, ngx_chain_t *in);


#endif /* _NGX_HTTP_WASM_UTIL_H_INCLUDED_ */
