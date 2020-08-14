/*
 * Copyright (C) Thibault Charbonnier
 */

#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_wasm_hfuncs.h>
#include <ngx_wasm_util.h>


typedef struct {
    ngx_wasm_rbtree_named_node_t   rbnode;
    ngx_str_t                      name;
    ngx_array_t                   *decls;
} ngx_wasm_hfuncs_decls_module_t;


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
                    ngx_wasm_rbtree_insert_named_node);

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
    ngx_rbtree_node_t               *n;
    ngx_wasm_hfuncs_decls_module_t  *mod;
    const ngx_wasm_hfunc_decl_t     *decl, **declp;

    len = ngx_strlen(module);

    n = ngx_wasm_rbtree_lookup_named_node(&store->rbtree, (u_char *) module, len);
    if (n) {
        mod = (ngx_wasm_hfuncs_decls_module_t *)
                  ((u_char *) n - offsetof(ngx_wasm_hfuncs_decls_module_t,
                                           rbnode));

    } else {
        mod = ngx_palloc(store->pool, sizeof(ngx_wasm_hfuncs_decls_module_t));
        if (mod == NULL) {
            goto failed;
        }

        mod->name.len = len;
        mod->name.data = (u_char *) module;

        mod->decls = ngx_array_create(store->pool, 2,
                                      sizeof(ngx_wasm_hfunc_decl_t *));
        if (mod->decls == NULL) {
            goto failed;
        }

        ngx_wasm_rbtree_set_named_node(&mod->rbnode, &mod->name);
        ngx_rbtree_insert(&store->rbtree, &mod->rbnode.node);
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

        ngx_rbtree_delete(&store->rbtree, &mod->rbnode.node);

        ngx_log_debug2(NGX_LOG_DEBUG_WASM, store->cycle->log, 0,
                       "[wasm] free \"%V\" host functions (store: %p)",
                       &mod->name, store);

        ngx_array_destroy(mod->decls);
        ngx_pfree(store->pool, mod);
    }

    ngx_pfree(store->pool, store);
}


