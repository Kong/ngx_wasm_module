#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_wavm.h>


#define ngx_wavm_hfuncs_tree_init(t)                                         \
    ngx_rbtree_init(&t->rbtree, &t->sentinel, ngx_str_rbtree_insert_value)


const wasm_valkind_t  ngx_wavm_i32 = WASM_I32;

const wasm_valkind_t * ngx_wavm_arity_i32[] = {
    &ngx_wavm_i32,
    NULL
};

const wasm_valkind_t * ngx_wavm_arity_i32_i32[] = {
    &ngx_wavm_i32, &ngx_wavm_i32,
    NULL
};

const wasm_valkind_t * ngx_wavm_arity_i32_i32_i32[] = {
    &ngx_wavm_i32, &ngx_wavm_i32, &ngx_wavm_i32,
    NULL
};


static void
ngx_wavm_hfuncs_tree_destroy(ngx_wavm_hfuncs_t *hfuncs,
    ngx_wavm_hfuncs_tree_t *t)
{
    ngx_rbtree_node_t  **root, **sentinel, *node;
    ngx_str_node_t      *sn;
    ngx_wavm_hfunc_t    *hfunc;

    root = &t->rbtree.root;
    sentinel = &t->rbtree.sentinel;

    while (*root != *sentinel) {
        node = ngx_rbtree_min(*root, *sentinel);
        sn = ngx_wasm_sn_n2sn(node);
        hfunc = ngx_wasm_sn_sn2data(sn, ngx_wavm_hfunc_t, sn);

        ngx_log_debug1(NGX_LOG_DEBUG_WASM, hfuncs->log, 0,
                       "wasm free \"%V\" host function",
                       &hfunc->def->name);

        wasm_functype_delete(hfunc->functype);

        ngx_rbtree_delete(&t->rbtree, node);

        ngx_pfree(hfuncs->pool, hfunc);
    }
}


ngx_wavm_hfuncs_t *
ngx_wavm_hfuncs_new(ngx_cycle_t *cycle)
{
    size_t              i;
    ngx_wavm_hfuncs_t  *hfuncs;

    hfuncs = ngx_pcalloc(cycle->pool, sizeof(ngx_wavm_hfuncs_t));
    if (hfuncs == NULL) {
        return NULL;
    }

    hfuncs->global = ngx_wasm_max_module + 1;
    hfuncs->pool = cycle->pool;
    hfuncs->log = &cycle->new_log;
    hfuncs->trees = ngx_pcalloc(hfuncs->pool, sizeof(void *) * hfuncs->global);
    if (hfuncs->trees == NULL) {
        ngx_wavm_hfuncs_free(hfuncs);
        return NULL;
    }

    for (i = 0; i < hfuncs->global; i++) {
        hfuncs->trees[i] = ngx_palloc(hfuncs->pool,
                                      sizeof(ngx_wavm_hfuncs_tree_t));
        if (hfuncs->trees[i] == NULL) {
            ngx_wavm_hfuncs_free(hfuncs);
            return NULL;
        }

        ngx_wavm_hfuncs_tree_init(hfuncs->trees[i]);
    }

    return hfuncs;
}


void
ngx_wavm_hfuncs_free(ngx_wavm_hfuncs_t *hfuncs)
{
    size_t                   i;
    ngx_wavm_hfuncs_tree_t  *tree;

    if (hfuncs->trees) {
        for (i = 0; i < hfuncs->global; i++) {
            tree = (ngx_wavm_hfuncs_tree_t *) hfuncs->trees[i];
            if (tree) {
                ngx_wavm_hfuncs_tree_destroy(hfuncs, tree);
                ngx_pfree(hfuncs->pool, tree);
            }
        }

        ngx_pfree(hfuncs->pool, hfuncs->trees);
    }

    ngx_pfree(hfuncs->pool, hfuncs);
}


static void
ngx_wavm_hfuncs_kindvec2typevec(const wasm_valkind_t **valkinds,
    wasm_valtype_vec_t *out)
{
    size_t           i;
    wasm_valkind_t  *valkind;

    if (valkinds == NULL) {
        wasm_valtype_vec_new_empty(out);
        return;
    }

    for (i = 0; ((wasm_valkind_t **) valkinds)[i]; i++) { /* void */ }

    wasm_valtype_vec_new_uninitialized(out, i);

    for (i = 0; i < out->size; i++) {
        valkind = ((wasm_valkind_t **) valkinds)[i];
        ngx_wasm_assert(valkind != NULL);
        out->data[i] = wasm_valtype_new(*valkind);
    }
}


ngx_int_t
ngx_wavm_hfuncs_add(ngx_wavm_hfuncs_t *hfuncs, ngx_wavm_hfunc_def_t *hdefs,
    ngx_module_t *m)
{
    u_char                  *err = NULL;
    ngx_str_node_t          *sn;
    ngx_wavm_hfuncs_tree_t  *tree;
    ngx_wavm_hfunc_t        *hfunc;
    ngx_wavm_hfunc_def_t    *hfunc_def;
    wasm_valtype_vec_t       args, rets;

    if (m == NULL) {
        tree = hfuncs->trees[hfuncs->global - 1];

    } else {
        tree = hfuncs->trees[m->ctx_index];
    }

    for (hfunc_def = hdefs; hfunc_def->ptr; hfunc_def++) {

        ngx_log_debug2(NGX_LOG_DEBUG_WASM, hfuncs->log, 0,
                       "wasm registering \"%V\" host function (m: %p)",
                       &hfunc_def->name, m);

        sn = ngx_wasm_sn_rbtree_lookup(&tree->rbtree, &hfunc_def->name);
        if (sn) {
            ngx_sprintf(err, "\"%V\" already defined", &hfunc_def->name);
            goto failed;
        }

        ngx_wavm_hfuncs_kindvec2typevec(hfunc_def->args, &args);
        ngx_wavm_hfuncs_kindvec2typevec(hfunc_def->rets, &rets);

        hfunc = ngx_pcalloc(hfuncs->pool, sizeof(ngx_wavm_hfunc_t));
        if (hfunc == NULL) {
            err = (u_char *) "no memory";
            goto failed;
        }

        hfunc->def = hfunc_def;
        hfunc->functype = wasm_functype_new(&args, &rets);

        ngx_wasm_sn_init(&hfunc->sn, &hfunc->def->name);
        ngx_wasm_sn_rbtree_insert(&tree->rbtree, &hfunc->sn);
    }

    return NGX_OK;

failed:

    ngx_wavm_log_error(NGX_LOG_EMERG, hfuncs->log, NULL, NULL,
                       "failed to register hfuncs functions: %s", err);

    return NGX_ERROR;
}


