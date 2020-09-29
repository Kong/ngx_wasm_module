/*
 * Copyright (C) Thibault Charbonnier
 */

#ifndef _NGX_WASM_UTIL_H_INCLUDED_
#define _NGX_WASM_UTIL_H_INCLUDED_


#include <ngx_core.h>
#include <ngx_wasm.h>


ngx_int_t ngx_wasm_conf_parse_phase(ngx_conf_t *cf, u_char *str,
    ngx_uint_t module_type);

void ngx_wasm_log_error(ngx_uint_t level, ngx_log_t *log, ngx_err_t err,
#if (NGX_HAVE_VARIADIC_MACROS)
    const char *fmt, ...);

#else
    const char *fmt, va_list args);
#endif


/* named rbnode */


typedef struct {
    ngx_rbtree_node_t   node;
    ngx_str_t          *name;
} ngx_wasm_nn_t;

void ngx_wasm_rbtree_insert_nn(ngx_rbtree_node_t *temp,
    ngx_rbtree_node_t *node, ngx_rbtree_node_t *sentinel);

#define ngx_wasm_nn_init(nn, str)                                           \
    (nn)->node.key = ngx_crc32_short((str)->data, (str)->len);             \
    (nn)->name = (str)

#define ngx_wasm_nn_rbtree_insert(rbtree, nn)                                \
    ngx_rbtree_insert((rbtree), &(nn)->node)

ngx_wasm_nn_t *ngx_wasm_nn_rbtree_lookup(ngx_rbtree_t *rbtree,
    u_char *name, size_t len);

#define ngx_wasm_nn_n2nn(n)                                                  \
    (ngx_wasm_nn_t *) ((u_char *) (n) - offsetof(ngx_wasm_nn_t, node))

#define ngx_wasm_nn_data(nn, type, member)                                   \
    (type *) ((u_char *) &(nn)->node - offsetof(type, member))


#endif /* _NGX_WASM_UTIL_H_INCLUDED_ */
