/*
 * Copyright (C) Thibault Charbonnier
 */

#ifndef _NGX_HTTP_WASM_UTIL_H_INCLUDED_
#define _NGX_HTTP_WASM_UTIL_H_INCLUDED_


#include <ngx_core.h>
#include <ngx_http.h>


ngx_int_t ngx_http_wasm_send_header(ngx_http_request_t *r);

ngx_int_t ngx_http_wasm_send_chain_link(ngx_http_request_t *r, ngx_chain_t *in);


#endif /* _NGX_HTTP_WASM_UTIL_H_INCLUDED_ */
