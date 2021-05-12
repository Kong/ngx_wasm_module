#ifndef _NGX_HTTP_WASM_HEADERS_H_INCLUDED_
#define _NGX_HTTP_WASM_HEADERS_H_INCLUDED_


#include <ngx_http.h>


#define NGX_HTTP_WASM_HEADERS_HASH_SKIP  0


typedef enum {
    NGX_HTTP_WASM_HEADERS_REQUEST,
    NGX_HTTP_WASM_HEADERS_RESPONSE,
} ngx_http_wasm_headers_type_e;


typedef enum {
    NGX_HTTP_WASM_HEADERS_SET,
    NGX_HTTP_WASM_HEADERS_APPEND,
    NGX_HTTP_WASM_HEADERS_REPLACE_IF_SET,
} ngx_http_wasm_headers_set_mode_e;


typedef struct ngx_http_wasm_header_val_s  ngx_http_wasm_header_val_t;

typedef ngx_int_t (*ngx_http_wasm_header_set_pt)(ngx_http_request_t *r,
    ngx_http_wasm_header_val_t *hv, ngx_str_t *value);


struct ngx_http_wasm_header_val_s {
    ngx_str_t                                key;
    ngx_uint_t                               hash;
    ngx_uint_t                               offset;
    ngx_uint_t                               mode;
    ngx_http_wasm_header_set_pt              handler;
    ngx_list_t                              *list;
};


typedef struct {
    ngx_str_t                                name;
    ngx_uint_t                               offset;
    ngx_http_wasm_header_set_pt              handler;
} ngx_http_wasm_header_t;


size_t ngx_http_wasm_req_headers_count(ngx_http_request_t *r);
size_t ngx_http_wasm_resp_headers_count(ngx_http_request_t *r);
ngx_int_t ngx_http_wasm_set_req_header(ngx_http_request_t *r, ngx_str_t key,
    ngx_str_t value, ngx_uint_t mode);
ngx_int_t ngx_http_wasm_set_resp_header(ngx_http_request_t *r, ngx_str_t key,
    ngx_str_t value, ngx_uint_t mode);
ngx_int_t ngx_http_wasm_set_resp_content_length(ngx_http_request_t *r,
    off_t cl);

/* handlers */
ngx_int_t ngx_http_wasm_set_header(ngx_http_request_t *r,
    ngx_http_wasm_headers_type_e htype, ngx_http_wasm_header_t *handlers,
    ngx_str_t key, ngx_str_t value, ngx_uint_t mode);
ngx_int_t ngx_http_wasm_set_header_helper(ngx_http_wasm_header_val_t *hv,
    ngx_str_t *value, ngx_table_elt_t **out);
ngx_int_t ngx_http_wasm_set_header_handler(ngx_http_request_t *r,
    ngx_http_wasm_header_val_t *hv, ngx_str_t *value);
ngx_int_t ngx_http_wasm_set_builtin_header_handler(ngx_http_request_t *r,
    ngx_http_wasm_header_val_t *hv, ngx_str_t *value);


#endif /* _NGX_HTTP_WASM_HEADERS_H_INCLUDED_ */
