#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_proxy_wasm.h>


static ngx_inline ngx_wavm_funcref_t *
ngx_proxy_wasm_func_lookup(ngx_proxy_wasm_t *pwm, const char *n)
{
    ngx_str_t   name;

    name.data = (u_char *) n;
    name.len = ngx_strlen(n);

    return ngx_wavm_module_func_lookup(pwm->module, &name);
}


static ngx_proxy_wasm_abi_version_e
ngx_proxy_wasm_abi_version(ngx_proxy_wasm_t *pwm)
{
    size_t                    i;
    ngx_wavm_module_t        *module = pwm->module;
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


/* ngx_proxy_wasm_t */


ngx_int_t
ngx_proxy_wasm_init(ngx_proxy_wasm_t *pwm)
{
    ngx_int_t                     rc;
    wasm_val_vec_t               *rets;
    ngx_proxy_wasm_filter_ctx_t  *fctx;

    ngx_wasm_assert(pwm->module);
    ngx_wasm_assert(pwm->max_filters);

    ngx_log_debug1(NGX_LOG_DEBUG_WASM, pwm->log, 0,
                   "initializing proxy_wasm filter (pwm: %p)", pwm);

    pwm->filter_idx = *pwm->max_filters;

    /* setup root prctx */

    ngx_memzero(&pwm->root_prctx, sizeof(ngx_proxy_wasm_ctx_t));

    pwm->root_prctx.pool = pwm->pool;
    pwm->root_prctx.log = pwm->log;
    pwm->root_prctx.id = NGX_PROXY_WASM_ROOT_CTX_ID;
    pwm->root_prctx.current_filter = 0;
    pwm->root_prctx.max_filters = 1;
    pwm->root_prctx.filters = ngx_pcalloc(pwm->pool,
                                  sizeof(ngx_proxy_wasm_filter_ctx_t *));
    if (pwm->root_prctx.filters == NULL) {
        return NGX_ERROR;
    }

    fctx = ngx_pcalloc(pwm->pool, sizeof(ngx_proxy_wasm_filter_ctx_t));
    if (fctx == NULL) {
        return NGX_ERROR;
    }

    fctx->filter = pwm;
    fctx->filter_ctx = &pwm->root_prctx;
    fctx->pool = pwm->pool;
    fctx->log = pwm->log;
    fctx->id = NGX_PROXY_WASM_ROOT_CTX_ID;
    fctx->filter_id = pwm->ctxid;

    /* linked module check */

    if (pwm->lmodule == NULL) {
        fctx->ecode = NGX_PROXY_WASM_ERR_BAD_HOST_INTERFACE;
        return NGX_ERROR;
    }

    /* check SDK */

    pwm->abi_version = ngx_proxy_wasm_abi_version(pwm);

    switch (pwm->abi_version) {

    case NGX_PROXY_WASM_0_1_0:
    case NGX_PROXY_WASM_0_2_1:
        break;

    case NGX_PROXY_WASM_UNKNOWN:
        fctx->ecode = NGX_PROXY_WASM_ERR_UNKNOWN_ABI;
        ngx_proxy_wasm_log_error(NGX_LOG_WASM_NYI, pwm->log, fctx->ecode, NULL);
        return NGX_ERROR;

    default:
        fctx->ecode = NGX_PROXY_WASM_ERR_BAD_ABI;
        ngx_proxy_wasm_log_error(NGX_LOG_EMERG, pwm->log, fctx->ecode, NULL);
        return NGX_ERROR;

    }

    /* retrieve SDK */

    pwm->proxy_on_memory_allocate =
        ngx_proxy_wasm_func_lookup(pwm, "malloc");

    if (pwm->proxy_on_memory_allocate == NULL) {
        pwm->proxy_on_memory_allocate =
            ngx_proxy_wasm_func_lookup(pwm, "proxy_on_memory_allocate");
        if (pwm->proxy_on_memory_allocate == NULL) {
            fctx->ecode = NGX_PROXY_WASM_ERR_BAD_MODULE_INTERFACE;
            ngx_proxy_wasm_log_error(NGX_LOG_EMERG, pwm->log, fctx->ecode,
                                     "missing malloc");
            return NGX_ERROR;
        }
    }

    /* context */

    pwm->proxy_on_context_create =
        ngx_proxy_wasm_func_lookup(pwm, "proxy_on_context_create");
    pwm->proxy_on_context_finalize =
        ngx_proxy_wasm_func_lookup(pwm, "proxy_on_context_finalize");

    if (pwm->abi_version < NGX_PROXY_WASM_VNEXT) {
        /* 0.1.0 - 0.2.1 */
        pwm->proxy_on_done =
            ngx_proxy_wasm_func_lookup(pwm, "proxy_on_done");
        pwm->proxy_on_log =
            ngx_proxy_wasm_func_lookup(pwm, "proxy_on_log");
        pwm->proxy_on_context_finalize =
            ngx_proxy_wasm_func_lookup(pwm, "proxy_on_delete");
    }

    /* configuration */

    pwm->proxy_on_vm_start =
        ngx_proxy_wasm_func_lookup(pwm, "proxy_on_vm_start");
    pwm->proxy_on_plugin_start =
        ngx_proxy_wasm_func_lookup(pwm, "proxy_on_plugin_start");

    if (pwm->abi_version < NGX_PROXY_WASM_VNEXT) {
        /* 0.1.0 - 0.2.1 */
        pwm->proxy_on_plugin_start =
            ngx_proxy_wasm_func_lookup(pwm, "proxy_on_configure");
    }

    /* stream */

    pwm->proxy_on_new_connection =
        ngx_proxy_wasm_func_lookup(pwm, "proxy_on_new_connection");
    pwm->proxy_on_downstream_data =
        ngx_proxy_wasm_func_lookup(pwm, "proxy_on_downstream_data");
    pwm->proxy_on_upstream_data =
        ngx_proxy_wasm_func_lookup(pwm, "proxy_on_upstream_data");
    pwm->proxy_on_downstream_close =
        ngx_proxy_wasm_func_lookup(pwm, "proxy_on_downstream_close");
    pwm->proxy_on_upstream_close =
        ngx_proxy_wasm_func_lookup(pwm, "proxy_on_upstream_close");

    if (pwm->abi_version < NGX_PROXY_WASM_VNEXT) {
        /* 0.1.0 - 0.2.1 */
        pwm->proxy_on_downstream_close =
            ngx_proxy_wasm_func_lookup(pwm, "proxy_on_downstream_connection_close");
        pwm->proxy_on_upstream_close =
            ngx_proxy_wasm_func_lookup(pwm, "proxy_on_upstream_connection_close");
    }

    /* http */

    pwm->proxy_on_http_request_headers =
        ngx_proxy_wasm_func_lookup(pwm, "proxy_on_http_request_headers");
    pwm->proxy_on_http_request_body =
        ngx_proxy_wasm_func_lookup(pwm, "proxy_on_http_request_body");
    pwm->proxy_on_http_request_trailers =
        ngx_proxy_wasm_func_lookup(pwm, "proxy_on_http_request_trailers");
    pwm->proxy_on_http_request_metadata =
        ngx_proxy_wasm_func_lookup(pwm, "proxy_on_http_request_metadata");
    pwm->proxy_on_http_response_headers =
        ngx_proxy_wasm_func_lookup(pwm, "proxy_on_http_response_headers");
    pwm->proxy_on_http_response_body =
        ngx_proxy_wasm_func_lookup(pwm, "proxy_on_http_response_body");
    pwm->proxy_on_http_response_trailers =
        ngx_proxy_wasm_func_lookup(pwm, "proxy_on_http_response_trailers");
    pwm->proxy_on_http_response_metadata =
        ngx_proxy_wasm_func_lookup(pwm, "proxy_on_http_response_metadata");

    if (pwm->abi_version < NGX_PROXY_WASM_VNEXT) {
        /* 0.1.0 - 0.2.1 */
        pwm->proxy_on_http_request_headers =
            ngx_proxy_wasm_func_lookup(pwm, "proxy_on_request_headers");
        pwm->proxy_on_http_request_body =
            ngx_proxy_wasm_func_lookup(pwm, "proxy_on_request_body");
        pwm->proxy_on_http_request_trailers =
            ngx_proxy_wasm_func_lookup(pwm, "proxy_on_request_trailers");
        pwm->proxy_on_http_request_metadata =
            ngx_proxy_wasm_func_lookup(pwm, "proxy_on_request_metadata");
        pwm->proxy_on_http_response_headers =
            ngx_proxy_wasm_func_lookup(pwm, "proxy_on_response_headers");
        pwm->proxy_on_http_response_body =
            ngx_proxy_wasm_func_lookup(pwm, "proxy_on_response_body");
        pwm->proxy_on_http_response_trailers =
            ngx_proxy_wasm_func_lookup(pwm, "proxy_on_response_trailers");
        pwm->proxy_on_http_response_metadata =
            ngx_proxy_wasm_func_lookup(pwm, "proxy_on_response_metadata");
    }

    /* shared queue */

    pwm->proxy_on_queue_ready =
        ngx_proxy_wasm_func_lookup(pwm, "proxy_on_queue_ready");

    /* timers */

    pwm->proxy_create_timer =
        ngx_proxy_wasm_func_lookup(pwm, "proxy_create_timer");
    pwm->proxy_delete_timer =
        ngx_proxy_wasm_func_lookup(pwm, "proxy_delete_timer");
    pwm->proxy_on_timer_ready =
        ngx_proxy_wasm_func_lookup(pwm, "proxy_on_timer_ready");

    if (pwm->abi_version < NGX_PROXY_WASM_VNEXT) {
        /* 0.1.0 - 0.2.1 */
        pwm->proxy_on_timer_ready =
            ngx_proxy_wasm_func_lookup(pwm, "proxy_on_tick");
    }

    /* http callouts */

    pwm->proxy_on_http_call_response =
        ngx_proxy_wasm_func_lookup(pwm, "proxy_on_http_call_response");

    /* grpc callouts */

    pwm->proxy_on_grpc_call_response_header_metadata =
        ngx_proxy_wasm_func_lookup(pwm, "proxy_on_grpc_call_response_header_metadata");
    pwm->proxy_on_grpc_call_response_message =
        ngx_proxy_wasm_func_lookup(pwm, "proxy_on_grpc_call_response_message");
    pwm->proxy_on_grpc_call_response_trailer_metadata =
        ngx_proxy_wasm_func_lookup(pwm, "proxy_on_grpc_call_response_trailer_metadata");
    pwm->proxy_on_grpc_call_close =
        ngx_proxy_wasm_func_lookup(pwm, "proxy_on_grpc_call_close");

    /* custom extensions */

    pwm->proxy_on_custom_callback =
        ngx_proxy_wasm_func_lookup(pwm, "proxy_on_custom_callback");

    if (pwm->abi_version < NGX_PROXY_WASM_VNEXT) {
        /* 0.2.0 - 0.2.1 */
        pwm->proxy_on_custom_callback =
            ngx_proxy_wasm_func_lookup(pwm, "proxy_on_foreign_function");
    }

    /* validate */

    if (pwm->proxy_on_context_create == NULL
        || pwm->proxy_on_vm_start == NULL
        || pwm->proxy_on_plugin_start == NULL)
    {
        fctx->ecode = NGX_PROXY_WASM_ERR_BAD_MODULE_INTERFACE;
        ngx_proxy_wasm_log_error(NGX_LOG_EMERG, pwm->log, fctx->ecode,
                                 "missing one of: on_context_create, "
                                 "on_vm_start, on_plugin_start");
        return NGX_ERROR;
    }

    /* init plugin */

    fctx->instance = ngx_wavm_instance_create(pwm->lmodule, pwm->pool, pwm->log,
                                              NULL, &pwm->root_prctx);
    if (fctx->instance == NULL) {
        fctx->ecode = NGX_PROXY_WASM_ERR_INSTANCE_FAILED;
        goto error;
    }

    pwm->ctxid = ngx_crc32_long(pwm->module->name.data, pwm->module->name.len);

    /* start plugin */

    rc = ngx_wavm_instance_call_funcref(fctx->instance,
                                        pwm->proxy_on_context_create, &rets,
                                        pwm->ctxid, NGX_PROXY_WASM_ROOT_CTX_ID);
    if (rc != NGX_OK) {
        goto error;
    }

    rc = ngx_wavm_instance_call_funcref(fctx->instance,
                                        pwm->proxy_on_vm_start, &rets,
                                        pwm->ctxid, 0); // TODO: size
    if (rc != NGX_OK || !rets->data[0].of.i32) {
        goto error;
    }

    rc = ngx_wavm_instance_call_funcref(fctx->instance,
                                        pwm->proxy_on_plugin_start, &rets,
                                        pwm->ctxid,
                                        pwm->filter_config.len);
    if (rc != NGX_OK || !rets->data[0].of.i32) {
        goto error;
    }

    fctx->ecode = NGX_PROXY_WASM_ERR_NONE;

    rc = NGX_OK;

    goto done;

error:

    if (!fctx->ecode) {
        fctx->ecode = NGX_PROXY_WASM_ERR_UNKNOWN;
    }

    ngx_proxy_wasm_log_error(NGX_LOG_EMERG, pwm->log, 0,
                             "failed initializing proxy_wasm filter");

    rc = NGX_ERROR;

done:

    return rc;
}


void
ngx_proxy_wasm_destroy(ngx_proxy_wasm_t *pwm)
{
    ngx_proxy_wasm_filter_ctx_t  *fctx;

    fctx = pwm->root_prctx.filters[0];

    ngx_wavm_instance_destroy(fctx->instance);

    ngx_pfree(pwm->pool, pwm->root_prctx.filters);
}


/* ngx_proxy_wasm_ctx_t */


ngx_wavm_ptr_t
ngx_proxy_wasm_alloc(ngx_proxy_wasm_filter_ctx_t *fctx, size_t size)
{
   ngx_wavm_ptr_t        p;
   ngx_int_t             rc;
   ngx_wavm_instance_t  *instance;
   wasm_val_vec_t       *rets;

   instance = fctx->instance;

   rc = ngx_wavm_instance_call_funcref(instance, fctx->filter->proxy_on_memory_allocate,
                                       &rets, size);
   if (rc != NGX_OK) {
       ngx_wasm_log_error(NGX_LOG_CRIT, instance->log, 0,
                          "proxy_wasm_alloc(%uz) failed", size);
       return 0;
   }

   p = rets->data[0].of.i32;

   ngx_log_debug3(NGX_LOG_DEBUG_WASM, instance->log, 0,
                  "proxy_wasm_alloc: %uz:%uz:%uz",
                  wasm_memory_data_size(instance->memory), p, size);

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
ngx_proxy_wasm_resume(ngx_proxy_wasm_filter_ctx_t *fctx, ngx_wasm_phase_t *phase)
{
    ngx_int_t                     rc;
    ngx_proxy_wasm_t             *pwm;
    ngx_proxy_wasm_filter_ctx_t  *root_fctx;

    pwm = fctx->filter;
    root_fctx = pwm->root_prctx.filters[0];

    ngx_log_debug2(NGX_LOG_DEBUG_WASM, fctx->log, 0,
                   "proxy_wasm resuming \"%V\" module in \"%V\" phase",
                   &pwm->module->name, &phase->name);

    if (root_fctx->ecode || fctx->ecode) {
        return pwm->ecode_(fctx, phase, root_fctx->ecode || fctx->ecode);
    }

    rc = pwm->resume_(fctx, phase);

    ngx_log_debug3(NGX_LOG_DEBUG_WASM, fctx->log, 0,
                   "proxy_wasm exiting \"%V\" module in \"%V\" phase rc: %d",
                   &pwm->module->name, &phase->name, rc);

    return rc;
}


ngx_int_t
ngx_proxy_wasm_on_log(ngx_proxy_wasm_filter_ctx_t *fctx)
{
    ngx_proxy_wasm_t  *filter = fctx->filter;

    if (filter->abi_version < NGX_PROXY_WASM_VNEXT) {
        /* 0.1.0 - 0.2.1 */
        if (ngx_wavm_instance_call_funcref(fctx->instance, filter->proxy_on_done,
                                           NULL, fctx->id) != NGX_OK
            || ngx_wavm_instance_call_funcref(fctx->instance, filter->proxy_on_log,
                                              NULL, fctx->id) != NGX_OK)
        {
            return NGX_ERROR;
        }
    }

    return NGX_OK;
}


// TODO: declaration
void
ngx_proxy_wasm_filter_ctx_free(ngx_proxy_wasm_filter_ctx_t *fctx)
{
    ngx_proxy_wasm_ctx_t  *prctx = fctx->filter_ctx;

    ngx_log_debug2(NGX_LOG_DEBUG_WASM, fctx->log, 0,
                   "proxy_wasm freeing stream context id \"#%d\""
                   " (fctx: %p)", fctx->id, fctx);

    prctx->filters[fctx->filter->filter_idx] = NULL;

    if (fctx->ecode != NGX_PROXY_WASM_ERR_INSTANCE_TRAPPED) {
        (void) ngx_wavm_instance_call_funcref(fctx->instance,
                                              fctx->filter->proxy_on_context_finalize,
                                              NULL, fctx->id);
    }

    if (fctx->instance) {
        ngx_wavm_instance_destroy(fctx->instance);
    }

    ngx_pfree(prctx->pool, fctx);
}


void
ngx_proxy_wasm_on_done(ngx_proxy_wasm_filter_ctx_t *fctx)
{
    ngx_proxy_wasm_ctx_t  *prctx = fctx->filter_ctx;

    ngx_log_debug2(NGX_LOG_DEBUG_WASM, fctx->log, 0,
                   "proxy wasm id %l finalizing filter %u/?",
                   fctx->id, fctx->filter->filter_idx);

    /* check if all filters are finished for this request */

    if (fctx->filter->filter_idx < prctx->max_filters - 1) {
        ngx_proxy_wasm_filter_ctx_free(fctx);
        return;
    }

    /* all filters are finished */

    ngx_proxy_wasm_filter_ctx_free(fctx);

    ngx_log_debug1(NGX_LOG_DEBUG_WASM, fctx->log, 0,
                   "proxy wasm destroying stream context id \"#%d\"",
                   fctx->id);

    fctx->filter->destroy_context_(prctx);
}
