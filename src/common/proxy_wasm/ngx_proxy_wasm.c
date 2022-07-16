#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_proxy_wasm.h>
#ifdef NGX_WASM_HTTP
#include <ngx_http_proxy_wasm.h>
#endif


static ngx_int_t ngx_proxy_wasm_ctx_action(ngx_proxy_wasm_ctx_t *pwctx,
    ngx_proxy_wasm_filter_ctx_t *fctx, ngx_uint_t *ret);
static void ngx_proxy_wasm_ctx_free(ngx_proxy_wasm_ctx_t *pwctx);
static void ngx_proxy_wasm_instance_destroy(
    ngx_proxy_wasm_instance_ctx_t *ictx);
static ngx_inline ngx_wavm_funcref_t *ngx_proxy_wasm_filter_func_lookup(
    ngx_proxy_wasm_filter_t *filter, const char *n);
static ngx_proxy_wasm_abi_version_e ngx_proxy_wasm_filter_abi_version(
    ngx_proxy_wasm_filter_t *filter);
static ngx_proxy_wasm_filter_ctx_t *ngx_proxy_wasm_root_ctx_lookup(
    ngx_proxy_wasm_instance_ctx_t *ictx, ngx_uint_t id);
static ngx_int_t ngx_proxy_wasm_start(ngx_proxy_wasm_filter_ctx_t *fctx);
static void ngx_proxy_wasm_on_log(ngx_proxy_wasm_filter_ctx_t *fctx);
static void ngx_proxy_wasm_on_done(ngx_proxy_wasm_filter_ctx_t *fctx);
static ngx_int_t ngx_proxy_wasm_on_timer_tick(ngx_proxy_wasm_filter_ctx_t *fctx);


ngx_proxy_wasm_ctx_t *
ngx_proxy_wasm_ctx(ngx_proxy_wasm_filter_t *filter, void *data)
{
    ngx_proxy_wasm_ctx_t  *pwctx;

    pwctx = filter->get_context_(filter, data);
    if (pwctx == NULL) {
        return NULL;
    }

    if (pwctx->data == NULL) {
        ngx_wasm_assert(pwctx->pool && pwctx->log);

        pwctx->data = data;
        pwctx->n_filters = *filter->n_filters;

        ngx_array_init(&pwctx->fctxs, pwctx->pool, pwctx->n_filters,
                       sizeof(ngx_proxy_wasm_filter_ctx_t));

        ngx_proxy_wasm_store_init(&pwctx->store, pwctx->pool);
    }

    return pwctx;
}


ngx_int_t
ngx_proxy_wasm_ctx_add(ngx_proxy_wasm_ctx_t *pwctx,
    ngx_proxy_wasm_filter_t *filter)
{
    ngx_proxy_wasm_filter_ctx_t    *fctx;
    ngx_proxy_wasm_instance_ctx_t  *ictx = NULL;
    ngx_proxy_wasm_store_t         *pwstore = NULL;

    if (filter->index < pwctx->fctxs.nelts) {
#if (NGX_DEBUG)
        fctx = &((ngx_proxy_wasm_filter_ctx_t *)
                 pwctx->fctxs.elts)[filter->index];

        ngx_wasm_assert(fctx->filter == filter);
#endif
        return NGX_DONE;
    }

    /* get instance */

    switch (*filter->isolation) {
    case NGX_PROXY_WASM_ISOLATION_NONE:
        pwstore = filter->store;
        break;
    case NGX_PROXY_WASM_ISOLATION_STREAM:
        pwstore = &pwctx->store;
        break;
    case NGX_PROXY_WASM_ISOLATION_FILTER:
        break;
    default:
        ngx_wasm_log_error(NGX_LOG_WASM_NYI, pwctx->log, 0,
                           "NYI - instance isolation: %d",
                           *filter->isolation);
        return NGX_ERROR;
    }

    ictx = ngx_proxy_wasm_instance_get(filter, pwstore, pwctx->log);
    if (ictx == NULL) {
        return NGX_ERROR;
    }

    /* add filter ctx */

    fctx = ngx_array_push(&pwctx->fctxs);
    if (fctx == NULL) {
        return NGX_ERROR;
    }

    ngx_memzero(fctx, sizeof(ngx_proxy_wasm_filter_ctx_t));

    fctx->id = ++ictx->next_context_id;
    fctx->root_id = filter->id;
    fctx->pool = pwctx->pool;
    fctx->log = pwctx->log;
    fctx->ictx = ictx;
    fctx->parent = pwctx;
    fctx->filter = filter;
    fctx->node.key = fctx->id;

    ngx_wasm_assert(filter->index == pwctx->fctxs.nelts - 1);

    return pwctx->fctxs.nelts == pwctx->n_filters
           ? NGX_DONE
           : NGX_OK;
}


