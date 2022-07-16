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

    dd("vm:%p", vm);

    return vm;
}


#ifdef NGX_WASM_HTTP
ngx_int_t
ngx_http_wasm_ffi_pwm_new(ngx_wavm_t *vm,
    ngx_wasm_ffi_filter_t *filters, size_t n_filters,
    ngx_wasm_ops_engine_t **out)
{
    size_t                      i;
    ngx_int_t                   rc;
    ngx_proxy_wasm_filter_t    *filter;
    ngx_wasm_ops_engine_t      *engine;
    ngx_wasm_ffi_filter_t      *ffi_filter;
    ngx_http_wasm_main_conf_t  *mcf;
    static ngx_uint_t           isolation = NGX_PROXY_WASM_ISOLATION_NONE;

    mcf = ngx_http_cycle_get_module_main_conf(ngx_cycle, ngx_http_wasm_module);

    engine = ngx_wasm_ops_engine_new(vm->pool, vm,
                                     &ngx_http_wasm_subsystem);
    if (engine == NULL) {
        return NGX_ERROR;
    }

    for (i = 0; i < n_filters; i++) {
        ffi_filter = filters + i;

        dd("filter name: \"%.*s\" (config: \"%.*s\")",
           (int) ffi_filter->name->len, ffi_filter->name->data,
           (int) ffi_filter->config->len, ffi_filter->config->data);

        rc = ngx_http_wasm_ops_add_filter(engine, vm->pool, vm->log,
                                          ffi_filter->name,
                                          ffi_filter->config,
                                          &isolation,
                                          &mcf->store,
                                          &filter);
        if (rc != NGX_OK) {
            if (rc == NGX_ABORT) {
                ngx_wasm_log_error(NGX_LOG_CRIT, vm->log, 0,
                                   "no \"%V\" module defined",
                                   ffi_filter->name);
            }

            ngx_wasm_ops_engine_destroy(engine);

            return NGX_ERROR;
        }

        ffi_filter->filter_id = filter->id;
    }

    rc = ngx_wasm_ops_engine_init(engine);
    if (rc != NGX_OK) {
        ngx_wasm_ops_engine_destroy(engine);
        return NGX_ERROR;
    }

    *out = engine;

    dd("out:%p", *out);

    return NGX_OK;
}


ngx_int_t
ngx_http_wasm_ffi_pwm_resume(ngx_http_request_t *r, ngx_wasm_ops_engine_t *e,
    ngx_uint_t phase)
{
    ngx_int_t                 rc;
    ngx_wasm_op_ctx_t        *opctx;
    ngx_http_wasm_req_ctx_t  *rctx;

    rc = ngx_http_wasm_rctx(r, &rctx);
    if (rc != NGX_OK) {
        goto done;
    }

    opctx = &rctx->opctx;

    if (opctx->ops_engine != e) {
        /* first invocation */
        opctx->ops_engine = e;
    }

    rc = ngx_wasm_ops_resume(opctx, phase);
    if (rc == NGX_ERROR || rc >= NGX_HTTP_SPECIAL_RESPONSE) {
        goto done;
    }

    rc = NGX_OK;

done:

    return rc;
}


void
ngx_http_wasm_ffi_pwm_free(void *cdata)
{
    ngx_wasm_ops_engine_t  *e = cdata;

    ngx_log_debug1(NGX_LOG_DEBUG_ALL, e->pool->log, 0,
                   "free engine:%p", e);

    ngx_wasm_ops_engine_destroy(e);
}
#endif
