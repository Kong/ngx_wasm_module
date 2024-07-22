#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_wasm.h>
#include <ngx_wa_metrics.h>


ngx_str_t *
ngx_wa_metric_type_name(ngx_wa_metric_type_e type)
{
    static ngx_str_t  counter = ngx_string("counter");
    static ngx_str_t  gauge = ngx_string("gauge");
    static ngx_str_t  histogram = ngx_string("histogram");
    static ngx_str_t  unknown = ngx_string("unknown");

    switch (type) {
    case NGX_WA_METRIC_COUNTER:
        return &counter;

    case NGX_WA_METRIC_GAUGE:
        return &gauge;

    case NGX_WA_METRIC_HISTOGRAM:
        return &histogram;

    default:
        return &unknown;
    }
}


static ngx_uint_t
get_counter(ngx_wa_metric_t *m, ngx_uint_t slots)
{
    ngx_uint_t  i, val = 0;

    for (i = 0; i < slots; i++) {
        val += m->slots[i].counter;
    }

    return val;
}


static ngx_uint_t
get_gauge(ngx_wa_metric_t *m, ngx_uint_t slots)
{
    ngx_msec_t  l;
    ngx_uint_t  i, val = 0;

    val = m->slots[0].gauge.value;
    l = m->slots[0].gauge.last_update;

    for (i = 1; i < slots; i++) {
        if (m->slots[i].gauge.last_update > l) {
            val = m->slots[i].gauge.value;
            l = m->slots[i].gauge.last_update;
        }
    }

    return val;
}


static ngx_int_t
realloc_histogram(ngx_wa_metrics_t *metrics, ngx_wa_metric_t *old_m,
    uint32_t mid)
{
    uint32_t          cas, slots = metrics->old_metrics->workers;
    ngx_int_t         rc;
    ngx_str_t        *val;
    ngx_wa_metric_t  *m;

    rc = ngx_wasm_shm_kv_get_locked(metrics->shm, NULL, &mid, &val, &cas);
    if (rc != NGX_OK) {
        return rc;
    }

    m = (ngx_wa_metric_t *) val->data;

    ngx_wa_metrics_histogram_get(metrics, old_m, slots, m->slots[0].histogram);

    return NGX_OK;
}


static ngx_int_t
realloc_metrics(ngx_wa_metrics_t *metrics, ngx_rbtree_node_t *node,
    ngx_rbtree_node_t *sentinel)
{
    uint32_t                 mid;
    ngx_int_t                rc;
    ngx_uint_t               val;
    ngx_wasm_shm_kv_node_t  *n = (ngx_wasm_shm_kv_node_t *) node;
    ngx_wa_metric_t         *m = (ngx_wa_metric_t *) n->value.data;

    if (node == sentinel) {
        return NGX_OK;
    }

    ngx_log_debug1(NGX_LOG_DEBUG_WASM, metrics->shm->log, 0,
                   "reallocating metric \"%V\"", &n->key.str);

    if (ngx_wa_metrics_define(metrics, &n->key.str, m->type, &mid) != NGX_OK) {
        ngx_wasm_log_error(NGX_LOG_ERR, metrics->shm->log, 0,
                           "failed redefining metric \"%V\"",
                           &n->key.str);
        return NGX_ERROR;
    }

    switch (m->type) {
    case NGX_WA_METRIC_COUNTER:
        val = get_counter(m, metrics->old_metrics->workers);
        rc = ngx_wa_metrics_increment(metrics, mid, val);
        break;

    case NGX_WA_METRIC_GAUGE:
        val = get_gauge(m, metrics->old_metrics->workers);
        rc = ngx_wa_metrics_record(metrics, mid, val);
        break;

    case NGX_WA_METRIC_HISTOGRAM:
        rc = realloc_histogram(metrics, m, mid);
        break;

    default:
        ngx_wa_assert(0);
        return NGX_ERROR;
    }

    if (rc != NGX_OK) {
        ngx_wasm_log_error(NGX_LOG_ERR, metrics->shm->log, 0,
                           "failed updating metric \"%V\"", &n->key.str);
        return NGX_ERROR;
    }

    if (node->left
        && realloc_metrics(metrics, node->left, sentinel) != NGX_OK)
    {
        return NGX_ERROR;
    }

    if (node->right
        && realloc_metrics(metrics, node->right, sentinel) != NGX_OK)
    {
        return NGX_ERROR;
    }

    return NGX_OK;
}


