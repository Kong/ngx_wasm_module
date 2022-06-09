#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"


#include <ngx_http_wasm.h>


size_t
ngx_http_wasm_resp_trailers_count(ngx_http_request_t *r)
{
    return ngx_wasm_list_nelts(&r->headers_out.trailers);
}
