#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_http_wasm_headers.h>
#include <ngx_http_wasm_util.h>


ngx_int_t
ngx_http_wasm_set_req_header(ngx_http_request_t *r, ngx_str_t key,
    ngx_str_t value, unsigned override)
{
    return NGX_OK;
}
