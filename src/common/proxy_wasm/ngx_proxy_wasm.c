#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_proxy_wasm.h>


static ngx_inline ngx_wavm_funcref_t *
ngx_proxy_wasm_func_lookup(ngx_proxy_wasm_filter_t *filter, const char *n)
{
    ngx_str_t   name;

    name.data = (u_char *) n;
    name.len = ngx_strlen(n);

    return ngx_wavm_module_func_lookup(filter->module, &name);
}


static ngx_proxy_wasm_abi_version_e
ngx_proxy_wasm_abi_version(ngx_proxy_wasm_filter_t *filter)
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
    }

    return NGX_PROXY_WASM_UNKNOWN;
}


/* ngx_proxy_wasm_filter_t */


ngx_int_t
ngx_proxy_wasm_filter_init_root_instance(ngx_proxy_wasm_filter_t *filter)
{
    filter->root_sctx.instance = NULL;
    filter->root_sctx.curr_filter_ctx->instance = NULL;

    return ngx_proxy_wasm_filter_init_instance(&filter->root_sctx);
}


ngx_int_t
ngx_proxy_wasm_filter_init_instance(ngx_proxy_wasm_stream_ctx_t *sctx)
{
    ngx_int_t                     rc;
    wasm_val_vec_t               *rets;
    ngx_wavm_instance_t          *instance = NULL;
    ngx_proxy_wasm_filter_ctx_t  *fctx = sctx->curr_filter_ctx;
    ngx_proxy_wasm_filter_t      *filter = fctx->filter;

    switch (filter->isolation) {

    case NGX_PROXY_WASM_ISOLATION_NONE:
        if (filter->root_sctx.curr_filter_ctx->instance) {
            ngx_log_debug2(NGX_LOG_DEBUG_WASM, sctx->log, 0,
                           "proxy_wasm reusing \"%V\" root instance in \"%V\" vm",
                           filter->name, filter->module->vm->name);

            sctx->instance = filter->root_sctx.curr_filter_ctx->instance;
            fctx->instance = sctx->instance;
            goto done;
        }

        break;

    /*
    case NGX_PROXY_WASM_ISOLATION_STREAM:
        if (sctx->instance) {
            ngx_log_debug2(NGX_LOG_DEBUG_WASM, sctx->log, 0,
                           "proxy_wasm reusing \"%V\" stream instance in \"%V\" vm",
                           filter->name, filter->module->vm->name);

            fctx->instance = sctx->instance;
            goto done;
        }

        break;

    case NGX_PROXY_WASM_ISOLATION_FULL:
        if (fctx->instance) {
            ngx_log_debug2(NGX_LOG_DEBUG_WASM, sctx->log, 0,
                           "proxy_wasm reusing \"%V\" request instance in \"%V\" vm",
                           filter->name, filter->module->vm->name);
            goto done;
        }

        break;
    */

    default:
        ngx_wasm_assert(0);
        goto error;

    }

    ngx_wasm_assert(sctx->instance == NULL);
    ngx_wasm_assert(fctx->instance == NULL);

//create:

    instance = ngx_wavm_instance_create(filter->lmodule, fctx->pool,
                                        fctx->log, NULL, sctx);
    if (instance == NULL) {
        sctx->ecode = NGX_PROXY_WASM_ERR_INSTANCE_FAILED;
        goto error;
    }

    sctx->instance = instance;
    fctx->instance = instance;

//start:

    /* start plugin */

    rc = ngx_wavm_instance_call_funcref(fctx->instance,
                                        filter->proxy_on_context_create, &rets,
                                        filter->id, NGX_PROXY_WASM_ROOT_CTX_ID);
    if (rc != NGX_OK) {
        goto error;
    }

    rc = ngx_wavm_instance_call_funcref(fctx->instance,
                                        filter->proxy_on_vm_start, &rets,
                                        filter->id, 0); // TODO: size
    if (rc != NGX_OK || !rets->data[0].of.i32) {
        goto error;
    }

    rc = ngx_wavm_instance_call_funcref(fctx->instance,
                                        filter->proxy_on_plugin_start, &rets,
                                        filter->id, filter->config.len);
    if (rc != NGX_OK || !rets->data[0].of.i32) {
        goto error;
    }

done:

    ngx_wavm_instance_set_data(fctx->instance, sctx);
    ngx_wavm_instance_set_log(fctx->instance, sctx->log);

    return NGX_OK;

error:

    if (instance) {
        ngx_wavm_instance_destroy(instance);
    }

    return NGX_ERROR;
}


