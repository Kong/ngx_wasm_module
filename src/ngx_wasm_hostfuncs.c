/*
 * Copyright (C) Thibault Charbonnier
 */

#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_wasm_hostfuncs.h>
#include <ngx_wasm_util.h>


static void/*{{{*/
ngx_wasm_hfuncs_decls_module_insert(ngx_rbtree_node_t *temp,
    ngx_rbtree_node_t *node, ngx_rbtree_node_t *sentinel)
{
    ngx_wasm_hfuncs_decls_module_t  *mod, *t;
    ngx_rbtree_node_t              **n;

    for ( ;; ) {
        mod = (ngx_wasm_hfuncs_decls_module_t *) node;
        t = (ngx_wasm_hfuncs_decls_module_t *) temp;

        if (node->key != temp->key) {
            n = (node->key < temp->key) ? &temp->left : &temp->right;

        } else if (mod->name.len != t->name.len) {
            n = (mod->name.len < t->name.len) ? &temp->left : &temp->right;

        } else {
            n = (ngx_memcmp(mod->name.data, t->name.data, mod->name.len) < 0)
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


static ngx_wasm_hfuncs_decls_module_t *
ngx_wasm_hfuncs_decls_module_lookup(ngx_wasm_hfuncs_store_t *store,
    u_char *name, size_t len)
{
    ngx_wasm_hfuncs_decls_module_t  *mod;
    uint32_t                         hash;
    ngx_rbtree_node_t               *node, *sentinel;
    ngx_int_t                        rc;

    hash = ngx_crc32_short(name, len);

    node = store->rbtree.root;
    sentinel = store->rbtree.sentinel;

    while (node != sentinel) {
        mod = (ngx_wasm_hfuncs_decls_module_t *) node;

        if (hash != node->key) {
            node = (hash < node->key) ? node->left : node->right;
            continue;
        }

        rc = ngx_memcmp(name, mod->name.data, mod->name.len);

        if (rc < 0) {
            node = node->left;
            continue;
        }

        if (rc > 0) {
            node = node->right;
            continue;
        }

        return mod;
    }

    return NULL;
}/*}}}*/



ngx_wasm_hfuncs_store_t *
ngx_wasm_hfuncs_store_new(ngx_cycle_t *cycle)
{
    ngx_wasm_hfuncs_store_t  *store;

    store = ngx_pcalloc(cycle->pool, sizeof(ngx_wasm_hfuncs_store_t));
    if (store == NULL) {
        goto failed;
    }

    store->cycle = cycle;
    store->pool = cycle->pool;

    ngx_rbtree_init(&store->rbtree, &store->sentinel,
                    ngx_wasm_hfuncs_decls_module_insert);

    ngx_log_debug1(NGX_LOG_DEBUG_WASM, cycle->log, 0,
                   "[wasm] new host functions store (store: %p)",
                   store);

    return store;

failed:

    ngx_wasm_log_error(NGX_LOG_EMERG, cycle->log, 0,
                       "failed to allocate host functions store");

    return NULL;
}


void
ngx_wasm_hfuncs_store_add(ngx_wasm_hfuncs_store_t *store,
    const char *module, const ngx_wasm_hfunc_decl_t decls[])
{
    size_t                           len;
    ngx_wasm_hfuncs_decls_module_t  *mod;
    u_char                          *p;
    const ngx_wasm_hfunc_decl_t     *decl, **declp;
    ngx_rbtree_node_t               *n;

    len = ngx_strlen(module);

    mod = ngx_wasm_hfuncs_decls_module_lookup(store, (u_char *) module, len);
    if (mod == NULL) {
        mod = ngx_palloc(store->pool, sizeof(ngx_wasm_hfuncs_decls_module_t));
        if (mod == NULL) {
            goto failed;
        }

        mod->name.len = len;
        mod->name.data = ngx_pnalloc(store->pool, mod->name.len + 1);
        if (mod->name.data == NULL) {
            goto failed;
        }

        p = ngx_copy(mod->name.data, module, mod->name.len);
        *p = '\0';

        mod->decls = ngx_array_create(store->pool, 2,
                                      sizeof(ngx_wasm_hfunc_decl_t *));
        if (mod->decls == NULL) {
            goto failed;
        }

        n = &mod->rbnode;
        n->key = ngx_crc32_short(mod->name.data, mod->name.len);

        ngx_rbtree_insert(&store->rbtree, n);
    }

    decl = decls;

    for (/* void */; decl->ptr; decl++) {
        declp = ngx_array_push(mod->decls);
        if (declp == NULL) {
            ngx_wasm_log_error(NGX_LOG_EMERG, store->cycle->log, 0,
                               "failed to register \"%V.%V\" host function",
                               &mod->name, &decl->name);
            return;
        }

        ngx_log_debug2(NGX_LOG_DEBUG_WASM, store->cycle->log, 0,
                       "[wasm] registering \"%V.%V\" host function",
                       &mod->name, &decl->name);

        *declp = decl;
    }

    return;

failed:

    ngx_wasm_log_error(NGX_LOG_EMERG, store->cycle->log, 0,
                       "failed to register \"%*s\" host functions",
                       module);

    if (mod) {
        if (mod->name.data) {
            ngx_pfree(store->pool, mod->name.data);
        }

        if (mod->decls) {
            ngx_array_destroy(mod->decls);
        }

        ngx_pfree(store->pool, mod);
    }
}


void
ngx_wasm_hfuncs_store_free(ngx_wasm_hfuncs_store_t *store)
{
    ngx_rbtree_node_t               **root, **sentinel;
    ngx_wasm_hfuncs_decls_module_t   *mod;

    ngx_log_debug1(NGX_LOG_DEBUG_WASM, store->cycle->log, 0,
                   "[wasm] free host functions store (store: %p)",
                   store);

    root = &store->rbtree.root;
    sentinel = &store->rbtree.sentinel;

    while (*root != *sentinel) {
        mod = (ngx_wasm_hfuncs_decls_module_t *) ngx_rbtree_min(*root,
                                                                *sentinel);

        ngx_rbtree_delete(&store->rbtree, &mod->rbnode);

        ngx_log_debug2(NGX_LOG_DEBUG_WASM, store->cycle->log, 0,
                       "[wasm] free \"%V\" host functions (store: %p)",
                       &mod->name, store);

        ngx_array_destroy(mod->decls);
        ngx_pfree(store->pool, mod->name.data);
        ngx_pfree(store->pool, mod);
    }

    ngx_pfree(store->pool, store);
}


ngx_int_t
ngx_wasm_hfuncs_resolver_init(ngx_wasm_hfuncs_resolver_t *resolver)
{
    size_t                            i;
    ngx_rbtree_node_t                *node, *root, *sentinel;
    ngx_hash_init_t                   hash_mods, hash_funcs;
    ngx_wasm_hfuncs_decls_module_t   *mod;
    ngx_wasm_hfuncs_module_t         *rmod;
    ngx_wasm_hfunc_decl_t           **decls, *decl;
    ngx_wasm_hfunc_t                 *hfunc;
    ngx_int_t                         rc = NGX_ERROR;

    ngx_wasm_assert(resolver->pool != NULL);
    ngx_wasm_assert(resolver->log != NULL);
    ngx_wasm_assert(resolver->store != NULL);
    ngx_wasm_assert(resolver->hf_new != NULL);
    ngx_wasm_assert(resolver->hf_free != NULL);
    ngx_wasm_assert(resolver->temp_pool == NULL);

    resolver->modules = ngx_pcalloc(resolver->pool, sizeof(ngx_hash_t));
    if (resolver->modules == NULL) {
        goto failed;
    }

    resolver->temp_pool = ngx_create_pool(NGX_DEFAULT_POOL_SIZE, resolver->log);
    if (resolver->temp_pool == NULL) {
        goto failed;
    }

    hash_mods.hash = resolver->modules;
    hash_mods.key = ngx_hash_key;
    hash_mods.max_size = 64;
    hash_mods.bucket_size = ngx_align(64, ngx_cacheline_size);
    hash_mods.pool = resolver->pool;
    hash_mods.temp_pool = resolver->temp_pool;
    hash_mods.name = "wasm hash for host functions resolver";

    resolver->modules_names = ngx_pcalloc(resolver->pool,
                                          sizeof(ngx_hash_keys_arrays_t));
    if (resolver->modules_names == NULL) {
        goto failed;
    }

    resolver->modules_names->pool = resolver->pool;
    resolver->modules_names->temp_pool = resolver->temp_pool;

    ngx_hash_keys_array_init(resolver->modules_names, NGX_HASH_SMALL);

    sentinel = resolver->store->rbtree.sentinel;
    root = resolver->store->rbtree.root;

    if (root == sentinel) {
        goto empty;
    }

    for (node = ngx_rbtree_min(root, sentinel);
         node;
         node = ngx_rbtree_next(&resolver->store->rbtree, node))
    {
        mod = (ngx_wasm_hfuncs_decls_module_t *) node;

        rmod = ngx_palloc(resolver->pool, sizeof(ngx_wasm_hfuncs_module_t));
        if (rmod == NULL) {
            goto failed;
        }

        rmod->name = &mod->name;

        rmod->hfuncs = ngx_pcalloc(resolver->pool, sizeof(ngx_hash_t));
        if (rmod->hfuncs == NULL) {
            goto failed;
        }

        hash_funcs.hash = rmod->hfuncs;
        hash_funcs.key = ngx_hash_key;
        hash_funcs.max_size = 128;
        hash_funcs.bucket_size = ngx_align(64, ngx_cacheline_size);
        hash_funcs.pool = resolver->pool;
        hash_funcs.temp_pool = resolver->temp_pool;
        hash_funcs.name = ngx_palloc(resolver->pool, 128);
        if (hash_funcs.name == NULL) {
            goto failed;
        }

        ngx_snprintf((u_char *) hash_funcs.name, 128,
                     "wasm hash for \"%V\" host functions", &mod->name);

        rmod->hfuncs_names = ngx_pcalloc(resolver->pool,
                                         sizeof(ngx_hash_keys_arrays_t));
        if (rmod->hfuncs_names == NULL) {
            goto failed;
        }

        rmod->hfuncs_names->pool = resolver->pool;
        rmod->hfuncs_names->temp_pool = resolver->temp_pool;

        ngx_hash_keys_array_init(rmod->hfuncs_names, NGX_HASH_SMALL);

        for (decls = mod->decls->elts, i = 0; i < mod->decls->nelts; i++) {
            decl = decls[i];

            ngx_log_debug4(NGX_LOG_DEBUG_WASM, resolver->log, 0,
                           "[wasm] defining \"%V.%V\" host function"
                           " (%z/%z)", &mod->name, &decl->name,
                           i + 1, mod->decls->nelts);

            hfunc = ngx_palloc(resolver->pool, sizeof(ngx_wasm_hfunc_t));
            if (hfunc == NULL) {
                goto failed;
            }

            hfunc->name = &decl->name;
            hfunc->ptr = decl->ptr;

            ngx_memcpy(&hfunc->args, &decl->args,
                       sizeof(ngx_wasm_val_kind) * NGX_WASM_ARGS_MAX);

            ngx_memcpy(&hfunc->rets, &decl->rets,
                       sizeof(ngx_wasm_val_kind) * NGX_WASM_RETS_MAX);

            for (hfunc->nargs = 0; hfunc->args[hfunc->nargs]; hfunc->nargs++);
            for (hfunc->nrets = 0; hfunc->rets[hfunc->nrets]; hfunc->nrets++);

            hfunc->vm_data = (void *) resolver->hf_new(hfunc);
            if (hfunc->vm_data == NULL) {
                ngx_wasm_log_error(NGX_LOG_EMERG, resolver->log, 0,
                                   "failed to initialize \"%V.%V\" host "
                                   "function: invalid init return value",
                                   &mod->name, &decl->name);

                ngx_pfree(resolver->pool, hfunc);

                goto failed;
            }

            ngx_hash_add_key(rmod->hfuncs_names, hfunc->name, hfunc,
                             NGX_HASH_READONLY_KEY);
        }

        if (ngx_hash_init(&hash_funcs, rmod->hfuncs_names->keys.elts,
                          rmod->hfuncs_names->keys.nelts)
            != NGX_OK)
        {
            goto failed;
        }

        ngx_hash_add_key(resolver->modules_names, rmod->name, rmod,
                         NGX_HASH_READONLY_KEY);
    }

empty:

    if (ngx_hash_init(&hash_mods, resolver->modules_names->keys.elts,
                      resolver->modules_names->keys.nelts)
        != NGX_OK)
    {
        goto failed;
    }

    rc = NGX_OK;

failed:

    if (rc != NGX_OK) {
        ngx_wasm_hfuncs_resolver_destroy(resolver);
    }

    return rc;
}


ngx_wasm_hfunc_t *
ngx_wasm_hfuncs_resolver_lookup(ngx_wasm_hfuncs_resolver_t *resolver,
    char *module, size_t mlen, char *func, size_t flen)
{
    ngx_wasm_hfuncs_module_t  *rmod;
    ngx_uint_t                 mkey, fkey;

    mkey = ngx_hash_key((u_char *) module, mlen);

    rmod = ngx_hash_find(resolver->modules, mkey, (u_char *) module, mlen);
    if (rmod == NULL) {
        return NULL;
    }

    fkey = ngx_hash_key((u_char *) func, flen);

    return (ngx_wasm_hfunc_t *)
            ngx_hash_find(rmod->hfuncs, fkey, (u_char *) func, flen);
}


static ngx_inline void
ngx_wasm_ngx_hash_keys_array_destroy(ngx_hash_keys_arrays_t *ha)
{
#ifdef NGX_WASM_NO_POOL
    ngx_array_destroy(&ha->keys);
    ngx_array_destroy(&ha->dns_wc_head);
    ngx_array_destroy(&ha->dns_wc_tail);

    ngx_pfree(ha->temp_pool, ha->keys_hash);
    ngx_pfree(ha->temp_pool, ha->dns_wc_head_hash);
    ngx_pfree(ha->temp_pool, ha->dns_wc_tail_hash);
#endif
}


void
ngx_wasm_hfuncs_resolver_destroy(ngx_wasm_hfuncs_resolver_t *resolver)
{
    size_t                     i, j;
    ngx_hash_key_t            *mkeys, *fkeys;
    ngx_wasm_hfuncs_module_t  *rmod;
    ngx_wasm_hfunc_t          *hfunc;

    for (mkeys = resolver->modules_names->keys.elts, i = 0;
         i < resolver->modules_names->keys.nelts;
         i++)
    {
        rmod = (ngx_wasm_hfuncs_module_t *) mkeys[i].value;

        for (fkeys = rmod->hfuncs_names->keys.elts, j = 0;
             j < rmod->hfuncs_names->keys.nelts;
             j++)
        {
            hfunc = (ngx_wasm_hfunc_t *) fkeys[i].value;

            ngx_log_debug2(NGX_LOG_DEBUG_WASM, resolver->log, 0,
                           "[wasm] free \"%V.%V\" host function",
                           rmod->name, hfunc->name);

            resolver->hf_free(hfunc->vm_data);

            ngx_pfree(resolver->pool, hfunc);
        }

        ngx_wasm_ngx_hash_keys_array_destroy(rmod->hfuncs_names);

        ngx_pfree(resolver->temp_pool, rmod->hfuncs_names);
        ngx_pfree(resolver->pool, rmod->hfuncs);
        ngx_pfree(resolver->pool, rmod);
    }

    ngx_wasm_ngx_hash_keys_array_destroy(resolver->modules_names);

    ngx_pfree(resolver->temp_pool, resolver->modules_names);

    ngx_destroy_pool(resolver->temp_pool);
}
