#ifndef _NGX_WASM_SHM_KV_H_INCLUDED_
#define _NGX_WASM_SHM_KV_H_INCLUDED_


#include <ngx_wasm_shm.h>


ngx_int_t ngx_wasm_shm_kv_init(ngx_wasm_shm_t *shm);
ngx_int_t ngx_wasm_shm_kv_get_locked(ngx_wasm_shm_t *shm,
    ngx_str_t *key, ngx_str_t **value_out, uint32_t *cas);
ngx_int_t ngx_wasm_shm_kv_set_locked(ngx_wasm_shm_t *shm,
    ngx_str_t *key, ngx_str_t *value, uint32_t cas, ngx_int_t *written);


#endif
