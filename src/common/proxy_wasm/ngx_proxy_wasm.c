#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_proxy_wasm.h>
#ifdef NGX_WASM_HTTP
#include <ngx_http_proxy_wasm.h>
#endif


#define ngx_proxy_wasm_n2fctx(n)                                             \
    (ngx_proxy_wasm_filter_ctx_t *)                                          \
     ((u_char *) (n) - offsetof(ngx_proxy_wasm_filter_ctx_t, node))


static ngx_proxy_wasm_instance_ctx_t *ngx_proxy_wasm_instance_get(
    ngx_proxy_wasm_filter_t *filter, ngx_proxy_wasm_store_t *store,
    ngx_log_t *log);
static void ngx_proxy_wasm_instance_release(
    ngx_proxy_wasm_instance_ctx_t *ictx);
static void ngx_proxy_wasm_instance_destroy(
    ngx_proxy_wasm_instance_ctx_t *ictx);
static ngx_proxy_wasm_filter_ctx_t *ngx_proxy_wasm_root_ctx_lookup(
    ngx_proxy_wasm_instance_ctx_t *ictx, ngx_uint_t id);
static ngx_inline ngx_wavm_funcref_t *ngx_proxy_wasm_filter_func_lookup(
    ngx_proxy_wasm_filter_t *filter, const char *n);
static ngx_proxy_wasm_abi_version_e ngx_proxy_wasm_filter_abi_version(
    ngx_proxy_wasm_filter_t *filter);
static ngx_int_t ngx_proxy_wasm_start(
    ngx_proxy_wasm_filter_ctx_t *fctx);
static ngx_proxy_wasm_filter_ctx_t *ngx_proxy_wasm_filter_ctx(
    ngx_proxy_wasm_filter_t *filter, ngx_proxy_wasm_stream_ctx_t *sctx);


/* ngx_proxy_wasm_store_t */


static ngx_proxy_wasm_instance_ctx_t *
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

    ictx = ngx_pcalloc(pool, sizeof(ngx_proxy_wasm_instance_ctx_t));
    if (ictx == NULL) {
        goto error;
    }

    ictx->filter_id = filter->id;
    ictx->filter = filter;
    ictx->pool = pool;
    ictx->log = log;
    ictx->store = store;

    ngx_rbtree_init(&ictx->tree_roots, &ictx->sentinel_roots,
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

    ngx_log_debug4(NGX_LOG_DEBUG_WASM, log, 0,
                   "proxy_wasm \"%V\" filter (%l/%l) "
                   "reusing instance (ictx: %p)",
                   filter->name, filter->index + 1,
                   *filter->n_filters, ictx);

done:

    if (store && !ictx->nrefs) {
        ngx_queue_insert_tail(&store->busy, &ictx->q);
    }

    ictx->nrefs++;

    return ictx;
}


static ngx_inline void
ngx_proxy_wasm_instance_release(ngx_proxy_wasm_instance_ctx_t *ictx)
{
    ictx->nrefs--;

    if (!ictx->nrefs) {
        if (ictx->store) {
            ngx_queue_remove(&ictx->q); /* remove from busy */

            ngx_queue_insert_tail(&ictx->store->free, &ictx->q);
        }

        if (ictx->instance->trapped) {
            ngx_proxy_wasm_instance_destroy(ictx);
        }
    }
}


