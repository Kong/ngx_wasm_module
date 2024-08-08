#ifndef _NGX_WA_SHM_H_INCLUDED_
#define _NGX_WA_SHM_H_INCLUDED_


#include <ngx_core.h>


#define NGX_WA_SHM_MIN_SIZE       (3 * ngx_pagesize)
#define NGX_WA_SHM_INDEX_NOTFOUND -1


typedef enum {
    NGX_WA_SHM_TYPE_KV,
    NGX_WA_SHM_TYPE_QUEUE,
    NGX_WA_SHM_TYPE_METRICS,
} ngx_wa_shm_type_e;


typedef enum {
    NGX_WA_SHM_EVICTION_LRU,
    NGX_WA_SHM_EVICTION_SLRU,
    NGX_WA_SHM_EVICTION_NONE,
} ngx_wa_shm_eviction_e;


typedef struct {
    ngx_wa_shm_type_e       type;
    ngx_wa_shm_eviction_e   eviction;
    ngx_str_t               name;
    ngx_log_t              *log;
    ngx_slab_pool_t        *shpool;
    void                   *data;
} ngx_wa_shm_t;


typedef struct {
    ngx_str_t               name;
    ngx_shm_zone_t         *zone;
} ngx_wa_shm_mapping_t;


ngx_int_t ngx_wa_shm_init(ngx_cycle_t *cycle);
ngx_int_t ngx_wa_shm_init_zone(ngx_shm_zone_t *shm_zone, void *data);
ngx_int_t ngx_wa_shm_init_process(ngx_cycle_t *cycle);
ngx_int_t ngx_wa_shm_lookup_index(ngx_str_t *name);


static ngx_inline void
ngx_wa_shm_lock(ngx_wa_shm_t *shm)
{
    ngx_shmtx_lock(&shm->shpool->mutex);
}


static ngx_inline void
ngx_wa_shm_unlock(ngx_wa_shm_t *shm)
{
    ngx_shmtx_unlock(&shm->shpool->mutex);
}


#endif /* _NGX_WA_SHM_H_INCLUDED_ */