ngx_wa_metrics_t *
ngx_wa_metrics_alloc(ngx_cycle_t *cycle)
{
    static ngx_str_t   shm_name = ngx_string("metrics");
    ngx_wa_metrics_t  *metrics;

    metrics = ngx_pcalloc(cycle->pool, sizeof(ngx_wa_metrics_t));
    if (metrics == NULL) {
        return NULL;
    }

    metrics->old_metrics = ngx_wasmx_metrics(cycle->old_cycle);
    metrics->config.slab_size = NGX_CONF_UNSET_SIZE;
    metrics->config.max_metric_name_length = NGX_CONF_UNSET_SIZE;

    metrics->shm = ngx_pcalloc(cycle->pool, sizeof(ngx_wasm_shm_t));
    if (metrics->shm == NULL) {
        ngx_pfree(cycle->pool, metrics);
        return NULL;
    }

    metrics->shm->log = &cycle->new_log;
    metrics->shm->name = shm_name;
    metrics->shm->type = NGX_WASM_SHM_TYPE_METRICS;
    metrics->shm->eviction = NGX_WASM_SHM_EVICTION_NONE;

    return metrics;
}


char *
ngx_wa_metrics_init_conf(ngx_wa_metrics_t *metrics, ngx_conf_t *cf)
{
    ngx_cycle_t           *cycle = cf->cycle;
    ngx_wa_metrics_t      *old_metrics = metrics->old_metrics;
    ngx_core_conf_t       *ccf;
    ngx_wasm_core_conf_t  *wcf = ngx_wasm_core_cycle_get_conf(cycle);

    ccf = (ngx_core_conf_t *) ngx_get_conf(cycle->conf_ctx, ngx_core_module);

    if (metrics->config.slab_size == NGX_CONF_UNSET_SIZE) {
        metrics->config.slab_size = NGX_WA_METRICS_DEFAULT_SLAB_SIZE;
    }

    if (metrics->config.max_metric_name_length == NGX_CONF_UNSET_SIZE) {
        metrics->config.max_metric_name_length =
            NGX_WA_METRICS_DEFAULT_MAX_NAME_LEN;
    }

    /* TODO: if eviction is enabled, metrics->workers must be set to 1 */
    metrics->workers = ccf->worker_processes;

    metrics->mapping = ngx_array_push(&wcf->shms);
    if (metrics->mapping == NULL) {
        return NULL;
    }

    metrics->mapping->name = metrics->shm->name;
    metrics->mapping->zone = ngx_shared_memory_add(cf, &metrics->shm->name,
                                                   metrics->config.slab_size,
                                                   &ngx_wasmx_module);
    if (metrics->mapping->zone == NULL) {
        return NGX_CONF_ERROR;
    }

    metrics->mapping->zone->init = ngx_wasm_shm_init_zone;
    metrics->mapping->zone->data = metrics->shm;
    metrics->mapping->zone->noreuse = 0;

    if (old_metrics
        && (metrics->workers != old_metrics->workers
            || metrics->config.slab_size != old_metrics->config.slab_size))
    {
        metrics->mapping->zone->noreuse = 1;
    }

    return NGX_CONF_OK;
}


