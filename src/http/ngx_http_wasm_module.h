#ifndef _NGX_HTTP_WASM_MODULE_H_INCLUDED_
#define _NGX_HTTP_WASM_MODULE_H_INCLUDED_


#include <ngx_http.h>
#include <ngx_wasm_vm_cache.h>
#include <ngx_wasm_phases.h>


typedef struct {
    ngx_http_request_t              *r;
    ngx_wasm_vm_cache_t             *vmcache;
    ngx_wasm_phases_ctx_t      pctx;

    /* control flow */

    unsigned                         sent_last:1;
} ngx_http_wasm_req_ctx_t;


#endif /* _NGX_HTTP_WASM_MODULE_H_INCLUDED_ */
