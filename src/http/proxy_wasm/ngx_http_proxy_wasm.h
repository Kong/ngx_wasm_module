#ifndef _NGX_HTTP_PROXY_WASM_H_INCLUDED_
#define _NGX_HTTP_PROXY_WASM_H_INCLUDED_


#include <ngx_proxy_wasm.h>
#include <ngx_http_wasm.h>
#include <ngx_http_proxy_wasm_dispatch.h>


void ngx_http_proxy_wasm_on_request_body_handler(ngx_http_request_t *r);


static ngx_inline ngx_http_wasm_req_ctx_t *
ngx_http_proxy_wasm_get_rctx(ngx_wavm_instance_t *instance)
{
    ngx_proxy_wasm_exec_t    *pwexec;
    ngx_proxy_wasm_ctx_t     *pwctx;
    ngx_http_wasm_req_ctx_t  *rctx;

    pwexec = ngx_proxy_wasm_instance2pwexec(instance);
    pwctx = pwexec->parent;
    if (!pwctx) {
        return NULL;
    }

    rctx = (ngx_http_wasm_req_ctx_t *) pwctx->data;

    return rctx;
}


static ngx_inline ngx_http_request_t *
ngx_http_proxy_wasm_get_req(ngx_wavm_instance_t *instance)
{
    ngx_http_wasm_req_ctx_t  *rctx;

    rctx = ngx_http_proxy_wasm_get_rctx(instance);

    return rctx->r;
}


extern ngx_proxy_wasm_subsystem_t  ngx_http_proxy_wasm;


#endif /* _NGX_HTTP_PROXY_WASM_H_INCLUDED_ */