ngx_int_t
ngx_proxy_wasm_ctx_resume(ngx_proxy_wasm_ctx_t *pwctx,
    ngx_wasm_phase_t *phase, ngx_proxy_wasm_step_e step)
{
    size_t                          i;
    ngx_int_t                       rc = NGX_OK;
    ngx_uint_t                      next_action;
    ngx_proxy_wasm_filter_t        *filter;
    ngx_proxy_wasm_filter_ctx_t    *fctx, *fctxs;
    ngx_proxy_wasm_instance_ctx_t  *ictx;

    pwctx->phase = phase;
    pwctx->step = step;

    next_action = NGX_PROXY_WASM_ACTION_CONTINUE;

    /* resume filters chain */

    fctxs = (ngx_proxy_wasm_filter_ctx_t *) pwctx->fctxs.elts;

    for (i = pwctx->cur_filter_idx; i < pwctx->fctxs.nelts; i++) {
        fctx = &fctxs[i];
        filter = fctx->filter;
        ictx = fctx->ictx;

        /* bubble-down filter error */

        if (filter->ecode) {
            fctx->ecode = filter->ecode;
        }

        /* check for trap */

#if 0
        if (ictx->instance->trapped) {
            if (step == NGX_PROXY_WASM_STEP_INIT_CTX
                && *filter->isolation == NGX_PROXY_WASM_ISOLATION_NONE)
            {
                /* recycle global instance */

                ngx_log_debug3(NGX_LOG_DEBUG_WASM, pwctx->log, 0,
                               "proxy_wasm \"%V\" filter (%l/%l) "
                               "recycling trapped global instance",
                               filter->name, filter->index + 1,
                               pwctx->n_filters);

                ngx_proxy_wasm_instance_release(ictx, 0);

                ictx = ngx_proxy_wasm_instance_get(filter, filter->store,
                                                   pwctx->log);
                if (ictx == NULL) {
                    rc = NGX_ERROR;
                    goto ret;
                }

                fctx->ictx = ictx;

            } else if (!fctx->ecode) {
                fctx->ecode = NGX_PROXY_WASM_ERR_INSTANCE_TRAPPED;
            }
        }
#else
        if (ictx->instance->trapped && !fctx->ecode) {
            fctx->ecode = NGX_PROXY_WASM_ERR_INSTANCE_TRAPPED;
        }
#endif

        /* check for yielded state */

        rc = ngx_proxy_wasm_ctx_action(pwctx, fctx, &next_action);
        if (rc != NGX_OK) {
            goto ret;
        }

        ngx_log_debug5(NGX_LOG_DEBUG_WASM, pwctx->log, 0,
                       "proxy_wasm \"%V\" filter (%l/%l)"
                       " resuming in \"%V\" phase (step: %l)",
                       filter->name, i + 1, pwctx->n_filters, &phase->name,
                       step);

        rc = ngx_proxy_wasm_resume(ictx, filter, fctx, step, &next_action);
        if (rc == NGX_ERROR) {
            rc = filter->ecode_(fctx->ecode);
            goto ret;
        }

#if 0
        if (rc == NGX_DONE) {
            ngx_log_debug0(NGX_LOG_DEBUG_WASM, pwctx->log, 0,
                           "proxy_wasm filter chain stopped");
            goto ret;
        }
#endif

        ngx_wasm_assert(rc == NGX_OK);

        /* check for yield/done */

        rc = ngx_proxy_wasm_ctx_action(pwctx, fctx, &next_action);
        if (rc != NGX_OK) {
            goto ret;
        }

        pwctx->cur_filter_idx = i + 1;

        dd("next filter");
    }

    ngx_wasm_assert(rc == NGX_OK);

#if 0
    if (pwctx->fctxs.nelts < pwctx->n_filters) {
        /* incomplete chain */
        goto ret;
    }
#endif

    /* next step */

    pwctx->cur_filter_idx = 0;

ret:

    if (step == NGX_PROXY_WASM_STEP_DONE) {
        ngx_proxy_wasm_ctx_free(pwctx);
    }

    return rc;
}


static ngx_int_t
ngx_proxy_wasm_ctx_action(ngx_proxy_wasm_ctx_t *pwctx,
    ngx_proxy_wasm_filter_ctx_t *fctx, ngx_uint_t *ret)
{
    ngx_int_t                     rc = NGX_ERROR;
    ngx_proxy_wasm_filter_t      *filter;
    ngx_proxy_wasm_filter_ctx_t  *yfctx, *fctxs;

    if (fctx->ecode) {
        filter = fctx->filter;

        if (!fctx->ecode_logged
            && pwctx->step != NGX_PROXY_WASM_STEP_DONE)
        {
            ngx_proxy_wasm_log_error(NGX_LOG_WARN, pwctx->log, fctx->ecode,
                                     "proxy_wasm \"%V\" filter (%l/%l)"
                                     " failed resuming",
                                     filter->name, filter->index + 1,
                                     pwctx->n_filters);

            fctx->ecode_logged = 1;
        }
#if (NGX_DEBUG)
        else {
            ngx_log_debug4(NGX_LOG_DEBUG_WASM, pwctx->log, 0,
                           "proxy_wasm \"%V\" filter (%l/%l)"
                           " skipped in \"%V\" phase",
                           filter->name, filter->index + 1, pwctx->n_filters,
                           &pwctx->phase->name);
        }
#endif

        rc = filter->ecode_(fctx->ecode);
        goto error;
    }

    /* check for flags set by host */

    switch (pwctx->action) {
    case NGX_PROXY_WASM_ACTION_PAUSE:
        goto yield;
    case NGX_PROXY_WASM_ACTION_DONE:
        /* resume later phases */
        pwctx->action = NGX_PROXY_WASM_ACTION_CONTINUE;
        rc = NGX_DONE;
        goto done;
    default:
        break;
    }

    /* check for next action return value */

    if (ret) {

        /* set next action */

        switch (*ret) {
        case NGX_PROXY_WASM_ACTION_PAUSE:
            dd("next action: yield");
            break;
        case NGX_PROXY_WASM_ACTION_CONTINUE:
            dd("next action: continue");
            break;
        default:
            ngx_proxy_wasm_log_error(NGX_LOG_WASM_NYI, pwctx->log, 0,
                                     "NYI - proxy_wasm next action: %d", ret);
            goto error;
        }

        pwctx->action = *ret;
    }

    /* exceptional steps */

    switch (pwctx->step) {
    case NGX_PROXY_WASM_STEP_DONE:
        /* force-resume the done step */
        goto cont;
    default:
        break;
    }

    /* determine current action rc */

    switch (pwctx->action) {

    case NGX_PROXY_WASM_ACTION_CONTINUE:
        goto cont;

    case NGX_PROXY_WASM_ACTION_PAUSE:
        switch (pwctx->phase->index) {
#ifdef NGX_WASM_HTTP
        case NGX_HTTP_REWRITE_PHASE:
        case NGX_HTTP_CONTENT_PHASE:
            ngx_log_debug6(NGX_LOG_DEBUG_WASM, pwctx->log, 0,
                           "proxy_wasm pausing in \"%V\" phase"
                           " (step: %d, filter: %l/%l, action: %d, pwctx: %p)",
                           &pwctx->phase->name, pwctx->step,
                           pwctx->cur_filter_idx + 1, pwctx->n_filters,
                           pwctx->action, pwctx);
            goto yield;
#endif
        default:
            ngx_wasm_log_error(NGX_LOG_ERR, pwctx->log, 0,
                               "proxy_wasm cannot pause in \"%V\" phase",
                               &pwctx->phase->name);

            fctxs = (ngx_proxy_wasm_filter_ctx_t *) pwctx->fctxs.elts;
            yfctx = &fctxs[pwctx->cur_filter_idx];
            yfctx->ecode = NGX_PROXY_WASM_ERR_NOT_YIELDABLE;
        }

        break;

    default:
        ngx_proxy_wasm_log_error(NGX_LOG_WASM_NYI, pwctx->log, 0,
                                 "NYI - proxy_wasm action: %d", pwctx->action);
        goto error;

    }

error:

    ngx_wasm_assert(rc == NGX_ERROR
#ifdef NGX_WASM_HTTP
                    || rc >= NGX_HTTP_SPECIAL_RESPONSE
#endif
                    );

    if (rc == NGX_ERROR) {
        fctx->ecode = NGX_PROXY_WASM_ERR_NOT_YIELDABLE;
    }

    goto done;

yield:

    rc = NGX_AGAIN;
    goto done;

cont:

    rc = NGX_OK;

done:

    return rc;
}


