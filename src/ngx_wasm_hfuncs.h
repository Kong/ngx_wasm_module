/*
 * Copyright (C) Thibault Charbonnier
 */

#ifndef _NGX_WASM_HFUNCS_H_INCLUDED_
#define _NGX_WASM_HFUNCS_H_INCLUDED_


#include <ngx_core.h>
#include <ngx_wasm.h>


/* store */


struct ngx_wasm_hfuncs_store_s {
    ngx_rbtree_t                   rbtree;
    ngx_rbtree_node_t              sentinel;
    ngx_cycle_t                   *cycle;
    ngx_pool_t                    *pool;
};


ngx_wasm_hfuncs_store_t *ngx_wasm_hfuncs_store_new(ngx_cycle_t *cycle);

void ngx_wasm_hfuncs_store_add(ngx_wasm_hfuncs_store_t *store,
    const char *module, const ngx_wasm_hfunc_decl_t decls[]);

void ngx_wasm_hfuncs_store_free(ngx_wasm_hfuncs_store_t *store);


/* resolver */


struct ngx_wasm_hfunc_s {
    ngx_str_t               *name;
    ngx_wasm_hfunc_pt        ptr;
    ngx_wasm_val_kind        args[NGX_WASM_ARGS_MAX];
    ngx_wasm_val_kind        rets[NGX_WASM_RETS_MAX];
    ngx_uint_t               nargs;
    ngx_uint_t               nrets;
    void                    *vm_data;
};


typedef struct {
    ngx_str_t               *name;
    ngx_hash_t              *hfuncs;
    ngx_hash_keys_arrays_t  *hfuncs_names;
} ngx_wasm_hfuncs_module_t;


struct ngx_wasm_hfuncs_resolver_s {
    ngx_pool_t               *pool;
    ngx_pool_t               *temp_pool;
    ngx_log_t                *log;
    ngx_wasm_hfuncs_store_t  *store;
    ngx_wasm_hfunc_new_pt     hf_new;
    ngx_wasm_hfunc_free_pt    hf_free;
    ngx_hash_t               *modules;
    ngx_hash_keys_arrays_t   *modules_names;
};


ngx_int_t ngx_wasm_hfuncs_resolver_init(ngx_wasm_hfuncs_resolver_t *resolver);

ngx_wasm_hfunc_t *ngx_wasm_hfuncs_resolver_lookup(
    ngx_wasm_hfuncs_resolver_t *resolver, char *module, size_t mlen,
    char *func, size_t flen);

void ngx_wasm_hfuncs_resolver_destroy(ngx_wasm_hfuncs_resolver_t *resolver);


#endif /* _NGX_WASM_HFUNCS_H_INCLUDED_ */