ngx_int_t
ngx_proxy_wasm_filter_init(ngx_proxy_wasm_filter_t *filter)
{
    ngx_int_t                     rc;
    ngx_proxy_wasm_filter_ctx_t  *root_fctx;

    ngx_memzero(&filter->root_sctx, sizeof(ngx_proxy_wasm_stream_ctx_t));

    filter->name = &filter->module->name;
    filter->index = *filter->max_filters;
    filter->isolation = NGX_PROXY_WASM_ISOLATION_NONE;

    /* setup root ctx */

    filter->root_sctx.root = 1;
    filter->root_sctx.pool = filter->pool;
    filter->root_sctx.log = filter->log;
    filter->root_sctx.id = NGX_PROXY_WASM_ROOT_CTX_ID;

    root_fctx = ngx_pcalloc(filter->pool,
                            sizeof(ngx_proxy_wasm_filter_ctx_t));
    if (root_fctx == NULL) {
        goto error;
    }

    root_fctx->pool = filter->pool;
    root_fctx->log = filter->log;
    root_fctx->filter = filter;
    root_fctx->stream_ctx = &filter->root_sctx;

    filter->root_sctx.curr_filter_ctx = root_fctx;

    /*
     * TODO: when using the same instance for multiple filters, this will need
     * to be revisited
     */
    filter->id = ngx_crc32_long(filter->name->data, filter->name->len);

    ngx_log_debug3(NGX_LOG_DEBUG_WASM, filter->log, 0,
                   "proxy_wasm initializing \"%V\" filter "
                   "(config size: %d, filter: %p)",
                   filter->name, filter->config.len, filter);

    if (filter->lmodule == NULL) {
        filter->root_sctx.ecode = NGX_PROXY_WASM_ERR_BAD_HOST_INTERFACE;
        goto error;
    }

    /* check SDK */

    filter->abi_version = ngx_proxy_wasm_abi_version(filter);

    switch (filter->abi_version) {

    case NGX_PROXY_WASM_0_1_0:
    case NGX_PROXY_WASM_0_2_1:
        break;

    case NGX_PROXY_WASM_UNKNOWN:
        filter->root_sctx.ecode = NGX_PROXY_WASM_ERR_UNKNOWN_ABI;
        goto error;

    default:
        filter->root_sctx.ecode = NGX_PROXY_WASM_ERR_BAD_ABI;
        goto error;

    }

    /* retrieve SDK */

    filter->proxy_on_memory_allocate =
        ngx_proxy_wasm_func_lookup(filter, "malloc");

    if (filter->proxy_on_memory_allocate == NULL) {
        filter->proxy_on_memory_allocate =
            ngx_proxy_wasm_func_lookup(filter, "proxy_on_memory_allocate");
        if (filter->proxy_on_memory_allocate == NULL) {
            filter->root_sctx.ecode = NGX_PROXY_WASM_ERR_BAD_MODULE_INTERFACE;
            ngx_proxy_wasm_log_error(NGX_LOG_EMERG, filter->log,
                                     filter->root_sctx.ecode, "missing malloc");
            goto error;
        }
    }

    /* context */

    filter->proxy_on_context_create =
        ngx_proxy_wasm_func_lookup(filter, "proxy_on_context_create");
    filter->proxy_on_context_finalize =
        ngx_proxy_wasm_func_lookup(filter, "proxy_on_context_finalize");

    if (filter->abi_version < NGX_PROXY_WASM_VNEXT) {
        /* 0.1.0 - 0.2.1 */
        filter->proxy_on_done =
            ngx_proxy_wasm_func_lookup(filter, "proxy_on_done");
        filter->proxy_on_log =
            ngx_proxy_wasm_func_lookup(filter, "proxy_on_log");
        filter->proxy_on_context_finalize =
            ngx_proxy_wasm_func_lookup(filter, "proxy_on_delete");
    }

    /* configuration */

    filter->proxy_on_vm_start =
        ngx_proxy_wasm_func_lookup(filter, "proxy_on_vm_start");
    filter->proxy_on_plugin_start =
        ngx_proxy_wasm_func_lookup(filter, "proxy_on_plugin_start");

    if (filter->abi_version < NGX_PROXY_WASM_VNEXT) {
        /* 0.1.0 - 0.2.1 */
        filter->proxy_on_plugin_start =
            ngx_proxy_wasm_func_lookup(filter, "proxy_on_configure");
    }

    /* stream */

    filter->proxy_on_new_connection =
        ngx_proxy_wasm_func_lookup(filter, "proxy_on_new_connection");
    filter->proxy_on_downstream_data =
        ngx_proxy_wasm_func_lookup(filter, "proxy_on_downstream_data");
    filter->proxy_on_upstream_data =
        ngx_proxy_wasm_func_lookup(filter, "proxy_on_upstream_data");
    filter->proxy_on_downstream_close =
        ngx_proxy_wasm_func_lookup(filter, "proxy_on_downstream_close");
    filter->proxy_on_upstream_close =
        ngx_proxy_wasm_func_lookup(filter, "proxy_on_upstream_close");

    if (filter->abi_version < NGX_PROXY_WASM_VNEXT) {
        /* 0.1.0 - 0.2.1 */
        filter->proxy_on_downstream_close =
            ngx_proxy_wasm_func_lookup(filter,
                                       "proxy_on_downstream_connection_close");
        filter->proxy_on_upstream_close =
            ngx_proxy_wasm_func_lookup(filter,
                                       "proxy_on_upstream_connection_close");
    }

    /* http */

    filter->proxy_on_http_request_headers =
        ngx_proxy_wasm_func_lookup(filter, "proxy_on_http_request_headers");
    filter->proxy_on_http_request_body =
        ngx_proxy_wasm_func_lookup(filter, "proxy_on_http_request_body");
    filter->proxy_on_http_request_trailers =
        ngx_proxy_wasm_func_lookup(filter, "proxy_on_http_request_trailers");
    filter->proxy_on_http_request_metadata =
        ngx_proxy_wasm_func_lookup(filter, "proxy_on_http_request_metadata");
    filter->proxy_on_http_response_headers =
        ngx_proxy_wasm_func_lookup(filter, "proxy_on_http_response_headers");
    filter->proxy_on_http_response_body =
        ngx_proxy_wasm_func_lookup(filter, "proxy_on_http_response_body");
    filter->proxy_on_http_response_trailers =
        ngx_proxy_wasm_func_lookup(filter, "proxy_on_http_response_trailers");
    filter->proxy_on_http_response_metadata =
        ngx_proxy_wasm_func_lookup(filter, "proxy_on_http_response_metadata");

    if (filter->abi_version < NGX_PROXY_WASM_VNEXT) {
        /* 0.1.0 - 0.2.1 */
        filter->proxy_on_http_request_headers =
            ngx_proxy_wasm_func_lookup(filter, "proxy_on_request_headers");
        filter->proxy_on_http_request_body =
            ngx_proxy_wasm_func_lookup(filter, "proxy_on_request_body");
        filter->proxy_on_http_request_trailers =
            ngx_proxy_wasm_func_lookup(filter, "proxy_on_request_trailers");
        filter->proxy_on_http_request_metadata =
            ngx_proxy_wasm_func_lookup(filter, "proxy_on_request_metadata");
        filter->proxy_on_http_response_headers =
            ngx_proxy_wasm_func_lookup(filter, "proxy_on_response_headers");
        filter->proxy_on_http_response_body =
            ngx_proxy_wasm_func_lookup(filter, "proxy_on_response_body");
        filter->proxy_on_http_response_trailers =
            ngx_proxy_wasm_func_lookup(filter, "proxy_on_response_trailers");
        filter->proxy_on_http_response_metadata =
            ngx_proxy_wasm_func_lookup(filter, "proxy_on_response_metadata");
    }

    /* shared queue */

    filter->proxy_on_queue_ready =
        ngx_proxy_wasm_func_lookup(filter, "proxy_on_queue_ready");

    /* timers */

    filter->proxy_create_timer =
        ngx_proxy_wasm_func_lookup(filter, "proxy_create_timer");
    filter->proxy_delete_timer =
        ngx_proxy_wasm_func_lookup(filter, "proxy_delete_timer");
    filter->proxy_on_timer_ready =
        ngx_proxy_wasm_func_lookup(filter, "proxy_on_timer_ready");

    if (filter->abi_version < NGX_PROXY_WASM_VNEXT) {
        /* 0.1.0 - 0.2.1 */
        filter->proxy_on_timer_ready =
            ngx_proxy_wasm_func_lookup(filter, "proxy_on_tick");
    }

    /* http callouts */

    filter->proxy_on_http_call_response =
        ngx_proxy_wasm_func_lookup(filter, "proxy_on_http_call_response");

    /* grpc callouts */

    filter->proxy_on_grpc_call_response_header_metadata =
        ngx_proxy_wasm_func_lookup(filter,
                             "proxy_on_grpc_call_response_header_metadata");

    filter->proxy_on_grpc_call_response_message =
        ngx_proxy_wasm_func_lookup(filter,
                            "proxy_on_grpc_call_response_message");

    filter->proxy_on_grpc_call_response_trailer_metadata =
        ngx_proxy_wasm_func_lookup(filter,
                            "proxy_on_grpc_call_response_trailer_metadata");

    filter->proxy_on_grpc_call_close =
        ngx_proxy_wasm_func_lookup(filter, "proxy_on_grpc_call_close");

    /* custom extensions */

    filter->proxy_on_custom_callback =
        ngx_proxy_wasm_func_lookup(filter, "proxy_on_custom_callback");

    if (filter->abi_version < NGX_PROXY_WASM_VNEXT) {
        /* 0.2.0 - 0.2.1 */
        filter->proxy_on_custom_callback =
            ngx_proxy_wasm_func_lookup(filter, "proxy_on_foreign_function");
    }

    /* validate */

    if (filter->proxy_on_context_create == NULL
        || filter->proxy_on_vm_start == NULL
        || filter->proxy_on_plugin_start == NULL)
    {
        filter->root_sctx.ecode = NGX_PROXY_WASM_ERR_BAD_MODULE_INTERFACE;
        ngx_proxy_wasm_log_error(NGX_LOG_EMERG, filter->log,
                                 filter->root_sctx.ecode,
                                 "missing one of: on_context_create, "
                                 "on_vm_start, on_plugin_start");
        goto error;
    }

    rc = ngx_proxy_wasm_filter_init_root_instance(filter);
    if (rc != NGX_OK) {
        goto error;
    }

    goto done;

error:

    if (!filter->root_sctx.ecode) {
        filter->root_sctx.ecode = NGX_PROXY_WASM_ERR_UNKNOWN;
    }

    ngx_proxy_wasm_log_error(NGX_LOG_EMERG, filter->log,
                             filter->root_sctx.ecode,
                             "failed initializing \"%V\" filter",
                             filter->name);

    rc = NGX_ERROR;

done:

    return rc;
}


