#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_wasm.h>
#include <ngx_wasm_shm_kv.h>


typedef struct {
    ngx_rbtree_t        rbtree;
    ngx_rbtree_node_t   sentinel;
} ngx_wasm_shm_kv_t;


typedef struct {
    ngx_str_node_t      key;
    ngx_str_t           value;
    uint32_t            cas;
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
    ngx_wasm_shm_kv_t  *kv;

    kv = ngx_slab_calloc(shm->shpool, sizeof(ngx_wasm_shm_kv_t));
    if (kv == NULL) {
        return NGX_ERROR;
    }

    ngx_rbtree_init(&kv->rbtree, &kv->sentinel, ngx_str_rbtree_insert_value);
    shm->data = kv;

    ngx_log_debug1(NGX_LOG_DEBUG_WASM, shm->log, 0,
                   "wasm \"%V\" shm store: initialized", &shm->name);

    return NGX_OK;
}


ngx_int_t
ngx_wasm_shm_kv_get_locked(ngx_wasm_shm_t *shm, ngx_str_t *key,
    ngx_str_t **value_out, uint32_t *cas)
{
    ngx_wasm_shm_kv_t       *kv = ngx_wasm_shm_get_kv(shm);
    ngx_wasm_shm_kv_node_t  *n;

    n = (ngx_wasm_shm_kv_node_t *) ngx_str_rbtree_lookup(&kv->rbtree, key, 0);

    if (n == NULL) {
        return NGX_DECLINED;
    }

    if (value_out) {
        *value_out = &n->value;
    }

    if (cas) {
        *cas = n->cas;
    }

    return NGX_OK;
}


ngx_int_t
ngx_wasm_shm_kv_set_locked(ngx_wasm_shm_t *shm, ngx_str_t *key,
    ngx_str_t *value, uint32_t cas, ngx_int_t *written)
{
    ngx_wasm_shm_kv_t       *kv = ngx_wasm_shm_get_kv(shm);
    ngx_wasm_shm_kv_node_t  *n, *old;

    old = NULL;
    n = (ngx_wasm_shm_kv_node_t *) ngx_str_rbtree_lookup(&kv->rbtree, key, 0);

    if (cas != (n == NULL ? 0 : n->cas)) {
        *written = 0;
        return NGX_OK;
    }

    if (value == NULL) {

        /* delete */

        if (n) {
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
        n = ngx_slab_calloc_locked(shm->shpool,
                                   sizeof(ngx_wasm_shm_kv_node_t)
                                   + key->len + value->len);
        if (n == NULL) {
            return NGX_ERROR;
        }

        n->key.str.data = (u_char *) n + sizeof(ngx_wasm_shm_kv_node_t);
        n->key.str.len = key->len;
        n->value.data = n->key.str.data + key->len;

        if (old) {
            n->cas = old->cas;
            ngx_rbtree_delete(&kv->rbtree, &old->key.node);
            ngx_slab_free_locked(shm->shpool, old);
        }

        ngx_memcpy(n->key.str.data, key->data, key->len);
        n->key.str.len = key->len;

        ngx_rbtree_insert(&kv->rbtree, &n->key.node);
    }

    /* no failure after this point */

    n->value.len = value->len;
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
