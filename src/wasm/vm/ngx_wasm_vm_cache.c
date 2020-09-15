/*
 * Copyright (C) Thibault Charbonnier
 */

#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_wasm_vm_cache.h>
#include <ngx_wasm_vm.h>
#include <ngx_wasm_core.h>


typedef struct {
    ngx_wasm_nn_t            nn;
    ngx_wasm_vm_instance_t  *inst;
} ngx_wasm_vm_cache_node_t;


ngx_wasm_vm_cache_t *
ngx_wasm_vm_cache_new(ngx_wasm_vm_t *vm)
{
    ngx_wasm_vm_cache_t  *cache;

    cache = ngx_palloc(vm->pool, sizeof(ngx_wasm_vm_cache_t));
    if (cache == NULL) {
        return NULL;
    }

    ngx_log_debug2(NGX_LOG_DEBUG_WASM, vm->pool->log, 0,
                   "wasm new vm cache for \"%V\" vm (cache: %p)",
                   &vm->name, cache);

    cache->vm = vm;

    ngx_wasm_vm_cache_init(cache);

    return cache;
}


ngx_inline void
ngx_wasm_vm_cache_init(ngx_wasm_vm_cache_t *cache)
{
    ngx_wasm_assert(cache->vm);

    cache->pool = cache->vm->pool;

    ngx_log_debug1(NGX_LOG_DEBUG_WASM, cache->pool->log, 0,
                   "wasm init vm cache (cache: %p)", cache);

    ngx_rbtree_init(&cache->rbtree, &cache->sentinel,
                    ngx_wasm_rbtree_insert_nn);
}


ngx_wasm_vm_instance_t *
ngx_wasm_vm_cache_get_instance(ngx_wasm_vm_cache_t *cache, ngx_str_t *mod_name)
{
    ngx_wasm_nn_t             *nn;
    ngx_wasm_vm_cache_node_t  *cnode;

    nn = ngx_wasm_nn_rbtree_lookup(&cache->rbtree, mod_name->data,
                                   mod_name->len);
    if (nn != NULL) {
        cnode = (ngx_wasm_vm_cache_node_t *) ngx_wasm_nn_data(nn, ngx_wasm_vm_cache_node_t, nn);
        return cnode->inst;
    }

    cnode = ngx_palloc(cache->pool, sizeof(ngx_wasm_vm_cache_node_t));
    if (cnode == NULL) {
        return NULL;
    }

    cnode->inst = ngx_wasm_vm_instance_new(cache->vm, mod_name);
    if (cnode->inst == NULL) {
        ngx_pfree(cache->pool, cnode);
        return NULL;
    }

    ngx_wasm_nn_init(&cnode->nn, mod_name);
    ngx_wasm_nn_rbtree_insert(&cache->rbtree, &cnode->nn);

    return cnode->inst;
}


void
ngx_wasm_vm_cache_cleanup(ngx_wasm_vm_cache_t *cache)
{
    ngx_rbtree_node_t         **root, **sentinel, *node;
    ngx_wasm_nn_t              *nn;
    ngx_wasm_vm_cache_node_t   *cnode;

    ngx_log_debug1(NGX_LOG_DEBUG_WASM, cache->pool->log, 0,
                   "wasm cleanup vm cache (cache: %p)", cache);

    root = &cache->rbtree.root;
    sentinel = &cache->rbtree.sentinel;

    while (*root != *sentinel) {
        node = ngx_rbtree_min(*root, *sentinel);
        nn = ngx_wasm_nn_n2nn(node);
        cnode = ngx_wasm_nn_data(nn, ngx_wasm_vm_cache_node_t, nn);

        ngx_wasm_vm_instance_free(cnode->inst);

        ngx_rbtree_delete(&cache->rbtree, node);

        ngx_pfree(cache->pool, cnode);
    }
}


void
ngx_wasm_vm_cache_free(ngx_wasm_vm_cache_t *cache)
{
    ngx_wasm_vm_cache_cleanup(cache);

    ngx_log_debug1(NGX_LOG_DEBUG_WASM, cache->pool->log, 0,
                   "wasm free vm cache (cache: %p)", cache);

    ngx_pfree(cache->pool, cache);
}