void
ngx_proxy_wasm_filter_destroy(ngx_proxy_wasm_filter_t *filter)
{
    ngx_proxy_wasm_stream_ctx_t  *root_sctx;
    ngx_proxy_wasm_filter_ctx_t  *root_fctx;

    root_sctx = &filter->root_sctx;
    root_fctx = root_sctx->curr_filter_ctx;

    if (root_fctx) {
        ngx_pfree(root_sctx->pool, root_fctx);
    }
}


ngx_proxy_wasm_filter_ctx_t *
ngx_proxy_wasm_filter_get_ctx(ngx_proxy_wasm_filter_t *filter, void *data)
{
    ngx_int_t                     rc;
    ngx_proxy_wasm_stream_ctx_t         *sctx;
    ngx_proxy_wasm_filter_ctx_t  *fctx = NULL;

    /*
     * data types:
     * - http subsystem: ngx_http_wasm_req_ctx_t
     */
    sctx = filter->get_context_(filter, data);
    if (sctx == NULL) {
        goto error;
    }

    fctx = sctx->filter_ctxs[filter->index];
    if (fctx == NULL) {
        fctx = ngx_pcalloc(sctx->pool, sizeof(ngx_proxy_wasm_filter_ctx_t));
        if (fctx == NULL) {
            goto error;
        }

        /* fctx->context_created = 0; */
        fctx->pool = sctx->pool;
        fctx->log = sctx->log;
        fctx->filter = filter;
        fctx->stream_ctx = sctx;

        sctx->filter_ctxs[filter->index] = fctx;
    }

    sctx->curr_filter_ctx = fctx;

    rc = ngx_proxy_wasm_filter_init_instance(sctx);
    if (rc != NGX_OK) {
        goto error;
    }

    if (!fctx->context_created) {
        ngx_log_debug1(NGX_LOG_DEBUG_WASM, sctx->log, 0,
                       "proxy_wasm creating stream context id \"#%d\"",
                       sctx->id);

        rc = ngx_wavm_instance_call_funcref(fctx->instance,
                                        fctx->filter->proxy_on_context_create,
                                        NULL, sctx->id, sctx->filter_id);
        if (rc != NGX_OK) {
            goto error;
        }

        fctx->context_created = 1;
    }

    return fctx;

error:

    if (fctx) {
        ngx_pfree(sctx->pool, fctx);
    }

    return NULL;
}


