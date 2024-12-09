#ifndef _NGX_PROXY_WASM_FOREIGN_CALLBACK_H_INCLUDED_
#define _NGX_PROXY_WASM_FOREIGN_CALLBACK_H_INCLUDED_


#include <ngx_wavm.h>
#include <ngx_wasm.h>
#include <ngx_proxy_wasm.h>
#include <ngx_wasm_subsystem.h>
#ifdef NGX_WASM_HTTP
#include <ngx_http_proxy_wasm.h>
#endif


struct ngx_proxy_wasm_foreign_call_s {
    ngx_proxy_wasm_exec_t              *pwexec;
#if (NGX_WASM_HTTP)
    ngx_http_wasm_req_ctx_t            *rctx;
#endif
    ngx_proxy_wasm_foreign_function_e   fcode;
    ngx_chain_t                        *args_out;
};


void ngx_proxy_wasm_foreign_call_destroy(ngx_proxy_wasm_foreign_call_t *call);


#if (NGX_WASM_HTTP)
ngx_int_t ngx_proxy_wasm_foreign_call_resolve_lua(ngx_wavm_instance_t *instance,
    ngx_http_wasm_req_ctx_t *rctx, ngx_str_t *fargs, ngx_wavm_ptr_t *ret_data,
    int32_t *ret_size, wasm_val_t rets[]);
#endif
#endif
