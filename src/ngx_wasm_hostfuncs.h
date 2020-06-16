/*
 * Copyright (C) Thibault Charbonnier
 */

#ifndef _NGX_WASM_HOSTFUNCS_H_INCLUDED_
#define _NGX_WASM_HOSTFUNCS_H_INCLUDED_


#include <ngx_wasm.h>


#define NGX_WASM_NO_ARGS            { 0, 0, 0, 0, 0, 0, 0, 0 }
#define NGX_WASM_NO_RETS            { 0 }

#define NGX_WASM_ARGS_I32_I32_I32                                           \
    { NGX_WASM_I32, NGX_WASM_I32, NGX_WASM_I32, 0, 0, 0, 0, 0 }

#define NGX_WASM_RETS_I32                                                   \
    { NGX_WASM_I32, 0, 0, 0, 0, 0, 0, 0 }

#define ngx_wasm_hostfuncs_null                                             \
    { ngx_null_string, NULL, NGX_WASM_NO_ARGS, NGX_WASM_NO_RETS }


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


ngx_int_t ngx_wasm_hostfuncs_new(ngx_log_t *log);

void ngx_wasm_hostfuncs_register(const char *namespace,
    ngx_wasm_hostfunc_t *hfuncs);

ngx_int_t ngx_wasm_hostfuncs_init();

ngx_wasm_hostfuncs_namespaces_t *ngx_wasm_hostfuncs_namespaces();


#endif /* _NGX_WASM_HOSTFUNCS_H_INCLUDED_ */


/* vi:set ft=c ts=4 sw=4 et fdm=marker: */