static void
ngx_proxy_wasm_ctx_free(ngx_proxy_wasm_ctx_t *pwctx)
{
    size_t                          i;
    ngx_proxy_wasm_filter_ctx_t    *fctx, *fctxs;
    ngx_proxy_wasm_instance_ctx_t  *ictx;
    unsigned                        force_destroy;

    ngx_log_debug2(NGX_LOG_DEBUG_WASM, pwctx->log, 0,
                   "proxy_wasm freeing stream context #%d (main: %d)",
                   pwctx->id, pwctx->main);

    fctxs = (ngx_proxy_wasm_filter_ctx_t *) pwctx->fctxs.elts;

    for (i = 0; i < pwctx->fctxs.nelts; i++) {
        fctx = &fctxs[i];
        ictx = fctx->ictx;
        force_destroy = *fctx->filter->isolation == NGX_PROXY_WASM_ISOLATION_FILTER;

        ngx_proxy_wasm_instance_release(ictx, force_destroy);
    }

    ngx_proxy_wasm_store_destroy(&pwctx->store);

    if (pwctx->authority.data) {
        ngx_pfree(pwctx->pool, pwctx->authority.data);
    }

    if (pwctx->scheme.data) {
        ngx_pfree(pwctx->pool, pwctx->scheme.data);
    }

    if (pwctx->path.data) {
        ngx_pfree(pwctx->pool, pwctx->path.data);
    }

    ngx_array_destroy(&pwctx->fctxs);

    ngx_pfree(pwctx->pool, pwctx);
}


/* ngx_proxy_wasm_store_t */


ngx_proxy_wasm_instance_ctx_t *
ngx_proxy_wasm_instance_get(ngx_proxy_wasm_filter_t *filter,
    ngx_proxy_wasm_store_t *store, ngx_log_t *log)
{
    ngx_queue_t                    *q;
    ngx_pool_t                     *pool;
    ngx_wavm_module_t              *module = filter->module;
    ngx_proxy_wasm_instance_ctx_t  *ictx = NULL;

    if (store == NULL) {
        pool = filter->pool;
        goto create;
    }

    /* lookup */

    for (q = ngx_queue_head(&store->busy);
         q != ngx_queue_sentinel(&store->busy);
         q = ngx_queue_next(q))
    {
        ictx = ngx_queue_data(q, ngx_proxy_wasm_instance_ctx_t, q);

        if (ictx->filter->module == filter->module) {
            ngx_wasm_assert(ictx->nrefs);
            goto reuse;
        }
    }

    for (q = ngx_queue_head(&store->free);
         q != ngx_queue_sentinel(&store->free);
         q = ngx_queue_next(q))
    {
        ictx = ngx_queue_data(q, ngx_proxy_wasm_instance_ctx_t, q);

        if (ictx->filter->module == filter->module) {
            ngx_wasm_assert(ictx->nrefs == 0);

            ngx_queue_remove(&ictx->q);

            goto reuse;
        }
    }

    pool = store->pool;

create:

    ngx_log_debug4(NGX_LOG_DEBUG_WASM, log, 0,
                   "proxy_wasm \"%V\" filter (%l/%l) "
                   "new instance (ictx: %p)",
                   filter->name, filter->index + 1,
                   *filter->n_filters, ictx);

    ictx = ngx_pcalloc(pool, sizeof(ngx_proxy_wasm_instance_ctx_t));
    if (ictx == NULL) {
        goto error;
    }

    ictx->filter_id = filter->id;
    ictx->filter = filter;
    ictx->pool = pool;
    ictx->log = log;
    ictx->store = store;

    ngx_rbtree_init(&ictx->tree_root_ctxs, &ictx->sentinel_root_ctxs,
                    ngx_rbtree_insert_value);

    ictx->instance = ngx_wavm_instance_create(module, ictx->pool, log, ictx);
    if (ictx->instance == NULL) {
        ngx_pfree(ictx->pool, ictx);
        goto error;
    }

    goto done;

error:

    return NULL;

reuse:

    ngx_log_debug5(NGX_LOG_DEBUG_WASM, log, 0,
                   "proxy_wasm \"%V\" filter (%l/%l) "
                   "reusing instance (ictx: %p, nrefs: %d)",
                   filter->name, filter->index + 1,
                   *filter->n_filters, ictx, ictx->nrefs + 1);

done:

    if (store && !ictx->nrefs) {
        ngx_queue_insert_tail(&store->busy, &ictx->q);
    }

    ictx->nrefs++;

    return ictx;
}


