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


/* ngx_str_node_t extensions */


#define ngx_wasm_sn_rbtree_insert(rbtree, sn)                                \
    ngx_rbtree_insert((rbtree), &(sn)->node)

#define ngx_wasm_sn_init(sn, s)                                              \
    (sn)->node.key = ngx_crc32_long((s)->data, (s)->len);                    \
    (sn)->str.len = (s)->len;                                                \
    (sn)->str.data = (s)->data

#define ngx_wasm_sn_n2sn(n)                                                  \
    (ngx_str_node_t *) ((u_char *) (n) - offsetof(ngx_str_node_t, node))

#define ngx_wasm_sn_sn2data(sn, type, member)                                \
    (type *) ((u_char *) &(sn)->node - offsetof(type, member))


static ngx_inline ngx_str_node_t *
ngx_wasm_sn_rbtree_lookup(ngx_rbtree_t *rbtree, ngx_str_t *str)
{
    uint32_t   hash;

    hash = ngx_crc32_long(str->data, str->len);

    return ngx_str_rbtree_lookup(rbtree, str, hash);
}


#endif /* _NGX_WASM_UTIL_H_INCLUDED_ */
