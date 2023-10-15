#ifndef _NGX_PROXY_WASM_PROPERTIES_H_INCLUDED_
#define _NGX_PROXY_WASM_PROPERTIES_H_INCLUDED_

#include <ngx_proxy_wasm.h>


void ngx_proxy_wasm_properties_init(ngx_conf_t *cf);
ngx_int_t ngx_proxy_wasm_properties_get(ngx_proxy_wasm_ctx_t *pwctx,
    ngx_str_t *path, ngx_str_t *value);
ngx_int_t ngx_proxy_wasm_properties_set(ngx_proxy_wasm_ctx_t *pwctx,
    ngx_str_t *path, ngx_str_t *value);
ngx_int_t ngx_proxy_wasm_properties_set_host(ngx_proxy_wasm_ctx_t *pwctx,
    ngx_str_t *path, ngx_str_t *value, unsigned is_const, unsigned retrieve);
ngx_int_t ngx_proxy_wasm_properties_set_host_prop_setter(
    ngx_proxy_wasm_ctx_t *pwctx, ngx_wasm_host_prop_fn_t fn, void *data);
ngx_int_t ngx_proxy_wasm_properties_set_host_prop_getter(
    ngx_proxy_wasm_ctx_t *pwctx, ngx_wasm_host_prop_fn_t fn, void *data);


#endif /* _NGX_PROXY_WASM_PROPERTIES_H_INCLUDED_ */
