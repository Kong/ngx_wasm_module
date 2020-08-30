/*
 * Copyright (C) Thibault Charbonnier
 */

#ifndef _NGX_WASM_HFUNCS_H_INCLUDED_
#define _NGX_WASM_HFUNCS_H_INCLUDED_


#include <ngx_core.h>
#include <ngx_wasm.h>


/* host functions */


struct ngx_wasm_hfunc_s {
    ngx_str_t               *name;
    ngx_wasm_dfunc_pt        ptr;
    ngx_wasm_val_kind        args[NGX_WASM_ARGS_MAX];
    ngx_wasm_val_kind        rets[NGX_WASM_RETS_MAX];
    ngx_uint_t               nargs;
    ngx_uint_t               nrets;
    ngx_wrt_functype_pt      wrt_functype;
};


/* store */


typedef struct {
    ngx_rbtree_t             rbtree;
    ngx_rbtree_node_t        sentinel;
    ngx_cycle_t             *cycle;
    ngx_pool_t              *pool;
} ngx_wasm_hfuncs_store_t;


ngx_wasm_hfuncs_store_t *ngx_wasm_hfuncs_store_new(ngx_cycle_t *cycle);

void ngx_wasm_hfuncs_store_add(ngx_wasm_hfuncs_store_t *store,
    const char *module, const ngx_wasm_hfunc_decl_t decls[]);

void ngx_wasm_hfuncs_store_free(ngx_wasm_hfuncs_store_t *store);


/* resolver */


typedef struct {
    ngx_str_t                   name;
    ngx_hash_t                 *hfuncs;
    ngx_hash_keys_arrays_t     *hfuncs_names;
} ngx_wasm_hfuncs_module_t;


struct ngx_wasm_hfuncs_resolver_s {
    ngx_pool_t                 *pool;
    ngx_pool_t                 *temp_pool;
    ngx_log_t                  *log;
    ngx_hash_t                 *modules;
    ngx_hash_keys_arrays_t     *modules_names;
    ngx_wrt_hfunctype_new_pt    functype_new;
    ngx_wrt_hfunctype_free_pt   functype_free;
};


ngx_wasm_hfuncs_resolver_t *ngx_wasm_hfuncs_resolver_new(ngx_cycle_t *cycle,
    ngx_wasm_hfuncs_store_t *store, ngx_wrt_t *runtime);

ngx_wasm_hfunc_t *ngx_wasm_hfuncs_resolver_lookup(
    ngx_wasm_hfuncs_resolver_t *resolver, u_char *mod_name, size_t mod_len,
    u_char *func_name, size_t func_len);

void ngx_wasm_hfuncs_resolver_free(ngx_wasm_hfuncs_resolver_t *resolver);


#endif /* _NGX_WASM_HFUNCS_H_INCLUDED_ */
