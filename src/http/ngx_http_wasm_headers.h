#ifndef _NGX_HTTP_WASM_HEADERS_H_INCLUDED_
#define _NGX_HTTP_WASM_HEADERS_H_INCLUDED_


#include <ngx_http.h>


#define NGX_HTTP_WASM_HEADERS_HASH_SKIP  0


typedef struct ngx_http_wasm_header_val_s  ngx_http_wasm_header_val_t;

typedef ngx_int_t (*ngx_http_wasm_header_set_pt)(ngx_http_request_t *r,
    ngx_http_wasm_header_val_t *hv, ngx_str_t *value);


struct ngx_http_wasm_header_val_s {
    ngx_http_complex_value_t                 value;
    ngx_uint_t                               hash;
    ngx_str_t                                key;
    ngx_http_wasm_header_set_pt              handler;
    ngx_uint_t                               offset;
    unsigned                                 no_override;
};


typedef struct {
    ngx_str_t                                name;
    ngx_uint_t                               offset;
    ngx_http_wasm_header_set_pt              handler;
} ngx_http_wasm_header_t;


ngx_int_t ngx_http_wasm_set_req_header(ngx_http_request_t *r, ngx_str_t key,
    ngx_str_t value, unsigned override);
ngx_int_t ngx_http_wasm_set_resp_header(ngx_http_request_t *r, ngx_str_t key,
    ngx_str_t value, unsigned override);
ngx_int_t ngx_http_wasm_set_resp_content_length(ngx_http_request_t *r,
    off_t cl);


#endif /* _NGX_HTTP_WASM_HEADERS_H_INCLUDED_ */