void
ngx_proxy_wasm_instance_release(ngx_proxy_wasm_instance_ctx_t *ictx, unsigned force)
{
    ictx->nrefs--;

    dd("ictx->nrefs: %ld", ictx->nrefs);

    if (ictx->nrefs) {
        return;
    }

    /* release */

    if (ictx->store) {
        ngx_queue_remove(&ictx->q); /* remove from busy */

        ngx_queue_insert_tail(&ictx->store->free, &ictx->q);
    }

    if (ictx->instance->trapped || force) {
        ngx_proxy_wasm_instance_destroy(ictx);
    }
}


static ngx_inline void
ngx_proxy_wasm_instance_destroy(ngx_proxy_wasm_instance_ctx_t *ictx)
{
    ngx_rbtree_node_t            **root, **sentinel, *s, *n;
    ngx_proxy_wasm_filter_ctx_t   *fctx;

    if (ictx->store) {
        ngx_queue_remove(&ictx->q);
    }

    root = &ictx->tree_root_ctxs.root;
    s = &ictx->sentinel_root_ctxs;
    sentinel = &s;

    while (*root != *sentinel) {
        n = ngx_rbtree_min(*root, *sentinel);
        fctx = ngx_rbtree_data(n, ngx_proxy_wasm_filter_ctx_t, node);
        ngx_wasm_assert(fctx->root_id == NGX_PROXY_WASM_ROOT_CTX_ID);

        ngx_rbtree_delete(&ictx->tree_root_ctxs, n);

        if (fctx->ev) {
            ngx_del_timer(fctx->ev);
            ngx_free(fctx->ev);
        }

        ngx_pfree(ictx->pool, fctx);
    }

    ngx_wavm_instance_destroy(ictx->instance);

    ngx_pfree(ictx->pool, ictx);
}


void
ngx_proxy_wasm_store_destroy(ngx_proxy_wasm_store_t *store)
{
    ngx_queue_t                    *q;
    ngx_proxy_wasm_instance_ctx_t  *ictx;

    while (!ngx_queue_empty(&store->busy)) {
        q = ngx_queue_head(&store->busy);
        ictx = ngx_queue_data(q, ngx_proxy_wasm_instance_ctx_t, q);

        ngx_queue_remove(&ictx->q);
        ngx_queue_insert_tail(&store->free, &ictx->q);
    }

    while (!ngx_queue_empty(&store->free)) {
        q = ngx_queue_head(&store->free);
        ictx = ngx_queue_data(q, ngx_proxy_wasm_instance_ctx_t, q);

        ngx_proxy_wasm_instance_destroy(ictx);
    }
}


/* ngx_proxy_wasm_filter_t */


static ngx_inline ngx_wavm_funcref_t *
ngx_proxy_wasm_filter_func_lookup(ngx_proxy_wasm_filter_t *filter, const char *n)
{
    ngx_str_t   name;

    name.data = (u_char *) n;
    name.len = ngx_strlen(n);

    return ngx_wavm_module_func_lookup(filter->module, &name);
}


static ngx_proxy_wasm_abi_version_e
ngx_proxy_wasm_filter_abi_version(ngx_proxy_wasm_filter_t *filter)
{
    size_t                    i;
    ngx_wavm_module_t        *module = filter->module;
    const wasm_exporttype_t  *exporttype;
    const wasm_name_t        *exportname;

    for (i = 0; i < module->exports.size; i++) {
        exporttype = ((wasm_exporttype_t **) module->exports.data)[i];
        exportname = wasm_exporttype_name(exporttype);

        if (ngx_strncmp(exportname->data, "proxy_abi_version_0_2_1",
                        exportname->size) == 0)
        {
            return NGX_PROXY_WASM_0_2_1;
        }

        if (ngx_strncmp(exportname->data, "proxy_abi_version_0_2_0",
                        exportname->size) == 0)
        {
            return NGX_PROXY_WASM_0_2_0;
        }

        if (ngx_strncmp(exportname->data, "proxy_abi_version_0_1_0",
                        exportname->size) == 0)
        {
            return NGX_PROXY_WASM_0_1_0;
        }

#if (NGX_DEBUG)
        if (ngx_strncmp(exportname->data, "proxy_abi_version_vnext",
                        exportname->size) == 0)
        {
            return NGX_PROXY_WASM_VNEXT;
        }
#endif
    }

    return NGX_PROXY_WASM_UNKNOWN;
}


ngx_uint_t
ngx_proxy_wasm_filter_id(ngx_str_t *name, ngx_str_t *config, ngx_uint_t idx)
{
#define NGX_PROXY_WASM_FILTER_ID_SPRINTF  0
    uint32_t    hash;
#if (NGX_PROXY_WASM_FILTER_ID_SPRINTF)
    ngx_str_t   str;

    str.data = ngx_alloc(NGX_INT64_LEN);
    if (str.data == NULL) {
        return 0;
    }

    str.len = ngx_sprintf(str.data, "%d", filter->index) - str.data;
#endif

    ngx_crc32_init(hash);
    ngx_crc32_update(&hash, name->data, name->len);
    ngx_crc32_update(&hash, config->data, config->len);
#if (NGX_PROXY_WASM_FILTER_ID_SPRINTF)
    ngx_crc32_update(&hash, str.data, str.len);
    ngx_crc32_final(hash);
#else
    ngx_crc32_final(hash);

    hash += idx;
#endif

#if (NGX_PROXY_WASM_FILTER_ID_SPRINTF)
    ngx_free(str.data);
#endif

    return hash;
}


