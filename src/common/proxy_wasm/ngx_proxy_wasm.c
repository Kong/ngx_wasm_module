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


/* ngx_proxy_wasm_filter_t */


static ngx_int_t
ngx_proxy_wasm_filter_init_root_fctx(ngx_proxy_wasm_filter_t *filter)
{
    ngx_int_t                     rc;
    ngx_proxy_wasm_filter_ctx_t  *root_fctx = &filter->root_fctx;
    wasm_val_vec_t               *rets;

    ngx_wasm_assert(root_fctx->id == NGX_PROXY_WASM_ROOT_CTX_ID);

    root_fctx->instance = ngx_wavm_instance_create(filter->module,
                                                   filter->pool,
                                                   filter->log,
                                                   &filter->root_fctx);
    if (root_fctx->instance == NULL) {
        root_fctx->ecode = NGX_PROXY_WASM_ERR_INSTANCE_FAILED;
        return NGX_ERROR;
    }

    rc = ngx_wavm_instance_call_funcref(root_fctx->instance,
                                        filter->proxy_on_context_create,
                                        NULL,
                                        filter->id, NGX_PROXY_WASM_ROOT_CTX_ID);
    if (rc != NGX_OK) {
        return NGX_ERROR;
    }

    root_fctx->context_created = 1;

    rc = ngx_wavm_instance_call_funcref(root_fctx->instance,
                                        filter->proxy_on_vm_start, &rets,
                                        filter->id, 0);
    if (rc != NGX_OK || !rets->data[0].of.i32) {
        return NGX_ERROR;
    }

    rc = ngx_wavm_instance_call_funcref(root_fctx->instance,
                                        filter->proxy_on_plugin_start, &rets,
                                        filter->id, filter->config.len);
    if (rc != NGX_OK || !rets->data[0].of.i32) {
        return NGX_ERROR;
    }

    root_fctx->ecode = 0;

    return NGX_OK;
}