ngx_int_t
ngx_wa_metrics_init(ngx_cycle_t *cycle)
{
    ngx_int_t           rc;
    ngx_wasm_shm_kv_t  *old_shm_kv;
    ngx_wa_metrics_t   *metrics = ngx_wasmx_metrics(cycle);

    if (metrics->old_metrics && !metrics->mapping->zone->noreuse) {
        /* reuse old kv store */
        metrics->shm->data = metrics->old_metrics->shm->data;
        return NGX_OK;
    }

    rc = ngx_wasm_shm_kv_init(metrics->shm);
    if (rc != NGX_OK) {
        return rc;
    }

    if (metrics->old_metrics && metrics->mapping->zone->noreuse) {
        /* mark the old kv store for cleanup during SIGHUP old_cycle free */
        metrics->old_metrics->mapping->zone->noreuse = 1;
        metrics->mapping->zone->noreuse = 0;

        /* realloc old kv store */
        old_shm_kv = ngx_wasm_shm_get_kv(metrics->old_metrics->shm);

        return realloc_metrics(metrics, old_shm_kv->rbtree.root,
                               old_shm_kv->rbtree.sentinel);
    }

    return NGX_OK;
}


/**
 * NGX_OK: success
 * NGX_ABORT: bad usage
 * NGX_ERROR: no memory
 */
ngx_int_t
ngx_wa_metrics_define(ngx_wa_metrics_t *metrics, ngx_str_t *name,
    ngx_wa_metric_type_e type, uint32_t *out)
{
    ssize_t           size = sizeof(ngx_wa_metric_t)
                             + sizeof(ngx_wa_metric_val_t) * metrics->workers;
    uint32_t          cas, mid;
    ngx_int_t         rc, written;
    ngx_str_t        *p, val;
    ngx_wa_metric_t  *m;
    u_char            buf[size];

    if (type != NGX_WA_METRIC_COUNTER
        && type != NGX_WA_METRIC_GAUGE
        && type != NGX_WA_METRIC_HISTOGRAM)
    {
        return NGX_ABORT;
    }

    if (name->len > metrics->config.max_metric_name_length) {
        return NGX_ABORT;
    }

    mid = ngx_crc32_long(name->data, name->len);

    ngx_wasm_shm_lock(metrics->shm);

    rc = ngx_wasm_shm_kv_get_locked(metrics->shm, NULL, &mid, &p, &cas);
    if (rc == NGX_OK) {
        ngx_log_debug1(NGX_LOG_DEBUG_WASM, metrics->shm->log, 0,
                       "wasm returning existing metric id \"%uD\"", mid);
        goto done;
    }

    ngx_wa_assert(rc == NGX_DECLINED);

    ngx_memzero(buf, size);
    m = (ngx_wa_metric_t *) buf;
    m->type = type;

    if (type == NGX_WA_METRIC_HISTOGRAM) {
        rc = ngx_wa_metrics_histogram_add_locked(metrics, m);
        if (rc != NGX_OK) {
            goto error;
        }
    }

    val.len = size;
    val.data = buf;

    rc = ngx_wasm_shm_kv_set_locked(metrics->shm, name, &val, 0, &written);
    if (rc != NGX_OK) {
        goto error;
    }

done:

    *out = mid;

error:

    ngx_wasm_shm_unlock(metrics->shm);

    if (rc == NGX_OK) {
        ngx_wasm_log_error(NGX_LOG_INFO, metrics->shm->log, 0,
                           "defined %V \"%V\" with id %uD",
                           ngx_wa_metric_type_name(type), name, mid);
    }

    ngx_wa_assert(rc == NGX_OK || rc == NGX_ERROR);

    return rc;
}


/**
 * NGX_OK: success
 * NGX_ABORT: bad usage
 * NGX_DECLINED: not found
 */
ngx_int_t
ngx_wa_metrics_increment(ngx_wa_metrics_t *metrics, uint32_t mid, ngx_int_t n)
{
    uint32_t          cas;
    ngx_uint_t        slot;
    ngx_int_t         rc;
    ngx_str_t        *val;
    ngx_wa_metric_t  *m;

    slot = (ngx_process == NGX_PROCESS_WORKER) ? ngx_worker : 0;

#if 0
    if (metrics->shm->eviction != NGX_WASM_EVICTION_NONE) {
        slot = 0;
        ngx_wasm_shm_lock(metrics->shm);
    }
#endif

    rc = ngx_wasm_shm_kv_get_locked(metrics->shm, NULL, &mid, &val, &cas);
    if (rc != NGX_OK) {
        goto error;
    }

    m = (ngx_wa_metric_t *) val->data;

    switch (m->type) {
    case NGX_WA_METRIC_COUNTER:
        break;

    default:
        rc = NGX_ABORT;
        goto error;
    }

    ngx_log_debug2(NGX_LOG_DEBUG_WASM, metrics->shm->log, 0,
                   "wasm updating metric \"%uD\" with %d", mid, n);

    m->slots[slot].counter += n;

error:

#if 0
    if (metrics->shm->eviction != NGX_WASM_EVICTION_NONE) {
        ngx_wasm_shm_unlock(metrics->shm);
    }
#endif

    ngx_wa_assert(rc == NGX_OK
                  || rc == NGX_ABORT
                  || rc == NGX_DECLINED);

    return rc;
}