ngx_int_t
ngx_proxy_wasm_filter_init(ngx_proxy_wasm_filter_t *filter)
{
    ngx_int_t   rc;

    filter->name = &filter->module->name;
    filter->index = *filter->n_filters - 1;
    filter->id = ngx_proxy_wasm_filter_id(filter->name,
                                          &filter->config,
                                          filter->index);

    ngx_log_debug5(NGX_LOG_DEBUG_WASM, filter->log, 0,
                   "proxy_wasm initializing \"%V\" filter "
                   "(config size: %d, filter: %p, filter->id: %ui, "
                   "isolation: %ui)",
                   filter->name, filter->config.len, filter, filter->id,
                   *filter->isolation);

    if (filter->module == NULL) {
        filter->ecode = NGX_PROXY_WASM_ERR_BAD_HOST_INTERFACE;
        goto error;
    }

    /* check SDK */

    filter->abi_version = ngx_proxy_wasm_filter_abi_version(filter);

    switch (filter->abi_version) {
    case NGX_PROXY_WASM_0_1_0:
    case NGX_PROXY_WASM_0_2_0:
    case NGX_PROXY_WASM_0_2_1:
        break;
    case NGX_PROXY_WASM_UNKNOWN:
        filter->ecode = NGX_PROXY_WASM_ERR_UNKNOWN_ABI;
        goto error;
    default:
        filter->ecode = NGX_PROXY_WASM_ERR_BAD_ABI;
        goto error;
    }

    /* retrieve SDK */

    filter->proxy_on_memory_allocate =
        ngx_proxy_wasm_filter_func_lookup(filter, "malloc");

    if (filter->proxy_on_memory_allocate == NULL) {
        filter->proxy_on_memory_allocate =
            ngx_proxy_wasm_filter_func_lookup(
                filter, "proxy_on_memory_allocate");

        if (filter->proxy_on_memory_allocate == NULL) {
            filter->ecode = NGX_PROXY_WASM_ERR_BAD_MODULE_INTERFACE;

            ngx_proxy_wasm_log_error(NGX_LOG_EMERG, filter->log, filter->ecode,
                                     "proxy_wasm \"%V\" filter missing malloc",
                                     filter->name);
            goto error;
        }
    }

    /* context */

    filter->proxy_on_context_create =
        ngx_proxy_wasm_filter_func_lookup(filter, "proxy_on_context_create");
    filter->proxy_on_context_finalize =
        ngx_proxy_wasm_filter_func_lookup(filter, "proxy_on_context_finalize");

    if (filter->abi_version < NGX_PROXY_WASM_VNEXT) {
        /* 0.1.0 - 0.2.1 */
        filter->proxy_on_done =
            ngx_proxy_wasm_filter_func_lookup(filter, "proxy_on_done");
        filter->proxy_on_log =
            ngx_proxy_wasm_filter_func_lookup(filter, "proxy_on_log");
        filter->proxy_on_context_finalize =
            ngx_proxy_wasm_filter_func_lookup(filter, "proxy_on_delete");
    }

    /* configuration */

    filter->proxy_on_vm_start =
        ngx_proxy_wasm_filter_func_lookup(filter, "proxy_on_vm_start");
    filter->proxy_on_plugin_start =
        ngx_proxy_wasm_filter_func_lookup(filter, "proxy_on_plugin_start");

    if (filter->abi_version < NGX_PROXY_WASM_VNEXT) {
        /* 0.1.0 - 0.2.1 */
        filter->proxy_on_plugin_start =
            ngx_proxy_wasm_filter_func_lookup(filter, "proxy_on_configure");
    }

    /* stream */

    filter->proxy_on_new_connection =
        ngx_proxy_wasm_filter_func_lookup(filter, "proxy_on_new_connection");
    filter->proxy_on_downstream_data =
        ngx_proxy_wasm_filter_func_lookup(filter, "proxy_on_downstream_data");
    filter->proxy_on_upstream_data =
        ngx_proxy_wasm_filter_func_lookup(filter, "proxy_on_upstream_data");
    filter->proxy_on_downstream_close =
        ngx_proxy_wasm_filter_func_lookup(filter, "proxy_on_downstream_close");
    filter->proxy_on_upstream_close =
        ngx_proxy_wasm_filter_func_lookup(filter, "proxy_on_upstream_close");

    if (filter->abi_version < NGX_PROXY_WASM_VNEXT) {
        /* 0.1.0 - 0.2.1 */
        filter->proxy_on_downstream_close =
            ngx_proxy_wasm_filter_func_lookup(
                filter, "proxy_on_downstream_connection_close");
        filter->proxy_on_upstream_close =
            ngx_proxy_wasm_filter_func_lookup(
                filter, "proxy_on_upstream_connection_close");
    }

    /* http */

    filter->proxy_on_http_request_headers =
        ngx_proxy_wasm_filter_func_lookup(filter,
                                          "proxy_on_http_request_headers");
    filter->proxy_on_http_request_body =
        ngx_proxy_wasm_filter_func_lookup(filter,
                                          "proxy_on_http_request_body");
    filter->proxy_on_http_request_trailers =
        ngx_proxy_wasm_filter_func_lookup(filter,
                                          "proxy_on_http_request_trailers");
    filter->proxy_on_http_request_metadata =
        ngx_proxy_wasm_filter_func_lookup(filter,
                                          "proxy_on_http_request_metadata");
    filter->proxy_on_http_response_headers =
        ngx_proxy_wasm_filter_func_lookup(filter,
                                          "proxy_on_http_response_headers");
    filter->proxy_on_http_response_body =
        ngx_proxy_wasm_filter_func_lookup(filter,
                                          "proxy_on_http_response_body");
    filter->proxy_on_http_response_trailers =
        ngx_proxy_wasm_filter_func_lookup(filter,
                                          "proxy_on_http_response_trailers");
    filter->proxy_on_http_response_metadata =
        ngx_proxy_wasm_filter_func_lookup(filter,
                                          "proxy_on_http_response_metadata");

    if (filter->abi_version < NGX_PROXY_WASM_VNEXT) {
        /* 0.1.0 - 0.2.1 */
        filter->proxy_on_http_request_headers =
            ngx_proxy_wasm_filter_func_lookup(filter,
                                              "proxy_on_request_headers");
        filter->proxy_on_http_request_body =
            ngx_proxy_wasm_filter_func_lookup(filter,
                                              "proxy_on_request_body");
        filter->proxy_on_http_request_trailers =
            ngx_proxy_wasm_filter_func_lookup(filter,
                                              "proxy_on_request_trailers");
        filter->proxy_on_http_request_metadata =
            ngx_proxy_wasm_filter_func_lookup(filter,
                                              "proxy_on_request_metadata");
        filter->proxy_on_http_response_headers =
            ngx_proxy_wasm_filter_func_lookup(filter,
                                              "proxy_on_response_headers");
        filter->proxy_on_http_response_body =
            ngx_proxy_wasm_filter_func_lookup(filter,
                                              "proxy_on_response_body");
        filter->proxy_on_http_response_trailers =
            ngx_proxy_wasm_filter_func_lookup(filter,
                                              "proxy_on_response_trailers");
        filter->proxy_on_http_response_metadata =
            ngx_proxy_wasm_filter_func_lookup(filter,
                                              "proxy_on_response_metadata");
    }

    /* shared queue */

    filter->proxy_on_queue_ready =
        ngx_proxy_wasm_filter_func_lookup(filter, "proxy_on_queue_ready");

    /* timers */

    filter->proxy_create_timer =
        ngx_proxy_wasm_filter_func_lookup(filter, "proxy_create_timer");
    filter->proxy_delete_timer =
        ngx_proxy_wasm_filter_func_lookup(filter, "proxy_delete_timer");
    filter->proxy_on_timer_ready =
        ngx_proxy_wasm_filter_func_lookup(filter, "proxy_on_timer_ready");

    if (filter->abi_version < NGX_PROXY_WASM_VNEXT) {
        /* 0.1.0 - 0.2.1 */
        filter->proxy_on_timer_ready =
            ngx_proxy_wasm_filter_func_lookup(filter, "proxy_on_tick");
    }

    /* http callouts */

    filter->proxy_on_http_call_response =
        ngx_proxy_wasm_filter_func_lookup(
            filter, "proxy_on_http_call_response");

    /* grpc callouts */

    filter->proxy_on_grpc_call_response_header_metadata =
        ngx_proxy_wasm_filter_func_lookup(
            filter, "proxy_on_grpc_call_response_header_metadata");
    filter->proxy_on_grpc_call_response_message =
        ngx_proxy_wasm_filter_func_lookup(
            filter, "proxy_on_grpc_call_response_message");
    filter->proxy_on_grpc_call_response_trailer_metadata =
        ngx_proxy_wasm_filter_func_lookup(
            filter, "proxy_on_grpc_call_response_trailer_metadata");
    filter->proxy_on_grpc_call_close =
        ngx_proxy_wasm_filter_func_lookup(filter,
                                          "proxy_on_grpc_call_close");

    /* custom extensions */

    filter->proxy_on_custom_callback =
        ngx_proxy_wasm_filter_func_lookup(filter,
                                          "proxy_on_custom_callback");

    if (filter->abi_version < NGX_PROXY_WASM_VNEXT) {
        /* 0.2.0 - 0.2.1 */
        filter->proxy_on_custom_callback =
            ngx_proxy_wasm_filter_func_lookup(
                filter, "proxy_on_foreign_function");
    }

    /* validate */

    if (filter->proxy_on_context_create == NULL
        || filter->proxy_on_vm_start == NULL
        || filter->proxy_on_plugin_start == NULL)
    {
        filter->ecode = NGX_PROXY_WASM_ERR_BAD_MODULE_INTERFACE;

        ngx_proxy_wasm_log_error(NGX_LOG_EMERG, filter->log, filter->ecode,
                                 "proxy_wasm \"%V\" filter missing one of: "
                                 "on_context_create, on_vm_start, "
                                 "on_plugin_start", filter->name);
        goto error;
    }

    /* root ctx */

    filter->root_ictx = ngx_proxy_wasm_instance_get(filter, filter->store, filter->log);
    if (filter->root_ictx == NULL) {
        goto error;
    }

    rc = ngx_proxy_wasm_resume(filter->root_ictx, filter, NULL,
                               NGX_PROXY_WASM_STEP_INIT_CTX, NULL);
    if (rc != NGX_OK) {
        goto error;
    }

    if (filter->ecode) {
        goto error;
    }

    rc = NGX_OK;

    goto done;

error:

    ngx_proxy_wasm_log_error(NGX_LOG_EMERG, filter->log, filter->ecode,
                             "proxy_wasm failed initializing \"%V\" filter",
                             filter->name);

    rc = NGX_ERROR;

done:

    return rc;
}


