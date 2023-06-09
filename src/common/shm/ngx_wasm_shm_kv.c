#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_wasm.h>
#include <ngx_wasm_shm_kv.h>


/* as defined in nginx/src/core/ngx_slab.c */
#define NGX_WASM_SLAB_MAX_SIZE       (ngx_pagesize / 2)
#define NGX_WASM_SLAB_SLOTS(pool)    (ngx_pagesize_shift - (pool)->min_shift)

/* one extra queue for larger items */
#define NGX_WASM_SLRU_NQUEUES(pool)  (NGX_WASM_SLAB_SLOTS(pool) + 1)


typedef struct {
    ngx_rbtree_t        rbtree;
    ngx_rbtree_node_t   sentinel;
    union {
        ngx_queue_t     lru_queue;
        ngx_queue_t     slru_queues[0];
    } eviction;
} ngx_wasm_shm_kv_t;


typedef struct {
    ngx_str_node_t      key;
    ngx_str_t           value;
    uint32_t            cas;
    ngx_queue_t         queue;
} ngx_wasm_shm_kv_node_t;


static ngx_inline ngx_wasm_shm_kv_t *
ngx_wasm_shm_get_kv(ngx_wasm_shm_t *shm)
{
    ngx_wasm_assert(shm->type == NGX_WASM_SHM_TYPE_KV);
    return shm->data;
}


ngx_int_t
ngx_wasm_shm_kv_init(ngx_wasm_shm_t *shm)
{
    size_t              size, i;
    ngx_uint_t          n;
    ngx_wasm_shm_kv_t  *kv;

    n = 0;
    size = sizeof(ngx_wasm_shm_kv_t);

    if (shm->eviction == NGX_WASM_SHM_EVICTION_SLRU) {
        n = NGX_WASM_SLRU_NQUEUES(shm->shpool);
        size += sizeof(ngx_queue_t) * n;
    }

    kv = ngx_slab_calloc(shm->shpool, size);
    if (kv == NULL) {
        return NGX_ERROR;
    }

    ngx_rbtree_init(&kv->rbtree, &kv->sentinel, ngx_str_rbtree_insert_value);
    shm->data = kv;
    shm->shpool->log_nomem = 0;

    if (shm->eviction == NGX_WASM_SHM_EVICTION_LRU) {
        ngx_queue_init(&kv->eviction.lru_queue);

    } else if (shm->eviction == NGX_WASM_SHM_EVICTION_SLRU) {
        for (i = 0; i < n; i++) {
            ngx_queue_init(&kv->eviction.slru_queues[i]);
        }
    }

    ngx_log_debug1(NGX_LOG_DEBUG_WASM, shm->log, 0,
                   "wasm \"%V\" shm store: initialized", &shm->name);

    return NGX_OK;
}


static ngx_uint_t
slru_index_for_size(ngx_wasm_shm_t *shm, size_t size)
{
    size_t      s;
    ngx_uint_t  shift;

    if (size > NGX_WASM_SLAB_MAX_SIZE) {
        return NGX_WASM_SLRU_NQUEUES(shm->shpool) - 1;
    }

    if (size > shm->shpool->min_size) {
        shift = 1;
        for (s = size - 1; s >>= 1; shift++) { /* void */ }

        return shift - shm->shpool->min_shift;
    }

    return 0;
}


static ngx_queue_t*
queue_for_node(ngx_wasm_shm_t *shm, ngx_wasm_shm_kv_node_t *n)
{
    size_t              size;
    ngx_wasm_shm_kv_t  *kv = ngx_wasm_shm_get_kv(shm);

    if (shm->eviction == NGX_WASM_SHM_EVICTION_LRU) {
        return &kv->eviction.lru_queue;
    }

    if (shm->eviction == NGX_WASM_SHM_EVICTION_SLRU) {
        size = sizeof(ngx_wasm_shm_kv_node_t)
               + n->key.str.len
               + n->value.len;

        return &kv->eviction.slru_queues[slru_index_for_size(shm, size)];
    }

    /* unreachable */
    ngx_wasm_assert(0);
    return NULL;
}


