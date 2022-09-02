#ifndef _NGX_WASM_SHM_H_INCLUDED_
#define _NGX_WASM_SHM_H_INCLUDED_


#include <ngx_core.h>


#define NGX_WASM_SHM_INDEX_NOTFOUND -1


typedef enum {
    NGX_WASM_SHM_TYPE_KV,
    NGX_WASM_SHM_TYPE_QUEUE,
} ngx_wasm_shm_type_e;


typedef struct {
    ngx_wasm_shm_type_e     type;
    ngx_str_t               name;
    ngx_log_t              *log;
    ngx_slab_pool_t        *shpool;
    void                   *data;
} ngx_wasm_shm_t;


typedef struct {
    ngx_str_t               name;
    ngx_shm_zone_t         *zone;
} ngx_wasm_shm_mapping_t;


ngx_array_t *ngx_wasm_core_shms(ngx_cycle_t *cycle);

ngx_int_t ngx_wasm_shm_init(ngx_cycle_t *cycle);
ngx_int_t ngx_wasm_shm_init_zone(ngx_shm_zone_t *shm_zone, void *data);
ngx_int_t ngx_wasm_shm_init_process(ngx_cycle_t *cycle);
ngx_int_t ngx_wasm_shm_lookup_index(ngx_str_t *name);


static ngx_inline void
ngx_wasm_shm_lock(ngx_wasm_shm_t *shm)
{
    ngx_shmtx_lock(&shm->shpool->mutex);
}


static ngx_inline void
ngx_wasm_shm_unlock(ngx_wasm_shm_t *shm)
{
    ngx_shmtx_unlock(&shm->shpool->mutex);
}


#endif /* _NGX_WASM_SHM_H_INCLUDED_ */