/**
 * NGX_OK: success
 * NGX_ABORT: bad usage
 * NGX_DECLINED: not found
 */
ngx_int_t
ngx_wa_metrics_record(ngx_wa_metrics_t *metrics, uint32_t mid, ngx_int_t n)
{
    uint32_t          cas;
    ngx_int_t         rc;
    ngx_str_t        *val;
    ngx_uint_t        slot;
    ngx_wa_metric_t  *m;

    slot = (ngx_process == NGX_PROCESS_WORKER) ? ngx_worker : 0;

#if 0
    if (metrics->shm->eviction != NGX_WASM_EVICTION_NONE) {
        slot = 0;
        ngx_wasm_shm_lock(metrics->shm);
    }
#endif

    rc = ngx_wasm_shm_kv_get_locked(metrics->shm, NULL, &mid, &val, &cas);
    if (rc != NGX_OK) {
        goto error;
    }

    ngx_log_debug2(NGX_LOG_DEBUG_WASM, metrics->shm->log, 0,
                   "wasm updating metric \"%uD\" with %d", mid, n);

    m = (ngx_wa_metric_t *) val->data;

    switch (m->type) {
    case NGX_WA_METRIC_GAUGE:
        m->slots[slot].gauge.value = n;
        m->slots[slot].gauge.last_update = ngx_current_msec;
        break;

    case NGX_WA_METRIC_HISTOGRAM:
        ngx_wa_metrics_histogram_record(metrics, m, slot, mid, n);
        break;

    default:
        rc = NGX_ABORT;
        goto error;
    }

error:

#if 0
    if (metrics->shm->eviction != NGX_WASM_EVICTION_NONE) {
        ngx_wasm_shm_unlock(metrics->shm);
    }
#endif

    ngx_wa_assert(rc == NGX_OK
                  || rc == NGX_ABORT
                  || rc == NGX_DECLINED);
    return rc;
}


/**
 * NGX_OK: success
 * NGX_ABORT: bad usage
 * NGX_DECLINED: not found
 */
ngx_int_t
ngx_wa_metrics_get(ngx_wa_metrics_t *metrics, uint32_t mid, ngx_wa_metric_t *o)
{
    uint32_t          cas;
    ngx_int_t         rc;
    ngx_str_t        *n;
    ngx_wa_metric_t  *m;

    rc = ngx_wasm_shm_kv_get_locked(metrics->shm, NULL, &mid, &n, &cas);
    if (rc != NGX_OK) {
        goto done;
    }

    m = (ngx_wa_metric_t *) n->data;
    o->type = m->type;

    switch (m->type) {
    case NGX_WA_METRIC_COUNTER:
        o->slots[0].counter = get_counter(m, metrics->workers);
        break;

    case NGX_WA_METRIC_GAUGE:
        o->slots[0].gauge.value = get_gauge(m, metrics->workers);
        break;

    case NGX_WA_METRIC_HISTOGRAM:
        ngx_wa_metrics_histogram_get(metrics, m, metrics->workers,
                                     o->slots[0].histogram);
        break;

    default:
        ngx_wa_assert(0);
        rc = NGX_ABORT;
        break;
    }

done:

    ngx_wa_assert(rc == NGX_OK
                  || rc == NGX_ABORT
                  || rc == NGX_DECLINED);

    return rc;
}