ngx_int_t
ngx_wasm_shm_kv_get_locked(ngx_wasm_shm_t *shm, ngx_str_t *key,
    ngx_str_t **value_out, uint32_t *cas)
{
    ngx_wasm_shm_kv_node_t  *n;
    ngx_wasm_shm_kv_t       *kv = ngx_wasm_shm_get_kv(shm);

    n = (ngx_wasm_shm_kv_node_t *) ngx_str_rbtree_lookup(&kv->rbtree, key, 0);

    if (n == NULL) {
        return NGX_DECLINED;
    }

    if (shm->eviction == NGX_WASM_SHM_EVICTION_LRU
        || shm->eviction == NGX_WASM_SHM_EVICTION_SLRU)
    {
        ngx_queue_remove(&n->queue);
        ngx_queue_insert_head(queue_for_node(shm, n), &n->queue);
    }

    if (value_out) {
        *value_out = &n->value;
    }

    if (cas) {
        *cas = n->cas;
    }

    return NGX_OK;
}


static ngx_int_t
queue_expire(ngx_wasm_shm_t *shm, ngx_queue_t *queue, ngx_queue_t *q)
{
    ngx_wasm_shm_kv_node_t  *node;
    ngx_wasm_shm_kv_t       *kv = ngx_wasm_shm_get_kv(shm);

    node = ngx_queue_data(q, ngx_wasm_shm_kv_node_t, queue);

    ngx_queue_remove(q);

    ngx_rbtree_delete(&kv->rbtree, &node->key.node);

    ngx_slab_free_locked(shm->shpool, node);

    return NGX_OK;
}


static ngx_int_t
slru_expire(ngx_wasm_shm_t *shm, size_t size)
{
    ngx_int_t           i;
    ngx_uint_t          n, start;
    ngx_queue_t        *q, *queue;
    ngx_wasm_shm_kv_t  *kv = ngx_wasm_shm_get_kv(shm);

    n = NGX_WASM_SLRU_NQUEUES(shm->shpool);
    start = slru_index_for_size(shm, size);

    /* find a non-empty queue to expire something,
       starting from the correct one, then into larger slots */
    for (i = start; i < (ngx_int_t) n; i++) {
        queue = &kv->eviction.slru_queues[i];

        q = ngx_queue_last(queue);
        if (q != ngx_queue_sentinel(queue)) {
            return queue_expire(shm, queue, q);
        }
    }

    /* fallback into freeing smaller slots */
    for (i = start - 1; i >= 0; i--) {
        queue = &kv->eviction.slru_queues[i];

        q = ngx_queue_last(queue);
        if (q != ngx_queue_sentinel(queue)) {
            return queue_expire(shm, queue, q);
        }
    }

    /* no luck */
    return NGX_ABORT;
}


static ngx_int_t
lru_expire(ngx_wasm_shm_t *shm)
{
    ngx_wasm_shm_kv_t  *kv = ngx_wasm_shm_get_kv(shm);
    ngx_queue_t        *q, *lru_queue = &kv->eviction.lru_queue;

    q = ngx_queue_last(lru_queue);
    if (q == ngx_queue_sentinel(lru_queue)) {
        return NGX_ABORT;
    }

    return queue_expire(shm, lru_queue, q);
}


