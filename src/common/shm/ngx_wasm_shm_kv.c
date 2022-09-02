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
    ngx_str_node_t   key;
    ngx_str_t        value;
    uint32_t         cas;
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

    return NGX_OK;
}


ngx_int_t
ngx_wasm_shm_kv_get_locked(ngx_wasm_shm_t *shm, ngx_str_t *key, ngx_str_t **value_out, uint32_t *cas)
{
    ngx_wasm_shm_kv_t       *kv = ngx_wasm_shm_get_kv(shm);
    ngx_wasm_shm_kv_node_t  *n = (ngx_wasm_shm_kv_node_t *) ngx_str_rbtree_lookup(&kv->rbtree, key, 0);

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
ngx_wasm_shm_kv_set_locked(ngx_wasm_shm_t *shm, ngx_str_t *key, ngx_str_t *value, uint32_t cas, ngx_int_t *written)
{
    ngx_wasm_shm_kv_t       *kv = ngx_wasm_shm_get_kv(shm);
    ngx_wasm_shm_kv_node_t  *n = (ngx_wasm_shm_kv_node_t *) ngx_str_rbtree_lookup(&kv->rbtree, key, 0);
    u_char                  *value_buffer;

    if (cas != (n == NULL ? 0 : n->cas)) {
        *written = 0;
        return NGX_OK;
    }

    if (value == NULL) {
        /* delete */

        if (n) {
            ngx_rbtree_delete(&kv->rbtree, &n->key.node);
            ngx_slab_free_locked(shm->shpool, n->key.str.data);
            ngx_slab_free_locked(shm->shpool, n->value.data);
            ngx_slab_free_locked(shm->shpool, n);
            *written = 1;

        } else {
            *written = 0;
        }

        return NGX_OK;
    }

    /* set */

    value_buffer = ngx_slab_alloc_locked(shm->shpool, value->len);
    if (value_buffer == NULL) {
        return NGX_ERROR;
    }

    if (n == NULL) {
        n = ngx_slab_calloc_locked(shm->shpool, sizeof(ngx_wasm_shm_kv_node_t));
        if (n == NULL) {
            ngx_slab_free_locked(shm->shpool, value_buffer);
            return NGX_ERROR;
        }

        n->key.str.data = ngx_slab_alloc_locked(shm->shpool, key->len);
        if (n->key.str.data == NULL) {
            ngx_slab_free_locked(shm->shpool, n);
            ngx_slab_free_locked(shm->shpool, value_buffer);
            return NGX_ERROR;
        }

        ngx_memcpy(n->key.str.data, key->data, key->len);
        n->key.str.len = key->len;

        ngx_rbtree_insert(&kv->rbtree, &n->key.node);
    }

    /* No failure after this point */
    if (n->value.data) {
        ngx_slab_free_locked(shm->shpool, n->value.data);
    }

    n->value.data = value_buffer;

    ngx_memcpy(n->value.data, value->data, value->len);
    n->value.len = value->len;
    n->cas += 1;

    *written = 1;
    return NGX_OK;
}