static void
ngx_proxy_wasm_filter_ctx_free(ngx_proxy_wasm_filter_ctx_t *fctx)
{
    ngx_proxy_wasm_stream_ctx_t  *sctx = fctx->stream_ctx;

    ngx_log_debug2(NGX_LOG_DEBUG_WASM, fctx->log, 0,
                   "proxy_wasm freeing stream \"#%d\""
                   " (fctx: %p)", fctx->stream_ctx->id, fctx);

    sctx->filter_ctxs[fctx->filter->index] = NULL;

    // TODO: free instance if full isolation

    ngx_pfree(sctx->pool, fctx);
}


ngx_int_t
ngx_proxy_wasm_filter_ctx_err_code(ngx_proxy_wasm_filter_ctx_t *fctx,
    ngx_uint_t ecode)
{
    ngx_int_t                     rc = NGX_OK;
    ngx_proxy_wasm_filter_t      *filter = fctx->filter;
    ngx_proxy_wasm_stream_ctx_t  *sctx = fctx->stream_ctx;

    if (ecode == NGX_PROXY_WASM_ERR_NONE) {
        goto done;
    }

    if (ecode == NGX_PROXY_WASM_ERR_INSTANCE_RECYCLED) {
        ngx_log_debug1(NGX_LOG_DEBUG_WASM, fctx->log, 0,
                       "proxy_wasm \"%V\" filter root instance was recycled",
                       filter->name);
        goto done;
    }

    if (ecode == NGX_PROXY_WASM_ERR_INSTANCE_TRAPPED
        && filter->isolation == NGX_PROXY_WASM_ISOLATION_NONE)
    {
        ngx_log_debug1(NGX_LOG_DEBUG_WASM, fctx->log, 0,
                       "proxy_wasm \"%V\" filter trapped, "
                       "recycling root instance", filter->name);

        ngx_wavm_instance_destroy(filter->root_sctx.curr_filter_ctx->instance);

        (void) ngx_proxy_wasm_filter_init_root_instance(filter);

        /* restore in filter ctx for calling proxy_on_context_finalize */
        sctx->instance = filter->root_sctx.curr_filter_ctx->instance;
        fctx->instance = sctx->instance;
        fctx->context_created = 0; /* re-invoke proxy_on_context_create */

        ngx_wavm_instance_set_data(fctx->instance, sctx);
        ngx_wavm_instance_set_log(fctx->instance, sctx->log);

        sctx->ecode = NGX_PROXY_WASM_ERR_INSTANCE_RECYCLED;

    } else if (!sctx->ecode_logged) {
        ngx_proxy_wasm_log_error(NGX_LOG_CRIT, fctx->log, ecode,
                                 "proxy_wasm failed resuming "
                                 "\"%V\" filter", &filter->module->name);
        sctx->ecode_logged = 1;
    }

    rc = filter->ecode_(ecode);

done:

#if (NGX_DEBUG)
    if (rc != NGX_OK) {
        ngx_log_debug3(NGX_LOG_DEBUG_WASM, fctx->log, 0,
                       "proxy_wasm stream \"#%d\" ecode: %l, rc: %d",
                       sctx->id, ecode, rc);
    }
#endif

    return rc;
}


