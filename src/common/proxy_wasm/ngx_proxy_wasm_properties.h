#ifndef _NGX_PROXY_WASM_PROPERTIES_H_INCLUDED_
#define _NGX_PROXY_WASM_PROPERTIES_H_INCLUDED_

#include <ngx_proxy_wasm.h>


void ngx_proxy_wasm_properties_init(ngx_conf_t *cf);
void ngx_proxy_wasm_properties_unmarsh_path(ngx_str_t *from, u_char **to);
ngx_int_t ngx_proxy_wasm_properties_get(ngx_proxy_wasm_ctx_t *pwctx,
    ngx_str_t *path, ngx_str_t *value, ngx_str_t *err);
ngx_int_t ngx_proxy_wasm_properties_set(ngx_proxy_wasm_ctx_t *pwctx,
    ngx_str_t *path, ngx_str_t *value, ngx_str_t *err);
ngx_int_t ngx_proxy_wasm_properties_set_host(ngx_proxy_wasm_ctx_t *pwctx,
    ngx_str_t *path, ngx_str_t *value, unsigned is_const, unsigned retrieve);
#if (NGX_WASM_LUA)
ngx_int_t ngx_proxy_wasm_properties_set_ffi_handlers(
    ngx_proxy_wasm_ctx_t *pwctx,
    ngx_proxy_wasm_properties_ffi_handler_pt getter,
    ngx_proxy_wasm_properties_ffi_handler_pt setter,
    void *data);
#endif


#endif /* _NGX_PROXY_WASM_PROPERTIES_H_INCLUDED_ */
