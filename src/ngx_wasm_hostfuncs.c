/*
 * Copyright (C) Thibault Charbonnier
 */

#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_wasm_hostfuncs.h>
#include <ngx_wasm_util.h>


typedef struct {
    ngx_wasm_hostfuncs_namespaces_t   namespaces;
    ngx_pool_t                       *pool;
    ngx_log_t                        *log;
} ngx_wasm_hostfuncs_store_t;


static ngx_wasm_hostfuncs_store_t  *store = NULL;


void/*{{{*/
ngx_wasm_hostfuncs_namespaces_insert(ngx_rbtree_node_t *temp,
    ngx_rbtree_node_t *node, ngx_rbtree_node_t *sentinel)
{
    ngx_wasm_hostfuncs_namespace_t  *nn, *t;
    ngx_rbtree_node_t              **n;

    for ( ;; ) {
        nn = (ngx_wasm_hostfuncs_namespace_t *) node;
        t = (ngx_wasm_hostfuncs_namespace_t *) temp;

        if (node->key != temp->key) {
            n = (node->key < temp->key) ? &temp->left : &temp->right;

        } else if (nn->name.len != t->name.len) {
            n = (nn->name.len < t->name.len) ? &temp->left : &temp->right;

        } else {
            n = (ngx_memcmp(nn->name.data, t->name.data, nn->name.len) < 0)
                ? &temp->left : &temp->right;
        }

        if (*n == sentinel) {
            break;
        }

        temp = *n;
    }

    *n = node;
    node->parent = temp;
    node->left = sentinel;
    node->right = sentinel;
    ngx_rbt_red(node);
}


static ngx_wasm_hostfuncs_namespace_t *
ngx_wasm_hostfuncs_namespaces_lookup(ngx_rbtree_t *rbtree, u_char *name,
    size_t len, uint32_t hash)
{
    ngx_wasm_hostfuncs_namespace_t  *nn;
    ngx_rbtree_node_t               *node, *sentinel;
    ngx_int_t                        rc;

    node = rbtree->root;
    sentinel = rbtree->sentinel;

    while (node != sentinel) {
        nn = (ngx_wasm_hostfuncs_namespace_t *) node;

        if (hash != node->key) {
            node = (hash < node->key) ? node->left : node->right;
            continue;
        }

        rc = ngx_memcmp(name, nn->name.data, nn->name.len);

        if (rc < 0) {
            node = node->left;
            continue;
        }

        if (rc > 0) {
            node = node->right;
            continue;
        }

        return nn;
    }

    return NULL;
}/*}}}*/


ngx_int_t
ngx_wasm_hostfuncs_new(ngx_log_t *log)
{
    ngx_pool_t          *pool;

    if (store) {
        return NGX_OK;
    }

    pool = ngx_create_pool(NGX_DEFAULT_POOL_SIZE, log);
    if (pool == NULL) {
        goto failed;
    }

    store = ngx_pcalloc(pool, sizeof(ngx_wasm_hostfuncs_store_t));
    if (store == NULL) {
        ngx_destroy_pool(pool);
        goto failed;
    }

    store->pool = pool;
    store->log = log;

    ngx_rbtree_init(&store->namespaces.rbtree,
                    &store->namespaces.sentinel,
                    ngx_wasm_hostfuncs_namespaces_insert);

    return NGX_OK;

failed:

    ngx_wasm_log_error(NGX_LOG_EMERG, log, 0,
                       "failed to allocate host namespaces store");

    return NGX_ERROR;
}


static ngx_wasm_hostfuncs_namespace_t *
ngx_wasm_hostfuncs_namespace(u_char *namespace, size_t len)
{
    uint32_t                         hash;

    ngx_wasm_assert(store);

    hash = ngx_crc32_short(namespace, len);

    return ngx_wasm_hostfuncs_namespaces_lookup(&store->namespaces.rbtree,
                                                namespace, len, hash);
}


void
ngx_wasm_hostfuncs_register(const char *namespace, ngx_wasm_hostfunc_t *hfuncs)
{
    ngx_str_t                                nspace;
    ngx_wasm_hostfuncs_namespace_t          *nn;
    u_char                                  *p;
    ngx_wasm_hostfunc_t                     *hfunc, **phfunc;
    ngx_rbtree_node_t                       *n;

    ngx_wasm_assert(store);

    nspace.len = ngx_strlen(namespace);
    nspace.data = (u_char *) namespace;

    nn = ngx_wasm_hostfuncs_namespace(nspace.data, nspace.len);
    if (nn == NULL) {
        nn = ngx_palloc(store->pool, sizeof(ngx_wasm_hostfuncs_namespace_t));
        if (nn == NULL) {
            goto failed;
        }

        nn->hash = NULL;
        nn->name.len = nspace.len;
        nn->name.data = ngx_palloc(store->pool, nspace.len + 1);
        if (nn->name.data == NULL) {
            goto failed;
        }

        p = ngx_copy(nn->name.data, nspace.data, nn->name.len);
        *p = '\0';

        nn->hfuncs = ngx_array_create(store->pool, 2,
                                      sizeof(ngx_wasm_hostfunc_t *));
        if (nn->hfuncs == NULL) {
            goto failed;
        }

        n = &nn->rbnode;
        n->key = ngx_crc32_short(nn->name.data, nn->name.len);
        ngx_rbtree_insert(&store->namespaces.rbtree, n);
    }

    hfunc = hfuncs;

    for (/* void */; hfunc->ptr; hfunc++) {
        phfunc = ngx_array_push(nn->hfuncs);
        if (phfunc == NULL) {
            ngx_wasm_log_error(NGX_LOG_EMERG, store->log, 0,
                               "failed to add \"%V\" function "
                               "to \"%V\" host namespace",
                               &hfunc->name, &nn->name);
            return;
        }

        *phfunc = hfunc;
    }

    return;

failed:

    ngx_wasm_log_error(NGX_LOG_EMERG, store->log, 0,
                       "failed to add functions to \"%V\" host namespace",
                       &nspace);

    if (nn) {
        if (nn->name.data) {
            ngx_pfree(store->pool, nn->name.data);
        }

        if (nn->hfuncs) {
            ngx_array_destroy(nn->hfuncs);
        }

        ngx_pfree(store->pool, nn);
    }

    return;
}