ngx_wasm_hfuncs_resolver_t *
ngx_wasm_hfuncs_resolver_new(ngx_cycle_t *cycle,
    ngx_wasm_hfuncs_store_t *store, ngx_wrt_t *runtime)
{
    size_t                            i;
    u_char                           *p;
    ngx_hash_init_t                   hash_mods, hash_funcs;
    ngx_rbtree_node_t                *node, *root, *sentinel;
    ngx_wasm_hfuncs_decls_module_t   *mod;
    ngx_wasm_hfuncs_module_t         *rmod;
    ngx_wasm_hfunc_decl_t           **decls, *decl;
    ngx_wasm_hfunc_t                 *hfunc;
    ngx_wasm_hfuncs_resolver_t       *resolver;

    resolver = ngx_pcalloc(cycle->pool, sizeof(ngx_wasm_hfuncs_resolver_t));
    if (resolver == NULL) {
        goto failed;
    }

    resolver->pool = cycle->pool;
    resolver->log = &cycle->new_log;
    resolver->functype_new = runtime->hfunctype_new;
    resolver->functype_free = runtime->hfunctype_free;

    resolver->temp_pool = ngx_create_pool(NGX_DEFAULT_POOL_SIZE, cycle->log);
    if (resolver->temp_pool == NULL) {
        goto failed;
    }

    resolver->modules = ngx_pcalloc(resolver->pool, sizeof(ngx_hash_t));
    if (resolver->modules == NULL) {
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

    sentinel = store->rbtree.sentinel;
    root = store->rbtree.root;

    if (root == sentinel) {
        goto empty;
    }

    for (node = ngx_rbtree_min(root, sentinel);
         node;
         node = ngx_rbtree_next(&store->rbtree, node))
    {
        mod = (ngx_wasm_hfuncs_decls_module_t *) node;

        rmod = ngx_palloc(resolver->pool, sizeof(ngx_wasm_hfuncs_module_t));
        if (rmod == NULL) {
            goto failed;
        }

        rmod->name.len = mod->name.len;
        rmod->name.data = ngx_pnalloc(resolver->pool, mod->name.len + 1);
        if (rmod->name.data == NULL) {
            goto failed;
        }

        p = ngx_copy(rmod->name.data, mod->name.data, rmod->name.len);
        *p = '\0';

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

            hfunc = ngx_pcalloc(resolver->pool, sizeof(ngx_wasm_hfunc_t));
            if (hfunc == NULL) {
                goto failed;
            }

            hfunc->name = &decl->name;
            hfunc->ptr = decl->ptr;

            ngx_memcpy(&hfunc->args, &decl->args,
                       sizeof(ngx_wasm_val_kind) * NGX_WASM_ARGS_MAX);

            ngx_memcpy(&hfunc->rets, &decl->rets,
                       sizeof(ngx_wasm_val_kind) * NGX_WASM_RETS_MAX);

            for (hfunc->nargs = 0;
                 hfunc->args[hfunc->nargs] && hfunc->nargs < NGX_WASM_ARGS_MAX;
                 hfunc->nargs++);

            for (hfunc->nrets = 0;
                 hfunc->rets[hfunc->nrets] && hfunc->nrets < NGX_WASM_RETS_MAX;
                 hfunc->nrets++);

            hfunc->wrt_functype = resolver->functype_new(hfunc);
            if (hfunc->wrt_functype == NULL) {
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

        ngx_hash_add_key(resolver->modules_names, &rmod->name, rmod,
                         NGX_HASH_READONLY_KEY);
    }

empty:

    if (ngx_hash_init(&hash_mods, resolver->modules_names->keys.elts,
                      resolver->modules_names->keys.nelts)
        != NGX_OK)
    {
        goto failed;
    }

    return resolver;

failed:

    ngx_wasm_log_error(NGX_LOG_EMERG, cycle->log, 0,
                       "failed to create host functions resolver");

    if (resolver) {
        ngx_wasm_hfuncs_resolver_free(resolver);
    }

    return NULL;
}


ngx_wasm_hfunc_t *
ngx_wasm_hfuncs_resolver_lookup(ngx_wasm_hfuncs_resolver_t *resolver,
    u_char *mod_name, size_t mod_len, u_char *func_name, size_t func_len)
{
    ngx_uint_t                 mkey, fkey;
    ngx_wasm_hfuncs_module_t  *rmod;

    mkey = ngx_hash_key(mod_name, mod_len);

    rmod = ngx_hash_find(resolver->modules, mkey, mod_name, mod_len);
    if (rmod == NULL) {
        return NULL;
    }

    fkey = ngx_hash_key(func_name, func_len);

    return (ngx_wasm_hfunc_t *)
            ngx_hash_find(rmod->hfuncs, fkey, func_name, func_len);
}


static ngx_inline void
ngx_wasm_hash_keys_array_cleanup(ngx_hash_keys_arrays_t *ha)
{
#ifdef NGX_WASM_NOPOOL
    ngx_array_destroy(&ha->keys);
    ngx_array_destroy(&ha->dns_wc_head);
    ngx_array_destroy(&ha->dns_wc_tail);

    ngx_pfree(ha->temp_pool, ha->keys_hash);
    ngx_pfree(ha->temp_pool, ha->dns_wc_head_hash);
    ngx_pfree(ha->temp_pool, ha->dns_wc_tail_hash);
#endif
}


void
ngx_wasm_hfuncs_resolver_free(ngx_wasm_hfuncs_resolver_t *resolver)
{
    size_t                     i, j;
    ngx_hash_key_t            *mkeys, *fkeys;
    ngx_wasm_hfuncs_module_t  *rmod;
    ngx_wasm_hfunc_t          *hfunc;

    ngx_wasm_assert(resolver != NULL);

    ngx_log_debug0(NGX_LOG_DEBUG_WASM, resolver->log, 0,
                   "[wasm] free hfuncs resolver");

    if (resolver->modules) {
        ngx_pfree(resolver->pool, resolver->modules);
    }

    if (resolver->modules_names) {
        for (mkeys = resolver->modules_names->keys.elts, i = 0;
             i < resolver->modules_names->keys.nelts;
             i++)
        {
            rmod = (ngx_wasm_hfuncs_module_t *) mkeys[i].value;

            for (fkeys = rmod->hfuncs_names->keys.elts, j = 0;
                 j < rmod->hfuncs_names->keys.nelts;
                 j++)
            {
                hfunc = (ngx_wasm_hfunc_t *) fkeys[j].value;

                ngx_log_debug2(NGX_LOG_DEBUG_WASM, resolver->log, 0,
                               "[wasm] free \"%V.%V\" host function",
                               &rmod->name, hfunc->name);

                resolver->functype_free(hfunc->wrt_functype);
                ngx_pfree(resolver->pool, hfunc);
            }

            ngx_wasm_hash_keys_array_cleanup(rmod->hfuncs_names);

            ngx_pfree(resolver->temp_pool, rmod->hfuncs_names);
            ngx_pfree(resolver->pool, rmod->name.data);
            ngx_pfree(resolver->pool, rmod->hfuncs);
            ngx_pfree(resolver->pool, rmod);
        }

        ngx_wasm_hash_keys_array_cleanup(resolver->modules_names);

        ngx_pfree(resolver->temp_pool, resolver->modules_names);
    }

    if (resolver->temp_pool) {
        ngx_destroy_pool(resolver->temp_pool);
    }

    ngx_pfree(resolver->pool, resolver);
}