ngx_int_t
ngx_wasm_shm_kv_set_locked(ngx_wasm_shm_t *shm, ngx_str_t *key,
    ngx_str_t *value, uint32_t cas, ngx_int_t *written)
{
    size_t                   size;
    ngx_wasm_shm_kv_node_t  *n, *old;
    ngx_wasm_shm_kv_t       *kv = ngx_wasm_shm_get_kv(shm);

    old = NULL;
    n = (ngx_wasm_shm_kv_node_t *) ngx_str_rbtree_lookup(&kv->rbtree, key, 0);

    if (cas != (n == NULL ? 0 : n->cas)) {
        *written = 0;
        return NGX_OK;
    }

    if (value == NULL) {

        /* delete */

        if (n) {
            if (shm->eviction == NGX_WASM_SHM_EVICTION_LRU) {
                ngx_queue_remove(&n->queue);
            }

            ngx_rbtree_delete(&kv->rbtree, &n->key.node);
            ngx_slab_free_locked(shm->shpool, n);
            *written = 1;

        } else {
            *written = 0;
        }

        return NGX_OK;
    }

    /* set */

    if (n && key->len > n->value.len) {
        old = n;
        n = NULL;
    }

    if (n == NULL) {
        size = sizeof(ngx_wasm_shm_kv_node_t) + key->len + value->len;

        for ( ;; ) {
            n = ngx_slab_calloc_locked(shm->shpool, size);
            if (n) {
                break;
            }

            if ((shm->eviction == NGX_WASM_SHM_EVICTION_LRU
                 && lru_expire(shm) == NGX_OK) ||
                (shm->eviction == NGX_WASM_SHM_EVICTION_SLRU
                 && slru_expire(shm, size) == NGX_OK))
            {
                ngx_log_debug1(NGX_LOG_DEBUG_WASM, shm->log, 0,
                               "wasm \"%V\" shm store: expired LRU entry",
                               &shm->name);

                continue;
            }

            ngx_wasm_log_error(NGX_LOG_CRIT, shm->log, 0,
                               "\"%V\" shm store: "
                               "no memory; cannot allocate pair with "
                               "key size %d and value size %d",
                               &shm->name, key->len, value->len);
            return NGX_ERROR;
        }

        n->key.str.data = (u_char *) n + sizeof(ngx_wasm_shm_kv_node_t);
        n->key.str.len = key->len;
        n->value.data = n->key.str.data + key->len;
        n->value.len = value->len;

        if (old) {
            if (shm->eviction == NGX_WASM_SHM_EVICTION_LRU
                || shm->eviction == NGX_WASM_SHM_EVICTION_SLRU)
            {
                ngx_queue_remove(&old->queue);
            }

            n->cas = old->cas;
            ngx_rbtree_delete(&kv->rbtree, &old->key.node);
            ngx_slab_free_locked(shm->shpool, old);
        }

        ngx_memcpy(n->key.str.data, key->data, key->len);
        n->key.str.len = key->len;

        ngx_rbtree_insert(&kv->rbtree, &n->key.node);

        if (shm->eviction == NGX_WASM_SHM_EVICTION_LRU
            || shm->eviction == NGX_WASM_SHM_EVICTION_SLRU)
        {
            ngx_queue_insert_head(queue_for_node(shm, n), &n->queue);
        }

    } else {
        n->value.len = value->len;

        if (shm->eviction == NGX_WASM_SHM_EVICTION_LRU
            || shm->eviction == NGX_WASM_SHM_EVICTION_SLRU)
        {
            ngx_queue_remove(&n->queue);
            ngx_queue_insert_head(queue_for_node(shm, n), &n->queue);
        }
    }

    /* no failure after this point */

    n->cas += 1;

    ngx_memcpy(n->value.data, value->data, value->len);

    *written = 1;

    return NGX_OK;
}


ngx_int_t
ngx_wasm_shm_kv_resolve_key(ngx_str_t *key, ngx_wasm_shm_kv_key_t *out)
{
    size_t                   i;
    ngx_int_t                zone_idx = NGX_WASM_SHM_INDEX_NOTFOUND;
    ngx_shm_zone_t          *zone;
    ngx_array_t             *zone_array;
    ngx_cycle_t             *cycle = (ngx_cycle_t *) ngx_cycle;
    static const ngx_str_t   default_namespace = ngx_string("*");

    ngx_memzero(out, sizeof(ngx_wasm_shm_kv_key_t));

    zone_array = ngx_wasm_core_shms(cycle);
    if (zone_array == NULL) {
        return NGX_DECLINED;
    }

    for (i = 0; i < key->len; i++) {
        if (key->data[i] == '/') {
            out->namespace.data = key->data;
            out->namespace.len = i;
            out->key.data = key->data + i + 1;
            out->key.len = key->len - i - 1;
            break;
        }
    }

    if (out->namespace.len) {
        zone_idx = ngx_wasm_shm_lookup_index(&out->namespace);
    }

    if (zone_idx == NGX_WASM_SHM_INDEX_NOTFOUND) {
        /*
         * If the key does not contain a namespace prefix, or the prefix does not
         * match any defined namespace, attempt to use the default namespace
         */
        out->namespace.data = default_namespace.data;
        out->namespace.len = default_namespace.len;
        out->key = *key;

        zone_idx = ngx_wasm_shm_lookup_index(&out->namespace);
        if (zone_idx == NGX_WASM_SHM_INDEX_NOTFOUND) {
            return NGX_DECLINED;
        }
    }

    zone = ((ngx_wasm_shm_mapping_t *) zone_array->elts)[zone_idx].zone;

    out->zone = zone;
    out->shm = zone->data;
    if (out->shm->type != NGX_WASM_SHM_TYPE_KV) {
        return NGX_ABORT;
    }

    return NGX_OK;
}
