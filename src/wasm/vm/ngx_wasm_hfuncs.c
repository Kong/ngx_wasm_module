/*
 * Copyright (C) Thibault Charbonnier
 */

#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_wasm_hfuncs.h>
#include <ngx_wasm_util.h>


ngx_wasm_hfuncs_t *
ngx_wasm_hfuncs_new(ngx_cycle_t *cycle)
{
    ngx_wasm_hfuncs_t  *hfuncs;

    hfuncs = ngx_pcalloc(cycle->pool, sizeof(ngx_wasm_hfuncs_t));
    if (hfuncs == NULL) {
        goto failed;
    }

    hfuncs->cycle = cycle;
    hfuncs->pool = cycle->pool;

    ngx_rbtree_init(&hfuncs->mtree, &hfuncs->sentinel,
                    ngx_str_rbtree_insert_value);

    ngx_log_debug1(NGX_LOG_DEBUG_WASM, cycle->log, 0,
                   "wasm new hfuncs store (hfuncs: %p)", hfuncs);

    return hfuncs;

failed:

    ngx_wasm_log_error(NGX_LOG_EMERG, cycle->log, 0,
                       "failed to allocate hfuncs store");

    return NULL;
}


void
ngx_wasm_hfuncs_add(ngx_wasm_hfuncs_t *hfuncs, u_char *mname,
    ngx_wasm_hdecls_t *hdecls)
{
    size_t                    nargs, nrets;
    u_char                   *err = NULL;
    ngx_str_t                 mod_name;
    ngx_str_node_t           *sn;
    ngx_wasm_hdecl_t         *hdecl;
    ngx_wasm_hfunc_t         *hf;
    ngx_wasm_hfuncs_fnode_t  *fnode;
    ngx_wasm_hfuncs_mnode_t  *mnode;

    mod_name.len = ngx_strlen(mname);
    mod_name.data = mname;

    sn = ngx_wasm_sn_rbtree_lookup(&hfuncs->mtree, &mod_name);
    if (sn) {
        mnode = ngx_wasm_sn_sn2data(sn, ngx_wasm_hfuncs_mnode_t, sn);

    } else {
        mnode = ngx_pcalloc(hfuncs->pool, sizeof(ngx_wasm_hfuncs_mnode_t));
        if (mnode == NULL) {
            err = (u_char *) "no memory";
            goto failed;
        }

        mnode->name.len = mod_name.len;
        mnode->name.data = mod_name.data;

        ngx_rbtree_init(&mnode->ftree, &mnode->sentinel,
                        ngx_str_rbtree_insert_value);

        ngx_wasm_sn_init(&mnode->sn, &mnode->name);
        ngx_wasm_sn_rbtree_insert(&hfuncs->mtree, &mnode->sn);
    }

    hdecl = hdecls->hdecls;

    for (/* void */; hdecl->ptr; hdecl++) {

        ngx_log_debug2(NGX_LOG_DEBUG_WASM, hfuncs->cycle->log, 0,
                       "wasm registering \"%V.%V\" host function",
                       &mnode->name, &hdecl->name);

        sn = ngx_wasm_sn_rbtree_lookup(&mnode->ftree, &hdecl->name);
        if (sn) {
            ngx_sprintf(err, "\"%V.%V\" already defined",
                        &mnode->name, &hdecl->name);
            goto failed;
        }

        hf = ngx_pcalloc(hfuncs->pool, sizeof(ngx_wasm_hfunc_t));
        if (hf == NULL) {
            err = (u_char *) "no memory";
            goto failed;
        }

        hf->name = &hdecl->name;
        hf->ptr = hdecl->ptr;
        hf->subsys = hdecls->subsys;

        for (nargs = 0; hdecl->args[nargs] && nargs < NGX_WASM_ARGS_MAX;
             nargs++) { /* void */ }

        for (nrets = 0; hdecl->rets[nrets] && nrets < NGX_WASM_RETS_MAX;
             nrets++) { /* void */ }

        hf->args.size = nargs;
        hf->args.vals = ngx_palloc(hfuncs->pool,
                                   nargs * sizeof(ngx_wasm_val_kind));
        if (hf->args.vals == NULL) {
            err = (u_char *) "no memory";
            goto failed;
        }

        hf->rets.size = nrets;
        hf->rets.vals = ngx_palloc(hfuncs->pool,
                                   nrets * sizeof(ngx_wasm_val_kind));
        if (hf->rets.vals == NULL) {
            err = (u_char *) "no memory";
            goto failed;
        }

        for (nargs = 0; nargs < hf->args.size; nargs++) {
            hf->args.vals[nargs] = hdecl->args[nargs];
        }

        for (nrets = 0; nrets < hf->rets.size; nrets++) {
            hf->rets.vals[nrets] = hdecl->rets[nrets];
        }

        fnode = ngx_pcalloc(hfuncs->pool, sizeof(ngx_wasm_hfuncs_fnode_t));
        if (fnode == NULL) {
            err = (u_char *) "no memory";
            goto failed;
        }

        fnode->hfunc = hf;

        ngx_wasm_sn_init(&fnode->sn, hf->name);
        ngx_wasm_sn_rbtree_insert(&mnode->ftree, &fnode->sn);
    }

    return;

failed:

    ngx_wasm_log_error(NGX_LOG_EMERG, hfuncs->cycle->log, 0,
                       "failed to register \"%*s\" host functions: %s",
                       mod_name, err);
}


