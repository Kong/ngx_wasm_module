/*
 * Copyright (C) Thibault Charbonnier
 */

#ifndef _NGX_WASM_UTIL_H_INCLUDED_
#define _NGX_WASM_UTIL_H_INCLUDED_


#include <ngx_core.h>
#include <ngx_wasm.h>


typedef struct {
    ngx_rbtree_node_t   node;
    ngx_str_t          *name;
} ngx_wasm_rbtree_named_node_t;


void ngx_wasm_rbtree_set_named_node(ngx_wasm_rbtree_named_node_t *nn,
    ngx_str_t *name);

void ngx_wasm_rbtree_insert_named_node(ngx_rbtree_node_t *temp,
    ngx_rbtree_node_t *node, ngx_rbtree_node_t *sentinel);

ngx_rbtree_node_t *ngx_wasm_rbtree_lookup_named_node(ngx_rbtree_t *rbtree,
    u_char *name, size_t len);

void ngx_wasm_log_error(ngx_uint_t level, ngx_log_t *log, ngx_err_t err,
#if (NGX_HAVE_VARIADIC_MACROS)
    const char *fmt, ...);

#else
    const char *fmt, va_list args);
#endif


#endif /* _NGX_WASM_UTIL_H_INCLUDED_ */
