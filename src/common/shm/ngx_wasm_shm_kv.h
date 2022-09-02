#ifndef _NGX_WASM_SHM_KV_H_INCLUDED_
#define _NGX_WASM_SHM_KV_H_INCLUDED_


#include <ngx_wasm_shm.h>


typedef struct {
    ngx_str_t           namespace;
    ngx_str_t           key;
    ngx_shm_zone_t     *zone;
    ngx_wasm_shm_t     *shm;
} ngx_wasm_shm_kv_key_t;


ngx_int_t ngx_wasm_shm_kv_init(ngx_wasm_shm_t *shm);
ngx_int_t ngx_wasm_shm_kv_get_locked(ngx_wasm_shm_t *shm,
    ngx_str_t *key, ngx_str_t **value_out, uint32_t *cas);
ngx_int_t ngx_wasm_shm_kv_set_locked(ngx_wasm_shm_t *shm,
    ngx_str_t *key, ngx_str_t *value, uint32_t cas, ngx_int_t *written);
ngx_int_t ngx_wasm_shm_kv_resolve_key(ngx_str_t *key,
    ngx_wasm_shm_kv_key_t *out);


#endif /* _NGX_WASM_SHM_KV_H_INCLUDED_ */
