#ifndef _NGX_WASM_SHM_H_INCLUDED_
#define _NGX_WASM_SHM_H_INCLUDED_


#include <ngx_core.h>


#define NGX_WASM_SHM_INDEX_NOTFOUND -1


typedef enum {
    NGX_WASM_SHM_TYPE_KV = 0,
    NGX_WASM_SHM_TYPE_QUEUE = 1,
} ngx_wasm_shm_type_e;


struct ngx_wasm_shm_s {
    ngx_wasm_shm_type_e     type;
    ngx_slab_pool_t        *shpool;
    ngx_str_t               name;
    ngx_log_t              *log;
    uint32_t               *notification;
    void                   *data;
};


struct ngx_wasm_shm_mapping_s {
    ngx_str_t        name;
    ngx_shm_zone_t  *zone;
};


typedef struct ngx_wasm_shm_s  ngx_wasm_shm_t;
typedef struct ngx_wasm_shm_mapping_s  ngx_wasm_shm_mapping_t;


ngx_array_t *ngx_wasm_core_shms(ngx_cycle_t *cycle);

ngx_int_t ngx_wasm_shm_init(ngx_cycle_t *cycle);
ngx_int_t ngx_wasm_shm_init_zone(ngx_shm_zone_t *shm_zone, void *data);
ngx_int_t ngx_wasm_shm_init_process(ngx_cycle_t *cycle);
ngx_int_t ngx_wasm_shm_lookup_index(ngx_str_t *name);


#endif
