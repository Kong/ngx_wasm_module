#ifndef _NGX_PROXY_WASM_PROPERTIES_H_INCLUDED_
#define _NGX_PROXY_WASM_PROPERTIES_H_INCLUDED_

#include <ngx_proxy_wasm.h>


void ngx_proxy_wasm_properties_init(ngx_conf_t *cf);
ngx_int_t ngx_proxy_wasm_properties_get(ngx_wavm_instance_t *instance,
    ngx_str_t *path, ngx_str_t *value);
ngx_int_t ngx_proxy_wasm_properties_set(ngx_wavm_instance_t *instance,
    ngx_str_t *path, ngx_str_t *value);


#endif /* _NGX_PROXY_WASM_PROPERTIES_H_INCLUDED_ */