void
ngx_wasm_hfuncs_init(ngx_wasm_hfuncs_t *hfuncs, ngx_wrt_t *runtime)
{
    if (hfuncs->hfunctype_new) {
        ngx_wasm_log_error(NGX_LOG_WARN, hfuncs->cycle->log, 0,
                           "hfuncs store already initialized (hfuncs: %p)",
                           hfuncs);
        return;
    }

    hfuncs->hfunctype_new = runtime->hfunctype_new;
    hfuncs->hfunctype_free = runtime->hfunctype_free;
}


ngx_wasm_hfunc_t *
ngx_wasm_hfuncs_lookup(ngx_wasm_hfuncs_t *hfuncs, u_char *mname,
    size_t mlen, u_char *fname, size_t flen)
{
    ngx_str_t                 mod_name, func_name;
    ngx_str_node_t           *sn;
    ngx_wasm_hfunc_t         *hf;
    ngx_wasm_hfuncs_mnode_t  *mnode;
    ngx_wasm_hfuncs_fnode_t  *fnode;

    mod_name.len = mlen;
    mod_name.data = mname;

    sn = ngx_wasm_sn_rbtree_lookup(&hfuncs->mtree, &mod_name);
    if (sn == NULL) {
        return NULL;
    }

    mnode = ngx_wasm_sn_sn2data(sn, ngx_wasm_hfuncs_mnode_t, sn);

    func_name.len = flen;
    func_name.data = fname;

    sn = ngx_wasm_sn_rbtree_lookup(&mnode->ftree, &func_name);
    if (sn == NULL) {
        return NULL;
    }

    fnode = ngx_wasm_sn_sn2data(sn, ngx_wasm_hfuncs_fnode_t, sn);

    hf = fnode->hfunc;

    if (hf->wrt_functype == NULL) {
        if (hfuncs->hfunctype_new == NULL) {
            ngx_wasm_log_error(NGX_LOG_EMERG, hfuncs->cycle->log, 0,
                               "failed to initialize \"%V.%V\" host "
                               "function: host functions store not "
                               "initialized (hfuncs: %p)",
                               &mnode->name, &hf->name, hfuncs);
            return NULL;
        }

        hf->wrt_functype = hfuncs->hfunctype_new(hf);
        if (hf->wrt_functype == NULL) {
            ngx_wasm_log_error(NGX_LOG_EMERG, hfuncs->cycle->log, 0,
                               "failed to initialize \"%V.%V\" host function",
                               &mnode->name, &hf->name);
            return NULL;
        }
    }

    return hf;
}


void
ngx_wasm_hfuncs_free(ngx_wasm_hfuncs_t *hfuncs)
{
    ngx_rbtree_node_t        **root, **sentinel, *node,
                             **root2, **sentinel2;
    ngx_str_node_t            *sn;
    ngx_wasm_hfunc_t          *hf;
    ngx_wasm_hfuncs_mnode_t   *mnode;
    ngx_wasm_hfuncs_fnode_t   *fnode;

    ngx_log_debug1(NGX_LOG_DEBUG_WASM, hfuncs->cycle->log, 0,
                   "wasm free hfuncs store (hfuncs: %p)",
                   hfuncs);

    root = &hfuncs->mtree.root;
    sentinel = &hfuncs->mtree.sentinel;

    while (*root != *sentinel) {
        node = ngx_rbtree_min(*root, *sentinel);
        sn = ngx_wasm_sn_n2sn(node);
        mnode = ngx_wasm_sn_sn2data(sn, ngx_wasm_hfuncs_mnode_t, sn);

        ngx_rbtree_delete(&hfuncs->mtree, node);

        ngx_log_debug2(NGX_LOG_DEBUG_WASM, hfuncs->cycle->log, 0,
                       "wasm free \"%V\" host functions (hfuncs: %p)",
                       &mnode->name, hfuncs);

        root2 = &mnode->ftree.root;
        sentinel2 = &mnode->ftree.sentinel;

        while (*root2 != *sentinel2) {
            node = ngx_rbtree_min(*root2, *sentinel2);
            sn = ngx_wasm_sn_n2sn(node);
            fnode = ngx_wasm_sn_sn2data(sn, ngx_wasm_hfuncs_fnode_t, sn);

            hf = fnode->hfunc;

            ngx_log_debug3(NGX_LOG_DEBUG_WASM, hfuncs->cycle->log, 0,
                           "wasm free \"%V.%V\" host function (hfuncs: %p)",
                           &mnode->name, hf->name, hfuncs);

            ngx_rbtree_delete(&mnode->ftree, node);

            if (hfuncs->hfunctype_free && hf->wrt_functype) {
                hfuncs->hfunctype_free(hf->wrt_functype);
            }

            ngx_pfree(hfuncs->pool, hf->args.vals);
            ngx_pfree(hfuncs->pool, hf->rets.vals);
            ngx_pfree(hfuncs->pool, hf);
        }

        ngx_pfree(hfuncs->pool, mnode);
    }

    ngx_pfree(hfuncs->pool, hfuncs);
}


void
ngx_wasm_hctx_trapmsg(ngx_wasm_hctx_t *hctx,
#if (NGX_HAVE_VARIADIC_MACROS)
    const char *fmt, ...)

#else
    const char *fmt, va_list args)
#endif
{
    u_char               *p;
    static const size_t   maxlen = NGX_WASM_MAX_HOST_TRAP_STR - 1;

#if (NGX_HAVE_VARIADIC_MACROS)
    va_list  args;

    va_start(args, fmt);
    p = ngx_vsnprintf(&hctx->trapmsg[0], maxlen, fmt, args);
    va_end(args);

#else
    p = ngx_vsnprintf(&hctx->trapmsg[0], maxlen, fmt, args);
#endif

    *p++ = '\0';

    hctx->trapmsglen = p - hctx->trapmsg;
}
