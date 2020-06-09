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
    ngx_rbtree_node_t  rbnode;

    ngx_str_t          name;
    ngx_array_t       *hfuncs;
    ngx_hash_t        *hash;
} ngx_wasm_hostfuncs_namespace_t;


typedef struct {
    ngx_rbtree_t       rbtree;
    ngx_rbtree_node_t  sentinel;
} ngx_wasm_hostfuncs_namespaces_t;


typedef struct {
    ngx_wasm_hostfuncs_namespaces_t   namespaces;
    ngx_pool_t                       *pool;
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
ngx_wasm_hostfuncs_namespaces_lookup(ngx_rbtree_t *rbtree, const char *name,
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


void
ngx_wasm_hostfuncs_register(const char *namespace,
    ngx_wasm_hostfunc_t *hfuncs, ngx_log_t *log)
{
    ngx_pool_t                              *pool;
    ngx_wasm_hostfuncs_namespace_t          *nn;
    ngx_rbtree_node_t                       *n;
    ngx_wasm_hostfunc_t                    **phfuncs;
    u_char                                  *p;
    uint32_t                                 hash;
    size_t                                   len;

    if (store == NULL) {
        pool = ngx_create_pool(NGX_DEFAULT_POOL_SIZE, log);
        if (pool == NULL) {
            return;
        }

        store = ngx_pcalloc(pool, sizeof(ngx_wasm_hostfuncs_store_t));
        if (store == NULL) {
            ngx_wasm_log_error(NGX_LOG_EMERG, log, 0,
                               "failed to allocate host functions store");
            ngx_destroy_pool(pool);
            return;
        }

        store->pool = pool;

        ngx_rbtree_init(&store->namespaces.rbtree,
                        &store->namespaces.sentinel,
                        ngx_wasm_hostfuncs_namespaces_insert);
    }

    len = ngx_strlen(namespace);
    hash = ngx_crc32_short((u_char *) namespace, len);

    nn = ngx_wasm_hostfuncs_namespaces_lookup(&store->namespaces.rbtree,
                                              namespace, len, hash);
    if (nn == NULL) {
        nn = ngx_palloc(store->pool, sizeof(ngx_wasm_hostfuncs_namespace_t));
        if (nn == NULL) {
            goto failed;
        }

        nn->name.len = len;
        nn->name.data = ngx_palloc(store->pool, len + 1);
        if (nn->name.data == NULL) {
            goto failed;
        }

        p = ngx_copy(nn->name.data, namespace, nn->name.len);
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

    phfuncs = ngx_array_push(nn->hfuncs);
    if (phfuncs == NULL) {
        ngx_wasm_log_error(NGX_LOG_EMERG, log, 0,
                           "failed to add host functions to \"%V\" namespace",
                           &nn->name);
        return;
    }

    *phfuncs = hfuncs;

    return;

failed:

    ngx_wasm_log_error(NGX_LOG_EMERG, log, 0,
                       "failed to add host functions to \"%*s\" namespace",
                       len, namespace);

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
    ngx_wasm_hostfunc_t             *hfunc, *hfuncs, **phfuncs;
    ngx_pool_t                      *temp_pool;
    ngx_hash_init_t                  hash;
    ngx_hash_keys_arrays_t           hash_keys;
    size_t                           i;
    ngx_int_t                        rc = NGX_ERROR;

    if (store == NULL) {
        return NGX_ERROR;
    }

    sentinel = store->namespaces.rbtree.sentinel;
    root = store->namespaces.rbtree.root;

    if (root == sentinel) {
        return NGX_OK;
    }

    temp_pool = ngx_create_pool(NGX_MIN_POOL_SIZE, store->pool->log);
    if (temp_pool == NULL) {
        return NGX_ERROR;
    }

    /* for each namespace */

    for (node = ngx_rbtree_min(root, sentinel);
         node;
         node = ngx_rbtree_next(&store->namespaces.rbtree, node))
    {
        nn = (ngx_wasm_hostfuncs_namespace_t *) node;

        if (nn->hash) {
            /* rebuild hash */
            ngx_pfree(store->pool, nn->hash);
        }

        /* init hash */

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
        ngx_sprintf((u_char *) hash.name,
                    "wasm host functions hash for \"%V\" namespace", &nn->name);

        hash_keys.pool = store->pool;
        hash_keys.temp_pool = temp_pool;

        ngx_hash_keys_array_init(&hash_keys, NGX_HASH_SMALL);

        phfuncs = nn->hfuncs->elts;

        /* for each namespace registered array */

        for (i = 0; i < nn->hfuncs->nelts; i++) {
            hfuncs = phfuncs[i];
            hfunc = hfuncs;

            /* for each host function */

            for (/* void */; hfunc->ptr; hfunc++) {
                ngx_hash_add_key(&hash_keys, (ngx_str_t *) &hfunc->name,
                                 hfunc->ptr, NGX_HASH_READONLY_KEY);
            }
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


ngx_wasm_hostfunc_pt
ngx_wasm_hostfuncs_find(const char *namespace, size_t nlen, char *name,
    size_t len)
{
    ngx_wasm_hostfuncs_namespace_t  *nn;
    uint32_t                         hash;
    ngx_uint_t                       key;

    if (!store) {
        return NULL;
    }

    hash = ngx_crc32_short((u_char *) namespace, nlen);

    nn = ngx_wasm_hostfuncs_namespaces_lookup(&store->namespaces.rbtree,
                                              namespace, nlen, hash);
    if (nn == NULL || nn->hash == NULL) {
        return NULL;
    }

    key = ngx_hash_key((u_char *) name, len);

    return ngx_hash_find(nn->hash, key, (u_char *) name, len);
}


void
ngx_wasm_hostfuncs_destroy()
{
    if (store) {
        ngx_destroy_pool(store->pool);
    }

    store = NULL;
}


/* vi:set ft=c ts=4 sw=4 et fdm=marker: */
