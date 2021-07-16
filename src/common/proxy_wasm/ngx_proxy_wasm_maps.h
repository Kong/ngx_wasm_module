#ifndef _NGX_PROXY_WASM_MAPS_H_INCLUDED_
#define _NGX_PROXY_WASM_MAPS_H_INCLUDED_

#include <ngx_proxy_wasm.h>


typedef enum {
    NGX_PROXY_WASM_MAP_SET = 0,
    NGX_PROXY_WASM_MAP_ADD,
    NGX_PROXY_WASM_MAP_REMOVE,
} ngx_proxy_wasm_map_op_e;


typedef struct ngx_proxy_wasm_maps_key_s  ngx_proxy_wasm_maps_key_t;

typedef ngx_str_t * (*ngx_proxy_wasm_maps_key_get_pt)(ngx_wavm_instance_t *instance);
typedef ngx_int_t (*ngx_proxy_wasm_maps_key_set_pt)(ngx_wavm_instance_t *instance, ngx_str_t *value);


struct ngx_proxy_wasm_maps_key_s {
    ngx_str_t                             key;
    ngx_uint_t                            map_type;
    ngx_proxy_wasm_maps_key_get_pt        get_;
    ngx_proxy_wasm_maps_key_set_pt        set_;
};


ngx_list_t *ngx_proxy_wasm_maps_get_all(ngx_wavm_instance_t *instance,
    ngx_proxy_wasm_map_type_t map_type, ngx_array_t *extras);
ngx_int_t ngx_proxy_wasm_maps_set_all(ngx_wavm_instance_t *instance,
    ngx_proxy_wasm_map_type_t map_type, ngx_array_t *pairs);
ngx_str_t *ngx_proxy_wasm_maps_get(ngx_wavm_instance_t *instance,
    ngx_proxy_wasm_map_type_t map_type, ngx_str_t *key);
ngx_int_t ngx_proxy_wasm_maps_set(ngx_wavm_instance_t *instance,
    ngx_proxy_wasm_map_type_t map_type, ngx_str_t *key, ngx_str_t *value,
    ngx_uint_t map_op);


#endif /* _NGX_PROXY_WASM_MAPS_H_INCLUDED_ */
