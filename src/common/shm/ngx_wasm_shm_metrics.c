#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#define NGX_SHM_METRICS_MAX_BATCH_SIZE 30

#include <ngx_event.h>
#include <ngx_wasm.h>
#include <ngx_wasm_shm_metrics.h>


typedef struct {
    uint32_t                    metric_id;
    ngx_int_t                   value;
} ngx_wasm_shm_metrics_update_t;


typedef struct {
    uint32_t                    id;
    ngx_wasm_shm_metric_type_e  type;
    ngx_uint_t                  value;
} ngx_wasm_shm_metrics_metric_t;


static ngx_array_t updates_buffer;


static void
ngx_wasm_shm_metrics_flush_updates(ngx_event_t *ev)
{
    size_t                          i, nupdates;
    uint32_t                        mid;
    ngx_int_t                       rc, written;
    ngx_wasm_shm_kv_node_t         *node;
    ngx_wasm_shm_metrics_t         *metrics = ev->data;
    ngx_wasm_shm_metrics_metric_t  *metric;
    ngx_wasm_shm_metrics_update_t  *updates = updates_buffer.elts;

    nupdates = updates_buffer.nelts;

    ngx_wasm_assert(ev == metrics->ev);

    if (nupdates) {
        ngx_log_debug1(NGX_LOG_DEBUG_WASM, metrics->shm->log, 0,
                       "wasm metrics: flushing \"%d\" updates", nupdates);

        ngx_wasm_shm_lock(metrics->shm);

        for (i = 0; i < nupdates; i++) {
            mid = updates[i].metric_id;
            rc = ngx_wasm_shm_kv_get_node_locked(metrics->shm, mid, &node);

            if (rc != NGX_OK) {
                /* metric not found, might have been removed */
                /* log not found metric */
                continue;
            }

            metric = (ngx_wasm_shm_metrics_metric_t *) node->value.data;

            switch (metric->type) {
                case NGX_WASM_SHM_METRIC_COUNTER:
                    metric->value += updates[i].value;
                    break;

                case NGX_WASM_SHM_METRIC_GAUGE:
                    metric->value = updates[i].value;
                    break;

                default:
                    /* TODO log error */
                    break;
            }

            rc = ngx_wasm_shm_kv_set_locked(metrics->shm, &node->key.str,
                                            &node->value, node->cas, &written);
            if (rc != NGX_OK) {
                /* TODO log error */
            }
        }

        ngx_wasm_shm_unlock(metrics->shm);

        ngx_array_destroy(&updates_buffer);
        ngx_array_init(&updates_buffer, updates_buffer.pool, nupdates,
                       sizeof(ngx_wasm_shm_metrics_update_t));
    }

    ngx_add_timer(metrics->ev, metrics->flush_interval);
}


ngx_int_t
ngx_wasm_shm_metrics_init_process(ngx_wasm_shm_metrics_t *metrics)
{
    ngx_array_init(&updates_buffer, metrics->pool, 100,
                   sizeof(ngx_wasm_shm_metrics_update_t *));

    return NGX_OK;
}


ngx_int_t
ngx_wasm_shm_metrics_add_locked(ngx_wasm_shm_metrics_t *metrics, ngx_str_t *name,
    ngx_wasm_shm_metric_type_e type, uint32_t *out)
{
    uint32_t                        cas;
    ngx_int_t                       rc, written;
    ngx_str_t                       val;
    ngx_str_t                      *val_ptr;
    ngx_wasm_shm_metrics_metric_t   metric;

    metric.id = ngx_crc32_long(name->data, name->len);
    metric.type = type;
    metric.value = 0;

    written = 0;

    if (ngx_wasm_shm_kv_get_locked(metrics->shm, name, &val_ptr, &cas) == \
        NGX_OK)
    {
        *out = metric.id;
        return NGX_OK;
    }

    ngx_log_debug2(NGX_LOG_DEBUG_WASM, metrics->shm->log, 0,
                   "wasm metrics: defining metric \"%V\" as %z",
                   name,
                   metric.id);

    val.len = sizeof(ngx_wasm_shm_metrics_metric_t);
    val.data = (u_char *) &metric;

    ngx_wasm_shm_lock(metrics->shm);

    rc = ngx_wasm_shm_kv_set_locked(metrics->shm, name, &val, 0, &written);

    ngx_wasm_shm_unlock(metrics->shm);

    if (rc != NGX_OK) {
        // TODO log
        return rc;
    }

    if (!written) {
        // TODO log
        return NGX_ERROR;
    }

    if (metrics->ev == NULL) {
        metrics->ev = ngx_calloc(sizeof(ngx_event_t), ngx_cycle->log);
        if(metrics->ev == NULL) {
            return NGX_ERROR;
        }

        metrics->ev->handler = ngx_wasm_shm_metrics_flush_updates;
        metrics->ev->data = metrics;
        metrics->ev->log = ngx_cycle->log;

        ngx_add_timer(metrics->ev, metrics->flush_interval);
    }

    *out = metric.id;
    return NGX_OK;
}


ngx_int_t
ngx_wasm_shm_metrics_get(ngx_wasm_shm_metrics_t *metrics, uint32_t metric_id,
    ngx_uint_t *out)
{
    ngx_int_t                       rc;
    ngx_wasm_shm_kv_node_t         *node;
    ngx_wasm_shm_metrics_metric_t  *metric;

    rc = ngx_wasm_shm_kv_get_node_locked(metrics->shm, metric_id, &node);
    if (rc != NGX_OK) {
        return rc;
    }

    metric = (ngx_wasm_shm_metrics_metric_t *) node->value.data;
    *out = metric->id;

    return NGX_OK;
}


ngx_int_t
ngx_wasm_shm_metrics_update(ngx_wasm_shm_metrics_t *metrics,
    uint32_t metric_id, ngx_int_t n)
{
    ngx_wasm_shm_metrics_update_t  *update = ngx_array_push(&updates_buffer);

    if (update == NULL) {
        /* TODO log */
        return NGX_ERROR;
    }

    update->metric_id = metric_id;
    update->value = n;

    if (updates_buffer.nelts >= metrics->flush_size) {
        /* TODO log */
        ngx_del_timer(metrics->ev);
        ngx_post_event(metrics->ev, &ngx_posted_events);
    }

    return NGX_OK;
}


ngx_int_t
ngx_wasm_shm_metrics_delete(ngx_wasm_shm_metrics_t *metrics, uint32_t metric_id)
{
    /* TODO */
    return NGX_OK;
}