void
ngx_proxy_wasm_filter_destroy(ngx_proxy_wasm_filter_t *filter)
{
    ngx_log_debug1(NGX_LOG_DEBUG_WASM, filter->log, 0,
                   "proxy_wasm freeing \"%V\" filter", filter->name);

    if (filter->root_ictx) {
        ngx_proxy_wasm_instance_release(filter->root_ictx, 1); /* force destroy */
    }

    /* filter->store points to mcf->store */
}


/* ngx_proxy_wasm_filter_ctx_t */


static ngx_proxy_wasm_filter_ctx_t *
ngx_proxy_wasm_root_ctx_lookup(ngx_proxy_wasm_instance_ctx_t *ictx, ngx_uint_t id)
{
    ngx_rbtree_t                 *rbtree;
    ngx_rbtree_node_t            *node, *sentinel;
    ngx_proxy_wasm_filter_ctx_t  *fctx;

    rbtree = &ictx->tree_root_ctxs;
    node = rbtree->root;
    sentinel = rbtree->sentinel;

    while (node != sentinel) {

        if (id != node->key) {
            node = (id < node->key) ? node->left : node->right;
            continue;
        }

        fctx = ngx_rbtree_data(node, ngx_proxy_wasm_filter_ctx_t, node);

        return fctx;
    }

    return NULL;
}


