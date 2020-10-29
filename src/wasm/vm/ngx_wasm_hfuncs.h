/*
 * Copyright (C) Thibault Charbonnier
 */

#ifndef _NGX_WASM_HFUNCS_H_INCLUDED_
#define _NGX_WASM_HFUNCS_H_INCLUDED_


#include <ngx_core.h>
#include <ngx_wasm_core.h>


typedef struct {
    ngx_str_node_t                 sn;
    ngx_wasm_hfunc_t              *hfunc;
} ngx_wasm_hfuncs_fnode_t;


typedef struct {
    ngx_str_t                      name;
    ngx_rbtree_t                   ftree;
    ngx_rbtree_node_t              sentinel;
    ngx_str_node_t                 sn;
} ngx_wasm_hfuncs_mnode_t;


struct ngx_wasm_hfuncs_s {
    ngx_cycle_t                   *cycle;
    ngx_pool_t                    *pool;
    ngx_rbtree_t                   mtree;
    ngx_rbtree_node_t              sentinel;
    ngx_wrt_hfunctype_new_pt       hfunctype_new;
    ngx_wrt_hfunctype_free_pt      hfunctype_free;
};


ngx_wasm_hfuncs_t *ngx_wasm_hfuncs_new(ngx_cycle_t *cycle);

void ngx_wasm_hfuncs_add(ngx_wasm_hfuncs_t *hfuncs, u_char *mname,
    ngx_wasm_hdecls_t *hdecls);

void ngx_wasm_hfuncs_init(ngx_wasm_hfuncs_t *hfuncs, ngx_wrt_t *runtime);

void ngx_wasm_hfuncs_free(ngx_wasm_hfuncs_t *hfuncs);


#endif /* _NGX_WASM_HFUNCS_H_INCLUDED_ */