ngx_wavm_hfunc_t *
ngx_wavm_hfuncs_lookup(ngx_wavm_hfuncs_t *hfuncs, ngx_str_t *name,
    ngx_module_t *m)
{
    size_t           i = m ? m->ctx_index : 0;
    ngx_str_node_t  *sn;

    while (i < hfuncs->global) {
        sn = ngx_wasm_sn_rbtree_lookup(
                 &(((ngx_wavm_hfuncs_tree_t **) hfuncs->trees)[i]->rbtree),
                 name);
        if (sn) {
            return ngx_wasm_sn_sn2data(sn, ngx_wavm_hfunc_t, sn);
        }

        if (m) {
            /* skip to global store */
            i = hfuncs->global - 1;
            continue;
        }

        i++;
    };

    return NULL;
}


wasm_trap_t *
ngx_wavm_hfuncs_trampoline(void *env,
#if (NGX_WASM_HAVE_WASMTIME)
    const wasm_val_t args[], wasm_val_t rets[]
#else
    const wasm_val_vec_t* args, wasm_val_vec_t* rets
#endif
    )
{
    size_t                  errlen, len;
    char                   *err = NULL;
    u_char                 *p, *buf, trapbuf[NGX_WAVM_HFUNCS_MAX_TRAP_LEN];
    ngx_int_t               rc;
    ngx_wavm_hfunc_tctx_t  *tctx = env;
    ngx_wavm_instance_t    *instance = tctx->instance;
    ngx_wavm_hfunc_t       *hfunc = tctx->hfunc;
    wasm_val_t             *hargs, *hrets;
    wasm_byte_vec_t         trapmsg;
    wasm_trap_t            *trap = NULL;

#if (NGX_WASM_HAVE_WASMTIME)
    hargs = (wasm_val_t *) args;
    hrets = (wasm_val_t *) rets;

#else
    hargs = (wasm_val_t *) args->data;
    hrets = (wasm_val_t *) rets->data;
#endif

    ngx_log_debug2(NGX_LOG_DEBUG_WASM, instance->log, 0,
                   "wasm hfuncs trampoline (hfunc: %V, tctx: %p)",
                   &hfunc->def->name, tctx);

    ngx_str_null(&instance->trapmsg);

    instance->trapbuf = (u_char *) &trapbuf;
    instance->mem_offset = (u_char *) wasm_memory_data(instance->memory);

    rc = hfunc->def->ptr(instance, hargs, hrets);

    ngx_log_debug1(NGX_LOG_DEBUG_WASM, instance->log, 0,
                   "wasm hfuncs trampoline rc: %d", rc);

    switch (rc) {

    case NGX_WAVM_OK:
        break;

    case NGX_WAVM_ERROR:
        err = "nginx hfuncs error";
        goto trap;

    case NGX_WAVM_BAD_CTX:
        err = "bad context";
        goto trap;

    case NGX_WAVM_BAD_USAGE:
        err = "bad usage";
        goto trap;

    default:
        err = "NYI - hfuncs trampoline rc";
        goto trap;

    }

    return NULL;

trap:

    errlen = ngx_strlen(err);

    if (instance->trapmsg.len) {
        len = errlen + instance->trapmsg.len + 2;

        wasm_byte_vec_new_uninitialized(&trapmsg, len);

        buf = (u_char *) trapmsg.data;
        p = ngx_copy(buf, err, errlen);
        p = ngx_snprintf(p, len - (p - buf), ": %V", &instance->trapmsg);
        //*p++ = '\0'; /* trapmsg null terminated */

        ngx_wasm_assert( ((size_t) (p - buf)) == len );

    } else {
        wasm_byte_vec_new(&trapmsg, errlen, err);
    }

    trap = wasm_trap_new(instance->ctx->store, &trapmsg);
    wasm_byte_vec_delete(&trapmsg);

    return trap;
}


void
ngx_wavm_instance_trap_printf(ngx_wavm_instance_t *instance,
#if (NGX_HAVE_VARIADIC_MACROS)
    const char *fmt, ...)

#else
    const char *fmt, va_list args)
#endif
{
    u_char               *p;
    static const size_t   maxlen = NGX_WAVM_HFUNCS_MAX_TRAP_LEN - 1;

#if (NGX_HAVE_VARIADIC_MACROS)
    va_list  args;

    va_start(args, fmt);
    p = ngx_vsnprintf(instance->trapbuf, maxlen, fmt, args);
    va_end(args);

#else
    p = ngx_vsnprintf(instance->trapbuf, maxlen, fmt, args);
#endif

    *p++ = '\0';

    instance->trapmsg.len = p - instance->trapbuf;
    instance->trapmsg.data = (u_char *) instance->trapbuf;
}