static ngx_int_t
ngx_proxy_wasm_start(ngx_proxy_wasm_filter_ctx_t *fctx)
{
    ngx_int_t                       rc;
    ngx_proxy_wasm_instance_ctx_t  *ictx = fctx->ictx;
    ngx_proxy_wasm_filter_t        *filter = fctx->filter;
    ngx_wavm_instance_t            *instance = ictx->instance;
    wasm_val_vec_t                 *rets;

    if (!fctx->created) {
        dd("create filter ctx (root_id: %ld, id: %ld, fctx: %p, ictx: %p)",
           fctx->root_id, fctx->id, fctx, ictx);

        rc = ngx_wavm_instance_call_funcref(instance,
                                            filter->proxy_on_context_create,
                                            NULL,
                                            fctx->id, fctx->root_id);
        if (rc != NGX_OK) {
            fctx->ecode = NGX_PROXY_WASM_ERR_INSTANCE_FAILED;
            return NGX_ERROR;
        }

        fctx->created = 1;
    }

    if (fctx->root_id == NGX_PROXY_WASM_ROOT_CTX_ID && !fctx->started) {
        dd("start filter ctx (root_id: %ld, id: %ld, fctx: %p, ictx: %p)",
           fctx->root_id, fctx->id, fctx, ictx);

        rc = ngx_wavm_instance_call_funcref(instance,
                                            filter->proxy_on_plugin_start,
                                            &rets,
                                            fctx->id, filter->config.len);
        if (rc != NGX_OK || !rets->data[0].of.i32) {
            fctx->ecode = NGX_PROXY_WASM_ERR_START_FAILED;
            return NGX_ERROR;
        }

        fctx->started = 1;

        if (fctx->root) {
            rc = ngx_wavm_instance_call_funcref(instance,
                                                filter->proxy_on_vm_start,
                                                &rets,
                                                fctx->id, 0);
            if (rc != NGX_OK || !rets->data[0].of.i32) {
                fctx->ecode = NGX_PROXY_WASM_ERR_START_FAILED;
                return NGX_ERROR;
            }
        }
    }

    return NGX_OK;
}


ngx_int_t
ngx_proxy_wasm_resume(ngx_proxy_wasm_instance_ctx_t *ictx,
    ngx_proxy_wasm_filter_t *filter, ngx_proxy_wasm_filter_ctx_t *fctx,
    ngx_proxy_wasm_step_e step, ngx_uint_t *ret)
{
    ngx_int_t                     rc;
    ngx_wavm_instance_t          *instance = ictx->instance;
    ngx_proxy_wasm_filter_ctx_t  *root_fctx;

    root_fctx = ngx_proxy_wasm_root_ctx_lookup(ictx, filter->id);
    if (root_fctx == NULL) {
        root_fctx = ngx_pcalloc(ictx->pool, sizeof(ngx_proxy_wasm_filter_ctx_t));
        if (root_fctx == NULL) {
            return NGX_ERROR;
        }

        root_fctx->id = filter->id;
        root_fctx->root_id = NGX_PROXY_WASM_ROOT_CTX_ID;
        root_fctx->pool = filter->pool;
        root_fctx->log = filter->log;
        root_fctx->ictx = ictx;
        root_fctx->parent = NULL;
        root_fctx->filter = filter;
        root_fctx->node.key = root_fctx->id;
        root_fctx->root = fctx == NULL;

        ngx_rbtree_insert(&ictx->tree_root_ctxs, &root_fctx->node);
    }

    ictx->current_ctx = root_fctx;

    ngx_wavm_instance_set_data(instance, ictx,
                               fctx && fctx->parent ? fctx->parent->log : filter->log);

    /* start root fctx */

    rc = ngx_proxy_wasm_start(root_fctx);
    if (rc != NGX_OK) {
        ngx_wasm_assert(root_fctx->ecode);
        filter->ecode = root_fctx->ecode;
        return NGX_ERROR;
    }

    /* start fctx */

    if (fctx == NULL) {
        ngx_wasm_assert(step == NGX_PROXY_WASM_STEP_INIT_CTX);
        return NGX_OK;
    }

    fctx->ictx = ictx;

    ictx->current_ctx = fctx;

    rc = ngx_proxy_wasm_start(fctx);
    if (rc != NGX_OK) {
        return NGX_ERROR;
    }

    switch (step) {
    case NGX_PROXY_WASM_STEP_INIT_CTX:
        /* ngx_proxy_wasm_start */
        break;
    case NGX_PROXY_WASM_STEP_REQ_HEADERS:
    case NGX_PROXY_WASM_STEP_REQ_BODY_READ:
    case NGX_PROXY_WASM_STEP_REQ_BODY:
    case NGX_PROXY_WASM_STEP_RESP_HEADERS:
    case NGX_PROXY_WASM_STEP_RESP_BODY:
        ngx_wasm_assert(ret);
        rc = filter->resume_(fctx, step, ret);
        break;
    case NGX_PROXY_WASM_STEP_LOG:
        ngx_proxy_wasm_on_log(fctx);
        rc = NGX_OK;
        break;
    case NGX_PROXY_WASM_STEP_DONE:
        ngx_proxy_wasm_on_done(fctx);
        rc = NGX_OK;
        break;
    case NGX_PROXY_WASM_STEP_ON_TIMER:
        rc = ngx_proxy_wasm_on_timer_tick(fctx);
        break;
    default:
        ngx_proxy_wasm_log_error(NGX_LOG_WASM_NYI, fctx->log, 0,
                                 "NYI - proxy_wasm step: %d", step);
        return NGX_ERROR;
    }

    if (rc == NGX_ABORT || fctx->ecode) {
        rc = NGX_ERROR;
    }

    return rc;
}