/* ngx_proxy_wasm_stream_ctx_t */


ngx_wavm_ptr_t
ngx_proxy_wasm_alloc(ngx_proxy_wasm_filter_ctx_t *fctx, size_t size)
{
   ngx_wavm_ptr_t        p;
   ngx_int_t             rc;
   wasm_val_vec_t       *rets;

   rc = ngx_wavm_instance_call_funcref(fctx->instance,
                                       fctx->filter->proxy_on_memory_allocate,
                                       &rets, size);
   if (rc != NGX_OK) {
       ngx_wasm_log_error(NGX_LOG_CRIT, fctx->instance->log, 0,
                          "proxy_wasm_alloc(%uz) failed", size);
       return 0;
   }

   p = rets->data[0].of.i32;

   ngx_log_debug3(NGX_LOG_DEBUG_WASM, fctx->instance->log, 0,
                  "proxy_wasm_alloc: %uz:%uz:%uz",
                  wasm_memory_data_size(fctx->instance->memory), p, size);

   return p;
}


unsigned
ngx_proxy_wasm_marshal(ngx_proxy_wasm_filter_ctx_t *fctx, ngx_list_t *list,
    ngx_array_t *shims, ngx_wavm_ptr_t *out, size_t *out_size,
    ngx_uint_t *truncated)
{
    size_t           size;
    ngx_wavm_ptr_t   p;

    size = ngx_proxy_wasm_pairs_size(list, shims, fctx->filter->max_pairs);
    p = ngx_proxy_wasm_alloc(fctx, size);
    if (!p) {
        return 0;
    }

    if (p + size > wasm_memory_data_size(fctx->instance->memory)) {
        return 0;
    }

    ngx_proxy_wasm_pairs_marshal(list, shims,
                    ngx_wavm_memory_lift(fctx->instance->memory, p),
                    fctx->filter->max_pairs,
                    truncated);

    *out = p;
    *out_size = size;

    return 1;
}


