#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_wasm_ffi.h>


ngx_wavm_t *
ngx_wasm_ffi_main_vm()
{
    ngx_wavm_t  *vm;

    vm = ngx_wasm_main_vm((ngx_cycle_t *) ngx_cycle);

    dd("vm: %p", vm);

    return vm;
}


#ifdef NGX_WASM_HTTP
ngx_int_t
ngx_http_wasm_ffi_plan_new(ngx_wavm_t *vm,
    ngx_wasm_ffi_filter_t *filters, size_t n_filters,
    ngx_wasm_ops_plan_t **out, u_char *err, size_t *errlen)
{
    size_t                      i;
    ngx_int_t                   rc;
    ngx_wasm_ops_plan_t        *plan;
    ngx_wasm_ffi_filter_t      *ffi_filter;
    ngx_http_wasm_main_conf_t  *mcf;
    static ngx_uint_t           isolation = NGX_PROXY_WASM_ISOLATION_STREAM;

    mcf = ngx_http_cycle_get_module_main_conf(ngx_cycle, ngx_http_wasm_module);

    plan = ngx_wasm_ops_plan_new(vm->pool, &ngx_http_wasm_subsystem);
    if (plan == NULL) {
        return NGX_ERROR;
    }

    for (i = 0; i < n_filters; i++) {
        ffi_filter = filters + i;

        dd("filter[%ld] name: \"%.*s\" (config: \"%.*s\")", i,
           (int) ffi_filter->name->len, ffi_filter->name->data,
           (int) ffi_filter->config->len, ffi_filter->config->data);

        rc = ngx_http_wasm_ops_add_filter(plan,
                                          ffi_filter->name,
                                          ffi_filter->config,
                                          &isolation,
                                          &mcf->store, vm);
        if (rc != NGX_OK) {
            if (rc == NGX_ABORT) {
                *errlen = ngx_snprintf(err, *errlen,
                                       "no \"%V\" module defined",
                                       ffi_filter->name) - err;
            }

            return NGX_ERROR;
        }
    }

    *out = plan;

    return NGX_OK;
}


void
ngx_http_wasm_ffi_plan_free(ngx_wasm_ops_plan_t *plan)
{
    ngx_log_debug1(NGX_LOG_DEBUG_WASM, ngx_cycle->log, 0,
                   "wasm freeing plan: %p", plan);

    ngx_wasm_ops_plan_destroy(plan);
}


ngx_int_t
ngx_http_wasm_ffi_plan_load(ngx_wasm_ops_plan_t *plan)
{
    ngx_log_debug1(NGX_LOG_DEBUG_WASM, ngx_cycle->log, 0,
                   "wasm loading plan: %p", plan);

    if (ngx_wasm_ops_plan_load(plan, ngx_cycle->log) != NGX_OK) {
        return NGX_ERROR;
    }

    return ngx_proxy_wasm_start((ngx_cycle_t *) ngx_cycle);
}


ngx_int_t
ngx_http_wasm_ffi_plan_attach(ngx_http_request_t *r, ngx_wasm_ops_plan_t *plan)
{
    ngx_int_t                  rc;
    ngx_http_wasm_req_ctx_t   *rctx;
    ngx_http_wasm_loc_conf_t  *loc;

    if (!plan->loaded) {
        return NGX_DECLINED;
    }

    loc = ngx_http_get_module_loc_conf(r, ngx_http_wasm_module);

    loc->plan = plan;

    rc = ngx_http_wasm_rctx(r, &rctx);
    if (rc != NGX_OK) {
        return NGX_ERROR;
    }

    loc->plan = NULL;

    if (!rctx->ffi_attached) {
        /**
         * ngx_http_wasm_module's rewrite handler already executed,
         * invoke resume again since we are still in rewrite phase
         */
        rc = ngx_wasm_ops_resume(&rctx->opctx, NGX_HTTP_REWRITE_PHASE);

        /* ignore errors: resume could trap, but attaching succeeded */

        ngx_wasm_assert(rc == NGX_OK
                        || rc == NGX_AGAIN
                        || rc >= NGX_HTTP_SPECIAL_RESPONSE);
    }

    rctx->ffi_attached = 1;

    return NGX_OK;
}
#endif