#ifndef _NGX_WASM_SHM_KV_H_INCLUDED_
#define _NGX_WASM_SHM_KV_H_INCLUDED_


#include <ngx_wasm_shm.h>


typedef struct {
    ngx_rbtree_t        rbtree;
    ngx_rbtree_node_t   sentinel;
    union {
        ngx_queue_t     lru_queue;
        ngx_queue_t     slru_queues[0];
    } eviction;
} ngx_wasm_shm_kv_t;


typedef struct {
    ngx_str_t           namespace;
    ngx_str_t           key;
    ngx_shm_zone_t     *zone;
    ngx_wasm_shm_t     *shm;
} ngx_wasm_shm_kv_key_t;


typedef struct {
    ngx_str_node_t  key;
    ngx_str_t       value;
    uint32_t        cas;
    ngx_queue_t     queue;
} ngx_wasm_shm_kv_node_t;


ngx_wasm_shm_kv_t * ngx_wasm_shm_get_kv(ngx_wasm_shm_t *shm);

ngx_int_t ngx_wasm_shm_kv_init(ngx_wasm_shm_t *shm);
ngx_int_t ngx_wasm_shm_kv_get_locked(ngx_wasm_shm_t *shm,
    ngx_str_t *key, uint32_t *key_hash, ngx_str_t **value_out, uint32_t *cas);
ngx_int_t ngx_wasm_shm_kv_set_locked(ngx_wasm_shm_t *shm,
    ngx_str_t *key, ngx_str_t *value, uint32_t cas, ngx_int_t *written);
ngx_int_t ngx_wasm_shm_kv_resolve_key(ngx_str_t *key,
    ngx_wasm_shm_kv_key_t *out);


#endif /* _NGX_WASM_SHM_KV_H_INCLUDED_ */
