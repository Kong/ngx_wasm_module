#ifndef _NGX_WASM_METRICS_H_INCLUDED_
#define _NGX_WASM_METRICS_H_INCLUDED_


#include <ngx_core.h>
#include <ngx_wrt.h>
#include <ngx_wasm_shm_kv.h>


typedef enum {
    NGX_WASM_SHM_METRIC_COUNTER,
    NGX_WASM_SHM_METRIC_GAUGE,
} ngx_wasm_shm_metric_type_e;

typedef struct {
    ngx_pool_t      *pool;
    ngx_shm_zone_t  *shm_zone;
    ngx_wasm_shm_t  *shm;
    ngx_uint_t       flush_interval;
    ngx_uint_t       flush_size;

    ngx_event_t     *ev;
} ngx_wasm_shm_metrics_t;


ngx_wasm_shm_metrics_t *ngx_wasm_core_metrics(ngx_cycle_t *cycle);


ngx_int_t ngx_wasm_shm_metrics_init_process(ngx_wasm_shm_metrics_t *metrics);
ngx_int_t ngx_wasm_shm_metrics_add_locked(ngx_wasm_shm_metrics_t *metrics,
    ngx_str_t *name, ngx_wasm_shm_metric_type_e type, uint32_t *out);
ngx_int_t ngx_wasm_shm_metrics_delete(ngx_wasm_shm_metrics_t *metrics,
    uint32_t metric_id);
ngx_int_t ngx_wasm_shm_metrics_update(ngx_wasm_shm_metrics_t *metrics,
    uint32_t metric_id, ngx_int_t val);
ngx_int_t ngx_wasm_shm_metrics_get(ngx_wasm_shm_metrics_t *metrics,
    uint32_t metric_id, ngx_uint_t *out);

#endif /* _NGX_WASM_METRICS_H_INCLUDED_ */
