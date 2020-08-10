/*
 * Copyright (C) Thibault Charbonnier
 */

#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_wasm_vm_cache.h>
#include <ngx_wasm_util.h>


typedef struct {
    ngx_wasm_rbtree_named_node_t   rbnode;
    ngx_wasm_instance_t           *instance;
} ngx_wasm_vm_cache_node_t;


void
ngx_wasm_vm_cache_init(ngx_wasm_vm_cache_t *cache)
{
    ngx_rbtree_init(&cache->rbtree, &cache->sentinel,
                    ngx_wasm_rbtree_insert_named_node);
}


ngx_wasm_instance_t *
ngx_wasm_vm_cache_get_instance(ngx_wasm_vm_cache_t *cache, ngx_str_t *mod_name)
{
    ngx_rbtree_node_t         *n;
    ngx_wasm_vm_cache_node_t  *cn;

    n = ngx_wasm_rbtree_lookup_named_node(&cache->rbtree, mod_name->data,
                                          mod_name->len);
    if (n != NULL) {
        cn = (ngx_wasm_vm_cache_node_t *)
                 ((u_char *) n - offsetof(ngx_wasm_vm_cache_node_t, rbnode));
        return cn->instance;
    }

    cn = ngx_palloc(cache->pool, sizeof(ngx_wasm_vm_cache_node_t));
    if (cn == NULL) {
        return NULL;
    }

    cn->instance = ngx_wasm_vm_instance_new(cache->vm, mod_name);
    if (cn->instance == NULL) {
        ngx_pfree(cache->pool, cn);
        return NULL;
    }

    ngx_wasm_rbtree_set_named_node(&cn->rbnode, mod_name);

    ngx_rbtree_insert(&cache->rbtree, &cn->rbnode.node);

    return cn->instance;
}


void
ngx_wasm_vm_cache_cleanup(ngx_wasm_vm_cache_t *cache)
{
    ngx_rbtree_node_t         **root, **sentinel, *node;
    ngx_wasm_vm_cache_node_t   *cn;

    ngx_log_debug1(NGX_LOG_DEBUG_WASM, cache->pool->log, 0,
                   "[wasm] vm cache cleanup (cache: %p)", cache);

    root = &cache->rbtree.root;
    sentinel = &cache->rbtree.sentinel;

    while (*root != *sentinel) {
        node = ngx_rbtree_min(*root, *sentinel);
        cn = (ngx_wasm_vm_cache_node_t *)
                 ((u_char *) node - offsetof(ngx_wasm_vm_cache_node_t, rbnode));

        ngx_wasm_vm_instance_free(cn->instance);

        ngx_rbtree_delete(&cache->rbtree, node);

        ngx_pfree(cache->pool, cn);
    }
}