static void
ngx_proxy_wasm_on_log(ngx_proxy_wasm_filter_ctx_t *fctx)
{
    ngx_proxy_wasm_filter_t  *filter = fctx->filter;
    ngx_wavm_instance_t      *instance = ngx_proxy_wasm_fctx2instance(fctx);

    if (filter->abi_version < NGX_PROXY_WASM_VNEXT) {
        /* 0.1.0 - 0.2.1 */
        (void) ngx_wavm_instance_call_funcref(instance, filter->proxy_on_done,
                                              NULL, fctx->id);
    }

    (void) ngx_wavm_instance_call_funcref(instance, filter->proxy_on_log,
                                          NULL, fctx->id);
}


static void
ngx_proxy_wasm_on_done(ngx_proxy_wasm_filter_ctx_t *fctx)
{
    ngx_wavm_instance_t             *instance;
#if (NGX_WASM_HTTP && 0)
    ngx_http_proxy_wasm_dispatch_t  *call;
#endif

    instance = ngx_proxy_wasm_fctx2instance(fctx);

    if (fctx->created) {
        ngx_log_debug3(NGX_LOG_DEBUG_WASM, fctx->log, 0,
                       "proxy_wasm \"%V\" filter (%l/%l) "
                       "finalizing context",
                       fctx->filter->name, fctx->filter->index + 1,
                       fctx->parent->n_filters);

#if (NGX_WASM_HTTP && 0)
        call = fctx->call;
        if (call) {
            ngx_log_debug3(NGX_LOG_DEBUG_WASM, fctx->log, 0,
                           "proxy_wasm \"%V\" filter (%l/%l) "
                           "cancelling HTTP dispatch",
                           fctx->filter->name, fctx->filter->index + 1,
                           fctx->parent->n_filters);

            ngx_http_proxy_wasm_dispatch_destroy(call);

            fctx->call = NULL;
        }
#endif

        (void) ngx_wavm_instance_call_funcref(instance,
                                              fctx->filter->proxy_on_context_finalize,
                                              NULL, fctx->id);
    }
}


static ngx_int_t
ngx_proxy_wasm_on_timer_tick(ngx_proxy_wasm_filter_ctx_t *fctx)
{
    ngx_int_t       rc;
    wasm_val_vec_t  args;

    ngx_wasm_assert(fctx->root_id == NGX_PROXY_WASM_ROOT_CTX_ID);

    wasm_val_vec_new_uninitialized(&args, 1);
    ngx_wasm_vec_set_i32(&args, 0, fctx->id);

    rc = ngx_wavm_instance_call_funcref_vec(fctx->ictx->instance,
                                            fctx->filter->proxy_on_timer_ready,
                                            NULL, &args);

    wasm_val_vec_delete(&args);

    return rc;
}


ngx_wavm_ptr_t
ngx_proxy_wasm_alloc(ngx_proxy_wasm_filter_ctx_t *fctx, size_t size)
{
    ngx_wavm_ptr_t        p;
    ngx_int_t             rc;
    wasm_val_vec_t       *rets;
    ngx_wavm_instance_t  *instance = ngx_proxy_wasm_fctx2instance(fctx);

    rc = ngx_wavm_instance_call_funcref(instance,
                                        fctx->filter->proxy_on_memory_allocate,
                                        &rets, size);
    if (rc != NGX_OK) {
        ngx_wasm_log_error(NGX_LOG_CRIT, fctx->log, 0,
                           "proxy_wasm_alloc(%uz) failed", size);
        return 0;
    }

    p = rets->data[0].of.i32;

    ngx_log_debug3(NGX_LOG_DEBUG_WASM, fctx->log, 0,
                   "proxy_wasm_alloc: %uz:%uz:%uz",
                   ngx_wavm_memory_data_size(instance->memory), p, size);

    return p;
}


unsigned
ngx_proxy_wasm_marshal(ngx_proxy_wasm_filter_ctx_t *fctx, ngx_list_t *list,
    ngx_array_t *shims, ngx_wavm_ptr_t *out, uint32_t *out_size,
    ngx_uint_t *truncated)
{
    size_t                size;
    ngx_wavm_ptr_t        p;
    ngx_wavm_instance_t  *instance = ngx_proxy_wasm_fctx2instance(fctx);

    size = ngx_proxy_wasm_pairs_size(list, shims, fctx->filter->max_pairs);
    p = ngx_proxy_wasm_alloc(fctx, size);
    if (!p) {
        return 0;
    }

    if (p + size > ngx_wavm_memory_data_size(instance->memory)) {
        return 0;
    }

    ngx_proxy_wasm_pairs_marshal(list, shims,
                                 NGX_WAVM_HOST_LIFT_SLICE(instance, p, size),
                                 fctx->filter->max_pairs,
                                 truncated);

    *out = p;
    *out_size = (uint32_t) size;

    return 1;
}