ngx_int_t
ngx_proxy_wasm_filter_init(ngx_proxy_wasm_filter_t *filter)
{
    ngx_int_t   rc;

    filter->name = &filter->module->name;
    filter->index = *filter->max_filters;
    //filter->isolation = NGX_PROXY_WASM_ISOLATION_STREAM;
    filter->isolation = NGX_PROXY_WASM_ISOLATION_NONE;
    filter->id = ngx_crc32_long(filter->name->data, filter->name->len);

    ngx_log_debug4(NGX_LOG_DEBUG_WASM, filter->log, 0,
                   "proxy_wasm initializing \"%V\" filter "
                   "(config size: %d, filter: %p, filter->id: %ui)",
                   filter->name, filter->config.len, filter, filter->id);

    /* setup root ctx */

    ngx_memzero(&filter->root_fctx, sizeof(ngx_proxy_wasm_filter_ctx_t));

    filter->root_fctx.id = NGX_PROXY_WASM_ROOT_CTX_ID;
    filter->root_fctx.pool = filter->pool;
    filter->root_fctx.log = filter->log;
    filter->root_fctx.filter = filter;

    if (filter->module == NULL) {
        filter->root_fctx.ecode = NGX_PROXY_WASM_ERR_BAD_HOST_INTERFACE;
        goto error;
    }

    /* check SDK */

    filter->abi_version = ngx_proxy_wasm_abi_version(filter);

    switch (filter->abi_version) {

    case NGX_PROXY_WASM_0_1_0:
    case NGX_PROXY_WASM_0_2_0:
    case NGX_PROXY_WASM_0_2_1:
        break;

    case NGX_PROXY_WASM_UNKNOWN:
        filter->root_fctx.ecode = NGX_PROXY_WASM_ERR_UNKNOWN_ABI;
        goto error;

    default:
        filter->root_fctx.ecode = NGX_PROXY_WASM_ERR_BAD_ABI;
        goto error;

    }

    /* retrieve SDK */

    filter->proxy_on_memory_allocate =
        ngx_proxy_wasm_func_lookup(filter, "malloc");

    if (filter->proxy_on_memory_allocate == NULL) {
        filter->proxy_on_memory_allocate =
            ngx_proxy_wasm_func_lookup(filter, "proxy_on_memory_allocate");
        if (filter->proxy_on_memory_allocate == NULL) {
            filter->root_fctx.ecode = NGX_PROXY_WASM_ERR_BAD_MODULE_INTERFACE;
            ngx_proxy_wasm_log_error(NGX_LOG_EMERG, filter->log,
                                     filter->root_fctx.ecode, "missing malloc");
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
        filter->root_fctx.ecode = NGX_PROXY_WASM_ERR_BAD_MODULE_INTERFACE;
        ngx_proxy_wasm_log_error(NGX_LOG_EMERG, filter->log,
                                 filter->root_fctx.ecode,
                                 "missing one of: on_context_create, "
                                 "on_vm_start, on_plugin_start");
        goto error;
    }

    rc = ngx_proxy_wasm_filter_init_root_fctx(filter);
    if (rc != NGX_OK) {
        goto error;
    }

    goto done;

error:

    ngx_proxy_wasm_log_error(NGX_LOG_CRIT, filter->log,
                             filter->root_fctx.ecode,
                             "failed initializing \"%V\" filter",
                             filter->name);

    rc = NGX_ERROR;

done:

    return rc;
}


void
ngx_proxy_wasm_filter_destroy(ngx_proxy_wasm_filter_t *filter)
{
    ngx_proxy_wasm_filter_ctx_t  *root_fctx = &filter->root_fctx;

    ngx_log_debug1(NGX_LOG_DEBUG_WASM, filter->log, 0,
                   "proxy_wasm freeing \"%V\" filter", filter->name);

    if (!root_fctx->ecode && root_fctx->context_created) {
        ngx_log_debug1(NGX_LOG_DEBUG_WASM, root_fctx->log, 0,
                       "proxy_wasm finalizing \"%V\" root context",
                       filter->name);

        ngx_wasm_assert(root_fctx->id == NGX_PROXY_WASM_ROOT_CTX_ID);

        /* log previously freed by ngx_destroy_pool */
        ngx_wavm_instance_set_data(root_fctx->instance, root_fctx, filter->log);

        (void) ngx_wavm_instance_call_funcref(root_fctx->instance,
                                      filter->proxy_on_context_finalize,
                                      NULL, filter->id);

        root_fctx->context_created = 0;

        ngx_wavm_instance_destroy(root_fctx->instance);
    }
}


ngx_proxy_wasm_filter_ctx_t *
ngx_proxy_wasm_filter_get_ctx(ngx_proxy_wasm_filter_t *filter, void *data)
{
    ngx_int_t                     rc;
    ngx_proxy_wasm_stream_ctx_t  *sctx = NULL;
    ngx_proxy_wasm_filter_ctx_t  *fctx = NULL;
    wasm_val_vec_t               *rets;

    sctx = filter->get_context_(filter, data);
    if (sctx == NULL) {
        goto error;
    }

    fctx = &sctx->filter_ctxs[filter->index];

    if (fctx->pool == NULL) {
        fctx->id = sctx->id;
        fctx->pool = sctx->pool;
        fctx->log = sctx->log;
        fctx->filter = filter;
        fctx->context_created = 0;
        fctx->instance = NULL;
        fctx->ecode = NGX_PROXY_WASM_ERR_NONE;
    }

    switch (filter->isolation) {

    case NGX_PROXY_WASM_ISOLATION_NONE:
        ngx_log_debug3(NGX_LOG_DEBUG_WASM, sctx->log, 0,
                       "proxy_wasm reusing \"%V\" root instance in \"%V\" vm"
                       " data: %p",
                       filter->name, filter->module->vm->name,
                       data);

        fctx->instance = filter->root_fctx.instance;

        ngx_wavm_instance_set_data(fctx->instance, fctx, fctx->log);

        goto init;

    case NGX_PROXY_WASM_ISOLATION_STREAM:
        if (fctx->instance) {
            ngx_log_debug1(NGX_LOG_DEBUG_WASM, fctx->log, 0,
                           "proxy_wasm \"%V\" filter reusing instance",
                           filter->name);

            goto init;
        }

        break;

    default:
        ngx_wasm_assert(0);
        goto error;

    }

    fctx->instance = ngx_wavm_instance_create(filter->module,
                                        fctx->pool, fctx->log, fctx);
    if (fctx->instance == NULL) {
        fctx->ecode = NGX_PROXY_WASM_ERR_INSTANCE_FAILED;
        goto error;
    }

init:

    fctx->stream_ctx = sctx;
    fctx->data = data;

    if (fctx->context_created) {
        goto done;
    }

    /* start context */

    if (filter->isolation == NGX_PROXY_WASM_ISOLATION_STREAM) {
        rc = ngx_wavm_instance_call_funcref(fctx->instance,
                                            filter->proxy_on_context_create,
                                            NULL,
                                            filter->id,
                                            NGX_PROXY_WASM_ROOT_CTX_ID);
        if (rc != NGX_OK) {
            goto error;
        }

        rc = ngx_wavm_instance_call_funcref(fctx->instance,
                                            filter->proxy_on_plugin_start, &rets,
                                            filter->id, filter->config.len);
        if (rc != NGX_OK || !rets->data[0].of.i32) {
            goto error;
        }
    }

    rc = ngx_wavm_instance_call_funcref(fctx->instance,
                                        filter->proxy_on_context_create, NULL,
                                        fctx->id, filter->id);
    if (rc != NGX_OK) {
        goto error;
    }

    fctx->context_created = 1;

done:

    return fctx;

error:

    if (fctx) {
        ngx_pfree(fctx->pool, fctx);
    }

    return NULL;
}


ngx_int_t
ngx_proxy_wasm_filter_ctx_err_code(ngx_proxy_wasm_filter_ctx_t *fctx,
    ngx_wasm_phase_t *phase)
{
    ngx_int_t                     rc = NGX_OK;
    ngx_proxy_wasm_filter_t      *filter = fctx->filter;

    if (fctx->ecode == NGX_PROXY_WASM_ERR_NONE) {
        goto done;
    }

    if (!fctx->ecode_logged) {
        ngx_proxy_wasm_log_error(NGX_LOG_CRIT, fctx->log, fctx->ecode,
                                 "proxy_wasm failed resuming "
                                 "\"%V\" filter", &filter->module->name);
        fctx->ecode_logged = 1;
    }

    rc = filter->ecode_(fctx->ecode, phase);

done:

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
                  ngx_wavm_memory_data_size(fctx->instance->memory), p, size);

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

    if (p + size > ngx_wavm_memory_data_size(fctx->instance->memory)) {
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

#if 0
    ngx_log_debug3(NGX_LOG_DEBUG_WASM, fctx->log, 0,
                   "proxy_wasm \"%V\" in \"%V\" rc: %d",
                   filter->name, &phase->name, rc);
#endif

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
    ngx_int_t                     rc;
    ngx_proxy_wasm_filter_t      *filter = fctx->filter;
    ngx_proxy_wasm_stream_ctx_t  *sctx = fctx->stream_ctx;

    if (fctx->context_created && !fctx->ecode) {
        ngx_log_debug3(NGX_LOG_DEBUG_WASM, fctx->log, 0,
                       "proxy_wasm stream \"#%d\" finalizing filter %l/%l",
                       sctx->id, fctx->filter->index + 1, sctx->filter_max);

        (void) ngx_wavm_instance_call_funcref(fctx->instance,
                                      fctx->filter->proxy_on_context_finalize,
                                      NULL, fctx->id);
    }

    switch (filter->isolation) {
    case NGX_PROXY_WASM_ISOLATION_NONE:
        if (fctx->ecode == NGX_PROXY_WASM_ERR_INSTANCE_TRAPPED) {
           ngx_log_debug1(NGX_LOG_DEBUG_WASM, fctx->log, 0,
                          "proxy_wasm \"%V\" filter trapped, "
                          "recycling root instance", filter->name);

           ngx_wavm_instance_destroy(fctx->instance);

            rc = ngx_proxy_wasm_filter_init_root_fctx(filter);
            if (rc != NGX_OK) {
                ngx_proxy_wasm_log_error(NGX_LOG_CRIT, filter->log,
                                         filter->root_fctx.ecode,
                                         "failed recycling \"%V\" filter"
                                         " root instance",
                                         filter->name);
            }
        }

        break;

    case NGX_PROXY_WASM_ISOLATION_FULL:
        break;

    case NGX_PROXY_WASM_ISOLATION_STREAM:
        // TODO: cache reusable instances
        ngx_wavm_instance_destroy(fctx->instance);
        break;
    }

    fctx->instance = NULL;
    fctx->ecode = NGX_PROXY_WASM_ERR_NONE;
    fctx->ecode_logged = 0;
    fctx->context_created = 0;

    if (fctx->filter->index == sctx->filter_max - 1) {
        ngx_log_debug1(NGX_LOG_DEBUG_WASM, fctx->log, 0,
                       "proxy wasm destroying stream context id \"#%d\"",
                       sctx->id);

        if (sctx->authority.data) {
            ngx_pfree(sctx->pool, sctx->authority.data);
        }

        if (sctx->scheme.data) {
            ngx_pfree(sctx->pool, sctx->scheme.data);
        }

        if (sctx->filter_ctxs) {
            ngx_pfree(sctx->pool, sctx->filter_ctxs);
        }

        ngx_pfree(sctx->pool, sctx);
    }
}
