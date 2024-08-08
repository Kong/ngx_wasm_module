#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_wasm.h>
#include <ngx_wa_shm_kv.h>
#include <ngx_wa_shm_queue.h>


ngx_int_t
ngx_wa_shm_init_zone(ngx_shm_zone_t *shm_zone, void *data)
{
    ngx_wa_shm_t  *shm = shm_zone->data;

    dd("zone: %p", shm_zone->shm.addr);

    shm->shpool = (ngx_slab_pool_t *) shm_zone->shm.addr;

    return NGX_OK;
}


ngx_int_t
ngx_wa_shm_init(ngx_cycle_t *cycle)
{
    size_t                 i;
    ngx_int_t              rc;
    ngx_array_t           *shms = ngx_wasmx_shms(cycle);
    ngx_wa_shm_mapping_t  *mappings = shms->elts;
    ngx_wa_shm_t          *shm;

    for (i = 0; i < shms->nelts; i++ ) {
        shm = mappings[i].zone->data;
        shm->log = cycle->log;

        ngx_log_debug2(NGX_LOG_DEBUG_WASM, shm->log, 0,
                       "wasm \"%V\" shm: initialization (zone: %p)",
                       &shm->name, mappings[i].zone->shm.addr);

        switch (shm->type) {
        case NGX_WA_SHM_TYPE_KV:
            rc = ngx_wa_shm_kv_init(shm);
            break;
        case NGX_WA_SHM_TYPE_QUEUE:
            rc = ngx_wa_shm_queue_init(shm);
            break;
        case NGX_WA_SHM_TYPE_METRICS:
            rc = ngx_wa_metrics_shm_init(cycle);
            break;
        default:
            ngx_wa_assert(0);
            return NGX_ERROR;
        }

        if (rc != NGX_OK) {
            return rc;
        }
    }

    return NGX_OK;
}


ngx_int_t
ngx_wa_shm_init_process(ngx_cycle_t *cycle)
{
#if (NGX_DEBUG)
    size_t                 i;
    ngx_array_t           *shms = ngx_wasmx_shms(cycle);
    ngx_wa_shm_mapping_t  *mappings = shms->elts;
    ngx_wa_shm_t          *shm;

    for (i = 0; i < shms->nelts; i++ ) {
        shm = mappings[i].zone->data;

        ngx_log_debug2(NGX_LOG_DEBUG_WASM, shm->log, 0,
                       "wasm \"%V\" shm: process initialization (zone: %p)",
                       &shm->name, mappings[i].zone->shm.addr);
    }
#endif

    /* TODO: initialize inter-process notifications */

    return NGX_OK;
}


ngx_int_t
ngx_wa_shm_lookup_index(ngx_str_t *name)
{
    size_t                 i;
    ngx_array_t           *shms;
    ngx_cycle_t           *cycle = (ngx_cycle_t *) ngx_cycle;
    ngx_wa_shm_mapping_t  *elements;

    shms = ngx_wasmx_shms(cycle);
    elements = shms->elts;

    for (i = 0; i < shms->nelts; i++) {
        if (ngx_str_eq(elements[i].name.data, elements[i].name.len,
                       name->data, name->len))
        {
            return i;
        }
    }

    return NGX_WA_SHM_INDEX_NOTFOUND;
}