ngx_int_t
ngx_wasm_hostfuncs_init()
{
    ngx_wasm_hostfuncs_namespace_t  *nn;
    ngx_rbtree_node_t               *node, *root, *sentinel;
    ngx_pool_t                      *temp_pool;
    ngx_hash_init_t                  hash;
    ngx_hash_keys_arrays_t           hash_keys;
    size_t                           i;
    ngx_wasm_hostfunc_t             *hfunc, **hfuncs;
    ngx_int_t                        rc = NGX_ERROR;

    ngx_wasm_assert(store);

    sentinel = store->namespaces.rbtree.sentinel;
    root = store->namespaces.rbtree.root;

    if (root == sentinel) {
        return NGX_OK;
    }

    temp_pool = ngx_create_pool(NGX_MIN_POOL_SIZE, store->log);
    if (temp_pool == NULL) {
        return NGX_ERROR;
    }

    for (node = ngx_rbtree_min(root, sentinel);
         node;
         node = ngx_rbtree_next(&store->namespaces.rbtree, node))
    {
        nn = (ngx_wasm_hostfuncs_namespace_t *) node;

        if (nn->hash) {
            /* rebuild hash */
            ngx_pfree(store->pool, nn->hash);
        }

        ngx_log_debug1(NGX_LOG_DEBUG_WASM, store->log, 0,
                      "[wasm] initializing \"%V\" host namespace", &nn->name);

        nn->hash = ngx_pcalloc(store->pool, sizeof(ngx_hash_t));
        if (nn->hash == NULL) {
            goto failed;
        }

        hash.hash = nn->hash;
        hash.key = ngx_hash_key;
        hash.max_size = 512;
        hash.bucket_size = ngx_align(64, ngx_cacheline_size);
        hash.pool = store->pool;
        hash.temp_pool = temp_pool;
        hash.name = ngx_palloc(temp_pool, 128);
        if (hash.name == NULL) {
            goto failed;
        }

        ngx_snprintf((u_char *) hash.name, 128,
                    "wasm functions hash for \"%V\" host namespace",
                    &nn->name);

        hash_keys.pool = store->pool;
        hash_keys.temp_pool = temp_pool;

        ngx_hash_keys_array_init(&hash_keys, NGX_HASH_SMALL);

        hfuncs = nn->hfuncs->elts;

        for (i = 0; i < nn->hfuncs->nelts; i++) {
            hfunc = hfuncs[i];

            ngx_hash_add_key(&hash_keys, &hfunc->name, hfunc->ptr,
                             NGX_HASH_READONLY_KEY);
        }

        rc = ngx_hash_init(&hash, hash_keys.keys.elts, hash_keys.keys.nelts);
        if (rc != NGX_OK) {
            goto failed;
        }
    }

    rc = NGX_OK;

failed:

    ngx_destroy_pool(temp_pool);

    return rc;
}


ngx_wasm_hostfuncs_namespaces_t *
ngx_wasm_hostfuncs_namespaces()
{
    ngx_wasm_assert(store);

    return &store->namespaces;
}


static ngx_wasm_hostfunc_pt
ngx_wasm_hostfuncs_namespace_find(ngx_wasm_hostfuncs_namespace_t *nn,
    char *name, size_t len)
{
    ngx_uint_t      key;

    ngx_wasm_assert(store);

    if (nn->hash == NULL) {
        ngx_wasm_log_error(NGX_LOG_EMERG, store->log, 0,
                           "failed to search \"%V\" host namespace: "
                           "hash not initialized", &nn->name);
        return NULL;
    }

    key = ngx_hash_key((u_char *) name, len);

    return ngx_hash_find(nn->hash, key, (u_char *) name, len);
}


static void
ngx_wasm_hostfuncs_destroy()
{
    if (store) {
        ngx_destroy_pool(store->pool);
    }

    store = NULL;
}


/* vi:set ft=c ts=4 sw=4 et fdm=marker: */
