/*
 * Copyright (C) Thibault Charbonnier
 */

#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_wasm_util.h>




void
ngx_wasm_rbtree_insert_nn(ngx_rbtree_node_t *temp,
    ngx_rbtree_node_t *node, ngx_rbtree_node_t *sentinel)
{
    ngx_rbtree_node_t  **p;
    ngx_wasm_nn_t       *nn, *nn_temp;

    for ( ;; ) {

        if (node->key < temp->key) {
            p = &temp->left;

        } else if (node->key > temp->key) {
            p = &temp->right;

        } else {
            nn = ngx_wasm_nn_n2nn(node);
            nn_temp = ngx_wasm_nn_n2nn(temp);

            p = (ngx_memn2cmp(nn->name->data, nn_temp->name->data,
                              nn->name->len, nn_temp->name->len)
                 < 0) ? &temp->left : &temp->right;
        }

        if (*p == sentinel) {
            break;
        }

        temp = *p;
    }

    *p = node;
    node->parent = temp;
    node->left = sentinel;
    node->right = sentinel;
    ngx_rbt_red(node);
}


ngx_wasm_nn_t *
ngx_wasm_nn_rbtree_lookup(ngx_rbtree_t *rbtree, u_char *name, size_t len)
{
    uint32_t            hash;
    ngx_int_t           rc;
    ngx_rbtree_node_t  *node, *sentinel;
    ngx_wasm_nn_t      *nn;

    hash = ngx_crc32_short(name, len);

    node = rbtree->root;
    sentinel = rbtree->sentinel;

    while (node != sentinel) {

        if (hash < node->key) {
            node = node->left;
            continue;
        }

        if (hash > node->key) {
            node = node->right;
            continue;
        }

        nn = ngx_wasm_nn_n2nn(node);

        rc = ngx_memn2cmp(name, nn->name->data, len, nn->name->len);

        if (rc == 0) {
            return nn;
        }

        node = (rc < 0) ? node->left : node->right;
    }

    return NULL;
}


void
ngx_wasm_log_error(ngx_uint_t level, ngx_log_t *log, ngx_err_t err,
#if (NGX_HAVE_VARIADIC_MACROS)
    const char *fmt, ...)

#else
    const char *fmt, va_list args)
#endif
{
#if (NGX_HAVE_VARIADIC_MACROS)
    va_list            args;
#endif
    u_char            *p, *last;
    u_char             errstr[NGX_MAX_ERROR_STR];

    last = errstr + NGX_MAX_ERROR_STR;
    p = &errstr[0];

#if (NGX_HAVE_VARIADIC_MACROS)
    va_start(args, fmt);
    p = ngx_vslprintf(p, last, fmt, args);
    va_end(args);

#else
    p = ngx_vslprintf(p, last, fmt, args);
#endif

    ngx_log_error_core(level, log, err, "[wasm] %*s", p - errstr, errstr);
}
