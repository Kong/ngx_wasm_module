#ifndef _NGX_HTTP_PROXY_WASM_H_INCLUDED_
#define _NGX_HTTP_PROXY_WASM_H_INCLUDED_


#include <ngx_http_wasm.h>
#include <ngx_proxy_wasm.h>


ngx_uint_t ngx_http_proxy_wasm_ctxid(ngx_proxy_wasm_t *pwm);
ngx_uint_t ngx_http_proxy_wasm_ecode(ngx_proxy_wasm_t *pwm, ngx_uint_t ecode);

ngx_int_t ngx_http_proxy_wasm_resume(ngx_proxy_wasm_t *pwm,
    ngx_wasm_phase_t *phase, ngx_wavm_ctx_t *wvctx);


#endif /* _NGX_HTTP_PROXY_WASM_H_INCLUDED_ */