static ngx_inline void
ngx_proxy_wasm_instance_destroy(ngx_proxy_wasm_instance_ctx_t *ictx)
{
    if (ictx->store) {
        ngx_queue_remove(&ictx->q);
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


static ngx_proxy_wasm_filter_ctx_t *
ngx_proxy_wasm_root_ctx_lookup(ngx_proxy_wasm_instance_ctx_t *ictx, ngx_uint_t id)
{
    ngx_rbtree_t                 *rbtree;
    ngx_rbtree_node_t            *node, *sentinel;
    ngx_proxy_wasm_filter_ctx_t  *fctx;

    rbtree = &ictx->tree_roots;
    node = rbtree->root;
    sentinel = rbtree->sentinel;

    while (node != sentinel) {

        if (id != node->key) {
            node = (id < node->key) ? node->left : node->right;
            continue;
        }

        fctx = ngx_proxy_wasm_n2fctx(node);

        return fctx;
    }

    return NULL;
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


ngx_int_t
ngx_proxy_wasm_filter_init(ngx_proxy_wasm_filter_t *filter)
{
    uint32_t                      hash;
    ngx_int_t                     rc;
    ngx_str_t                     s;
    ngx_proxy_wasm_filter_ctx_t  *fctx = NULL;

    filter->name = &filter->module->name;
    filter->index = *filter->n_filters;

    s.data = ngx_pnalloc(filter->pool, NGX_INT64_LEN);
    if (s.data == NULL) {
        goto error;
    }

    s.len = ngx_sprintf(s.data, "%d", filter->index) - s.data;

    ngx_crc32_init(hash);
    ngx_crc32_update(&hash, filter->name->data, filter->name->len);
    ngx_crc32_update(&hash, filter->config.data, filter->config.len);
    ngx_crc32_update(&hash, s.data, s.len);
    ngx_crc32_final(hash);

    filter->id = hash;

    ngx_pfree(filter->pool, s.data);

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
        ngx_proxy_wasm_filter_func_lookup(
            filter, "proxy_on_http_request_headers");
    filter->proxy_on_http_request_body =
        ngx_proxy_wasm_filter_func_lookup(
            filter, "proxy_on_http_request_body");
    filter->proxy_on_http_request_trailers =
        ngx_proxy_wasm_filter_func_lookup(
            filter, "proxy_on_http_request_trailers");
    filter->proxy_on_http_request_metadata =
        ngx_proxy_wasm_filter_func_lookup(
            filter, "proxy_on_http_request_metadata");
    filter->proxy_on_http_response_headers =
        ngx_proxy_wasm_filter_func_lookup(
            filter, "proxy_on_http_response_headers");
    filter->proxy_on_http_response_body =
        ngx_proxy_wasm_filter_func_lookup(
            filter, "proxy_on_http_response_body");
    filter->proxy_on_http_response_trailers =
        ngx_proxy_wasm_filter_func_lookup(
            filter, "proxy_on_http_response_trailers");
    filter->proxy_on_http_response_metadata =
        ngx_proxy_wasm_filter_func_lookup(
            filter, "proxy_on_http_response_metadata");

    if (filter->abi_version < NGX_PROXY_WASM_VNEXT) {
        /* 0.1.0 - 0.2.1 */
        filter->proxy_on_http_request_headers =
            ngx_proxy_wasm_filter_func_lookup(
                filter, "proxy_on_request_headers");
        filter->proxy_on_http_request_body =
            ngx_proxy_wasm_filter_func_lookup(
                filter, "proxy_on_request_body");
        filter->proxy_on_http_request_trailers =
            ngx_proxy_wasm_filter_func_lookup(
                filter, "proxy_on_request_trailers");
        filter->proxy_on_http_request_metadata =
            ngx_proxy_wasm_filter_func_lookup(
                filter, "proxy_on_request_metadata");
        filter->proxy_on_http_response_headers =
            ngx_proxy_wasm_filter_func_lookup(
                filter, "proxy_on_response_headers");
        filter->proxy_on_http_response_body =
            ngx_proxy_wasm_filter_func_lookup(
                filter, "proxy_on_response_body");
        filter->proxy_on_http_response_trailers =
            ngx_proxy_wasm_filter_func_lookup(
                filter, "proxy_on_response_trailers");
        filter->proxy_on_http_response_metadata =
            ngx_proxy_wasm_filter_func_lookup(
                filter, "proxy_on_response_metadata");
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
        ngx_proxy_wasm_filter_func_lookup(
            filter, "proxy_on_grpc_call_close");

    /* custom extensions */

    filter->proxy_on_custom_callback =
        ngx_proxy_wasm_filter_func_lookup(
            filter, "proxy_on_custom_callback");

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

    fctx = ngx_proxy_wasm_filter_ctx(filter, NULL);
    if (fctx == NULL || fctx->ecode) {
        goto error;
    }

    rc = NGX_OK;

    goto done;

error:

    ngx_proxy_wasm_log_error(NGX_LOG_CRIT, filter->log,
                             fctx ? fctx->ecode : filter->ecode,
                             "proxy_wasm failed initializing \"%V\" filter",
                             filter->name);

    rc = NGX_ERROR;

done:

    return rc;
}


ngx_int_t
ngx_proxy_wasm_filter_resume(ngx_proxy_wasm_filter_t *filter,
    ngx_wasm_phase_t *phase, ngx_log_t *log, void *data)
{
    size_t                          i;
    ngx_int_t                       rc;
    ngx_proxy_wasm_stream_ctx_t    *sctx;
    ngx_proxy_wasm_filter_ctx_t    *fctx;
    ngx_proxy_wasm_instance_ctx_t  *ictx;

    ngx_log_debug4(NGX_LOG_DEBUG_WASM, log, 0,
                   "proxy_wasm \"%V\" filter (%l/%l) "
                   "resuming in \"%V\" phase ",
                   filter->name, filter->index + 1,
                   *filter->n_filters, &phase->name);

    sctx = filter->get_context_(filter, data);
    if (sctx == NULL) {
        return NGX_ERROR;
    }

    fctx = ngx_proxy_wasm_filter_ctx(filter, sctx);
    if (fctx == NULL) {
        return NGX_ERROR;
    }

    ictx = fctx->ictx;
    ictx->current_ctx = fctx;

    if (ictx->instance->trapped) {
        fctx->ecode = NGX_PROXY_WASM_ERR_INSTANCE_TRAPPED;
    }

    ngx_log_debug7(NGX_LOG_DEBUG_WASM, log, 0,
                   "           \"%V\" filter (%l/%l) "
                   "fctx->id: %l, fctx->ecode: %d, fctx: %p, ictx: %p",
                   filter->name, filter->index + 1,
                   *filter->n_filters, fctx->id, fctx->ecode, fctx, ictx);

    if (fctx->ecode) {
        if (!fctx->ecode_logged && phase->index != NGX_WASM_DONE_PHASE) {
            ngx_proxy_wasm_log_error(NGX_LOG_CRIT, fctx->log, fctx->ecode,
                                     "proxy_wasm \"%V\" filter (%l/%l) "
                                     "failed resuming",
                                     filter->name, filter->index + 1,
                                     *filter->n_filters);
            fctx->ecode_logged = 1;
        }
#if (NGX_DEBUG)
        else {
            ngx_log_debug4(NGX_LOG_DEBUG_WASM, fctx->log, 0,
                           "proxy_wasm \"%V\" filter (%l/%l) "
                           "canceled in \"%V\" phase",
                           filter->name, filter->index + 1,
                           *filter->n_filters, &phase->name);
        }
#endif

        rc = filter->ecode_(fctx->ecode);

        goto skip;
    }

    rc = filter->resume_(fctx, phase);

    if (ictx->instance->trapped) {
        fctx->ecode = NGX_PROXY_WASM_ERR_INSTANCE_TRAPPED;
    }

skip:

    switch (phase->index) {

    case NGX_WASM_DONE_PHASE:

        ngx_proxy_wasm_instance_release(ictx);

        if (filter->index == sctx->n_filters - 1
            || rc != NGX_OK) {

            /* destroy stream ctx */

            ngx_log_debug2(NGX_LOG_DEBUG_WASM, sctx->log, 0,
                           "proxy_wasm freeing stream context #%d (main: %d)",
                           sctx->id, sctx->main);

            ngx_proxy_wasm_store_destroy(&sctx->store);

            if (sctx->authority.data) {
                ngx_pfree(sctx->pool, sctx->authority.data);
            }

            if (sctx->scheme.data) {
                ngx_pfree(sctx->pool, sctx->scheme.data);
            }

            if (sctx->path.data) {
                ngx_pfree(sctx->pool, sctx->path.data);
            }

            for (i = 0; i < sctx->fctxs.nelts; i++) {
                fctx = ((ngx_proxy_wasm_filter_ctx_t **) sctx->fctxs.elts)[i];
                ngx_pfree(sctx->pool, fctx);
            }

            ngx_array_destroy(&sctx->fctxs);

            ngx_pfree(sctx->pool, sctx);
        }

    default:
        break;

    }

    return rc;
}


static ngx_proxy_wasm_filter_ctx_t *
ngx_proxy_wasm_filter_ctx(ngx_proxy_wasm_filter_t *filter,
    ngx_proxy_wasm_stream_ctx_t *sctx)
{
    ngx_int_t                        rc;
    ngx_proxy_wasm_instance_ctx_t   *ictx;
    ngx_proxy_wasm_filter_ctx_t    **pfctx, *fctx = NULL;

    if (sctx == NULL) {
        ictx = ngx_proxy_wasm_instance_get(filter, NULL, filter->log);
        if (ictx == NULL) {
            goto error;
        }

        goto init;

    } else if (filter->index < sctx->fctxs.nelts) {

        /* filter ctx in stream ctx */

        fctx = ((ngx_proxy_wasm_filter_ctx_t **)
                sctx->fctxs.elts)[filter->index];

        ngx_wasm_assert(fctx->filter == filter);

        goto done;
    }

    /* get instance */

    switch (*filter->isolation) {

    case NGX_PROXY_WASM_ISOLATION_NONE:
        ictx = ngx_proxy_wasm_instance_get(filter, filter->store, sctx->log);
        if (ictx == NULL) {
            goto error;
        }

        break;

    case NGX_PROXY_WASM_ISOLATION_STREAM:
        ictx = ngx_proxy_wasm_instance_get(filter, &sctx->store, sctx->log);
        if (ictx == NULL) {
            goto error;
        }

        break;

    case NGX_PROXY_WASM_ISOLATION_FILTER:
        ictx = ngx_proxy_wasm_instance_get(filter, NULL, sctx->log);
        if (ictx == NULL) {
            goto error;
        }

        break;

    default:
        ngx_wasm_assert(0);
        goto error;

    }

init:

    /* create filter root ctx */

    fctx = ngx_proxy_wasm_root_ctx_lookup(ictx, filter->id);
    if (fctx == NULL) {
        fctx = ngx_pcalloc(ictx->pool, sizeof(ngx_proxy_wasm_filter_ctx_t));
        if (fctx == NULL) {
            goto error;
        }

        fctx->id = filter->id;
        fctx->root_id = NGX_PROXY_WASM_ROOT_CTX_ID;
        fctx->pool = filter->pool;
        fctx->log = filter->log;
        fctx->ictx = ictx;
        fctx->sctx = NULL;
        fctx->filter = filter;
        fctx->node.key = fctx->id;

        ngx_rbtree_insert(&ictx->tree_roots, &fctx->node);

        if (sctx == NULL) {
            fctx->root_instance = 1;
        }
    }

    ngx_wavm_instance_set_data(fctx->ictx->instance,
                               fctx->ictx,
                               sctx ? sctx->log : filter->log);

    /* start root ctx */

    rc = ngx_proxy_wasm_start(fctx);
    if (rc != NGX_OK) {
        goto error;
    }

    if (sctx == NULL) {
        /* get root ctx */
        goto done;
    }

    /* create filter ctx */

    fctx = ngx_pcalloc(sctx->pool, sizeof(ngx_proxy_wasm_filter_ctx_t));
    if (fctx == NULL) {
        goto error;
    }

    fctx->id = ++ictx->next_context_id;
    fctx->root_id = filter->id;
    fctx->pool = sctx->pool;
    fctx->log = sctx->log;
    fctx->ictx = ictx;
    fctx->sctx = sctx;
    fctx->filter = filter;
    fctx->node.key = fctx->id;

    pfctx = ngx_array_push(&sctx->fctxs);
    if (pfctx == NULL) {
        goto error;
    }

    *pfctx = fctx;

    ngx_wasm_assert(filter->index == sctx->fctxs.nelts - 1);

    /* start ctx */

    rc = ngx_proxy_wasm_start(fctx);
    if (rc != NGX_OK) {
        goto error;
    }

    goto done;

error:

    if (fctx && !fctx->ecode) {
        fctx->ecode = NGX_PROXY_WASM_ERR_UNKNOWN;
    }

done:

    if (fctx && fctx->ictx) {
        ngx_wavm_instance_set_data(fctx->ictx->instance,
                                   fctx->ictx,
                                   sctx ? sctx->log : filter->log);
    }

    return fctx;
}


void
ngx_proxy_wasm_filter_destroy(ngx_proxy_wasm_filter_t *filter)
{
    ngx_log_debug1(NGX_LOG_DEBUG_WASM, filter->log, 0,
                   "proxy_wasm freeing \"%V\" filter", filter->name);

    ngx_proxy_wasm_store_destroy(filter->store);
}


/* ngx_proxy_wasm_filter_ctx_t */


static ngx_int_t
ngx_proxy_wasm_start(ngx_proxy_wasm_filter_ctx_t *fctx)
{
    ngx_int_t                       rc;
    ngx_proxy_wasm_instance_ctx_t  *ictx = fctx->ictx;
    ngx_proxy_wasm_filter_t        *filter = fctx->filter;
    ngx_wavm_instance_t            *instance = ictx->instance;
    wasm_val_vec_t                 *rets;

    ictx->current_ctx = fctx;

    if (filter->ecode) {
        fctx->ecode = filter->ecode;
        goto done;
    }

    if (!fctx->created) {
        dd("create filter ctx (root_id: %ld, id: %ld, ictx: %p)",
           fctx->root_id, fctx->id, ictx);

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
        dd("start filter ctx (root_id: %ld, id: %ld, ictx: %p, "
           "root instance: %d)",
           fctx->root_id, fctx->id, ictx, fctx->root_instance);

        rc = ngx_wavm_instance_call_funcref(instance,
                                            filter->proxy_on_plugin_start,
                                            &rets,
                                            fctx->id, filter->config.len);
        if (rc != NGX_OK || !rets->data[0].of.i32) {
            fctx->ecode = NGX_PROXY_WASM_ERR_START_FAILED;
            return NGX_ERROR;
        }

        fctx->started = 1;

        if (fctx->root_instance) {
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

done:

    return NGX_OK;
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
    ngx_array_t *shims, ngx_wavm_ptr_t *out, size_t *out_size,
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
                   ngx_wavm_memory_lift(instance->memory, p),
                   fctx->filter->max_pairs,
                   truncated);

    *out = p;
    *out_size = size;

    return 1;
}


/* phases */


ngx_int_t
ngx_proxy_wasm_on_log(ngx_proxy_wasm_filter_ctx_t *fctx)
{
    ngx_proxy_wasm_filter_t  *filter = fctx->filter;
    ngx_wavm_instance_t      *instance = ngx_proxy_wasm_fctx2instance(fctx);

    if (filter->abi_version < NGX_PROXY_WASM_VNEXT) {
        /* 0.1.0 - 0.2.1 */
        if (ngx_wavm_instance_call_funcref(instance, filter->proxy_on_done,
                                           NULL, fctx->id) != NGX_OK
            || ngx_wavm_instance_call_funcref(instance, filter->proxy_on_log,
                                              NULL, fctx->id) != NGX_OK)
        {
            return NGX_ERROR;
        }
    }

    return NGX_OK;
}


void
ngx_proxy_wasm_on_done(ngx_proxy_wasm_filter_ctx_t *fctx)
{
    ngx_proxy_wasm_filter_t        *filter = fctx->filter;
    ngx_wavm_instance_t            *instance;

    if (fctx->created) {
        ngx_log_debug3(NGX_LOG_DEBUG_WASM, fctx->log, 0,
                       "proxy_wasm \"%V\" filter (%l/%l) "
                       "finalizing context",
                       filter->name, filter->index + 1,
                       *filter->n_filters);

        instance = ngx_proxy_wasm_fctx2instance(fctx);

        (void) ngx_wavm_instance_call_funcref(instance,
                                              filter->proxy_on_context_finalize,
                                              NULL, fctx->id);
    }
}