/* phases */


ngx_int_t
ngx_proxy_wasm_resume(ngx_proxy_wasm_filter_ctx_t *fctx,
    ngx_wasm_phase_t *phase)
{
    ngx_int_t                 rc;
    ngx_proxy_wasm_filter_t  *filter = fctx->filter;

    ngx_log_debug2(NGX_LOG_DEBUG_WASM, fctx->log, 0,
                   "proxy_wasm resuming \"%V\" filter in \"%V\" phase",
                   filter->name, &phase->name);

    rc = filter->resume_(fctx, phase);

    ngx_log_debug3(NGX_LOG_DEBUG_WASM, fctx->log, 0,
                   "proxy_wasm \"%V\" in \"%V\" rc: %d",
                   filter->name, &phase->name, rc);

    return rc;
}


ngx_int_t
ngx_proxy_wasm_on_log(ngx_proxy_wasm_filter_ctx_t *fctx)
{
    ngx_proxy_wasm_filter_t  *filter = fctx->filter;

    if (filter->abi_version < NGX_PROXY_WASM_VNEXT) {
        /* 0.1.0 - 0.2.1 */
        if (ngx_wavm_instance_call_funcref(fctx->instance, filter->proxy_on_done,
                                           NULL, fctx->stream_ctx->id) != NGX_OK
            || ngx_wavm_instance_call_funcref(fctx->instance, filter->proxy_on_log,
                                              NULL, fctx->stream_ctx->id) != NGX_OK)
        {
            return NGX_ERROR;
        }
    }

    return NGX_OK;
}


void
ngx_proxy_wasm_on_done(ngx_proxy_wasm_filter_ctx_t *fctx)
{
    ngx_proxy_wasm_filter_t      *filter = fctx->filter;
    ngx_proxy_wasm_stream_ctx_t  *sctx = fctx->stream_ctx;

    ngx_log_debug3(NGX_LOG_DEBUG_WASM, fctx->log, 0,
                   "proxy_wasm stream \"#%d\" finalizing filter %l/%l",
                   sctx->id, fctx->filter->index + 1, sctx->n_filters);

    /* check if all filters are finished for this request */

    if (fctx->filter->index < sctx->n_filters - 1) {
        ngx_proxy_wasm_filter_ctx_free(fctx);
        return;
    }

    /* all filters are finished */

    if (!sctx->ecode) {
        (void) ngx_wavm_instance_call_funcref(fctx->instance,
                                      fctx->filter->proxy_on_context_finalize,
                                      NULL, sctx->id);
    }

    ngx_log_debug1(NGX_LOG_DEBUG_WASM, fctx->log, 0,
                   "proxy wasm destroying stream context id \"#%d\"",
                   sctx->id);

    ngx_proxy_wasm_filter_ctx_free(fctx);

    filter->free_context_(sctx);
}
