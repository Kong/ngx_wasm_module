#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_wasm_lua_ffi.h>
#include <ngx_wa_shm_kv.h>
#ifdef NGX_WASM_HTTP
#include <ngx_http_lua_common.h>
#endif


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
    size_t                          i;
    ngx_int_t                       rc;
    ngx_wasm_ops_plan_t            *plan;
    ngx_wasm_ffi_filter_t          *ffi_filter;
    ngx_proxy_wasm_filters_root_t  *pwroot;
    ngx_http_wasm_main_conf_t      *mcf;

    mcf = ngx_http_cycle_get_module_main_conf(ngx_cycle, ngx_http_wasm_module);
    if (mcf == NULL) {
        /* no http{} block */
        *errlen = ngx_snprintf(err, NGX_WASM_LUA_FFI_MAX_ERRLEN,
                               "%V", &NGX_WASM_STR_NO_HTTP)
                  - err;
        return NGX_ERROR;
    }

    plan = ngx_wasm_ops_plan_new(vm->pool, &ngx_http_wasm_subsystem);
    if (plan == NULL) {
        return NGX_ERROR;
    }

    pwroot = ngx_proxy_wasm_root_alloc(plan->pool);
    if (pwroot == NULL) {
        return NGX_ERROR;
    }

    ngx_proxy_wasm_root_init(pwroot, plan->pool);

    plan->conf.proxy_wasm.pwroot = pwroot;
    plan->conf.proxy_wasm.worker_pwroot = &mcf->pwroot;

    for (i = 0; i < n_filters; i++) {
        ffi_filter = filters + i;

        dd("filter[%ld] \"%.*s\" (config: \"%.*s\", plan: %p)", i,
           (int) ffi_filter->name->len, ffi_filter->name->data,
           (int) ffi_filter->config->len, ffi_filter->config->data,
           plan);

        rc = ngx_http_wasm_ops_add_filter(plan,
                                          ffi_filter->name,
                                          ffi_filter->config,
                                          vm);
        if (rc != NGX_OK) {
            if (rc == NGX_ABORT) {
                *errlen = ngx_snprintf(err, NGX_WASM_LUA_FFI_MAX_ERRLEN,
                                       "no \"%V\" module defined",
                                       ffi_filter->name)
                          - err;
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
    dd("enter (plan: %p)", plan);

    if (plan->conf.proxy_wasm.pwroot) {
        ngx_proxy_wasm_root_destroy(plan->conf.proxy_wasm.pwroot);

        ngx_pfree(plan->pool, plan->conf.proxy_wasm.pwroot);
    }

    ngx_wasm_ops_plan_destroy(plan);
}


ngx_int_t
ngx_http_wasm_ffi_plan_load(ngx_wasm_ops_plan_t *plan)
{
    if (ngx_wasm_ops_plan_load(plan,
                               (ngx_log_t *) &ngx_cycle->new_log)
        != NGX_OK)
    {
        return NGX_ERROR;
    }

    return ngx_proxy_wasm_start(plan->conf.proxy_wasm.pwroot);
}


ngx_int_t
ngx_http_wasm_ffi_plan_attach(ngx_http_request_t *r, ngx_wasm_ops_plan_t *plan,
    ngx_uint_t isolation)
{
    ngx_int_t                  rc;
    ngx_http_lua_ctx_t        *ctx;
    ngx_http_wasm_req_ctx_t   *rctx;
    ngx_http_wasm_loc_conf_t  *loc;
    ngx_wasm_ops_plan_t       *old_plan;

    if (!plan->loaded) {
        return NGX_DECLINED;
    }

    ctx = ngx_http_get_module_ctx(r, ngx_http_lua_module);
    loc = ngx_http_get_module_loc_conf(r, ngx_http_wasm_module);

    old_plan = loc->plan;
    loc->plan = plan;

    rc = ngx_http_wasm_rctx(r, &rctx);
    ngx_wa_assert(rc != NGX_DECLINED);
    if (rc != NGX_OK) {
        return NGX_ERROR;
    }

    loc->plan = old_plan;

    if (rctx->ffi_attached) {
        return NGX_ABORT;
    }

    rctx->ffi_attached = 1;
    rctx->opctx.ctx.proxy_wasm.isolation = isolation;
    rctx->opctx.ctx.proxy_wasm.req_headers_in_access =
        ctx->context == NGX_HTTP_LUA_CONTEXT_ACCESS;

    return NGX_OK;
}


static ngx_inline ngx_proxy_wasm_ctx_t *
get_pwctx(ngx_http_request_t *r)
{
    ngx_http_wasm_req_ctx_t  *rctx;

    if (ngx_http_wasm_rctx(r, &rctx) != NGX_OK) {
        return NULL;
    }

    return ngx_proxy_wasm_ctx(NULL, NULL,
                              NGX_PROXY_WASM_ISOLATION_STREAM,
                              &ngx_http_proxy_wasm, rctx);
}


ngx_int_t
ngx_http_wasm_ffi_set_property(ngx_http_request_t *r,
    ngx_str_t *key, ngx_str_t *value, u_char *err, size_t *errlen)
{
    ngx_int_t              rc;
    ngx_str_t              e = { 0, NULL };
    ngx_proxy_wasm_ctx_t  *pwctx;

    pwctx = get_pwctx(r);
    if (pwctx == NULL) {
        return NGX_ERROR;
    }

    rc = ngx_proxy_wasm_properties_set(pwctx, key, value, &e);
    if (rc == NGX_ERROR && e.len) {
        *errlen = ngx_snprintf(err, NGX_WASM_LUA_FFI_MAX_ERRLEN, "%V", &e)
                  - err;
    }

    dd("exit: %ld", rc);

    return rc;
}


ngx_int_t
ngx_http_wasm_ffi_get_property(ngx_http_request_t *r,
    ngx_str_t *key, ngx_str_t *value, u_char *err, size_t *errlen)
{
    ngx_int_t              rc;
    ngx_str_t              e = { 0, NULL };
    ngx_proxy_wasm_ctx_t  *pwctx;

    pwctx = get_pwctx(r);
    if (pwctx == NULL) {
        /**
         * TODO: return code signifying "no plan attached to request" and co.
         * return associated constant as err/code from Lua lib
         */
        return NGX_ERROR;
    }

    rc = ngx_proxy_wasm_properties_get(pwctx, key, value, &e);
    if (rc == NGX_ERROR && e.len) {
        *errlen = ngx_snprintf(err, NGX_WASM_LUA_FFI_MAX_ERRLEN, "%V", &e)
                  - err;
    }

    dd("exit: %ld", rc);

    return rc;
}


ngx_int_t
ngx_http_wasm_ffi_set_host_property(ngx_http_request_t *r,
    ngx_str_t *key, ngx_str_t *value, unsigned is_const, unsigned retrieve)
{
    ngx_proxy_wasm_ctx_t  *pwctx;

    pwctx = get_pwctx(r);
    if (pwctx == NULL) {
        return NGX_ERROR;
    }

    return ngx_proxy_wasm_properties_set_host(pwctx, key, value,
                                              is_const, retrieve);
}


ngx_int_t
ngx_http_wasm_ffi_set_host_properties_handlers(ngx_http_request_t *r,
    ngx_proxy_wasm_properties_ffi_handler_pt getter,
    ngx_proxy_wasm_properties_ffi_handler_pt setter)
{
    ngx_proxy_wasm_ctx_t  *pwctx;

    pwctx = get_pwctx(r);
    if (pwctx == NULL) {
        return NGX_ERROR;
    }

    return ngx_proxy_wasm_properties_set_ffi_handlers(pwctx, getter, setter, r);
}
#endif


ngx_int_t
ngx_wa_ffi_shm_setup_zones(ngx_wa_ffi_shm_setup_zones_handler_pt setup_handler)
{
    ngx_uint_t             i;
    ngx_array_t           *shms = ngx_wasmx_shms((ngx_cycle_t *) ngx_cycle);
    ngx_wa_shm_t          *shm;
    ngx_wa_shm_mapping_t  *mappings;

    if (shms == NULL) {
        return NGX_ABORT;
    }

    mappings = shms->elts;

    for (i = 0; i < shms->nelts; i++) {
        shm = mappings[i].zone->data;
        setup_handler(shm);
    }

    return NGX_OK;
}


static void
retrieve_keys(ngx_rbtree_node_t *node, ngx_rbtree_node_t *sentinel,
    ngx_uint_t limit, ngx_uint_t start_idx, ngx_uint_t *cur_idx,
    ngx_str_t **keys)
{
    ngx_wa_shm_kv_node_t  *n = (ngx_wa_shm_kv_node_t *) node;

    if ((*cur_idx - start_idx) == limit) {
        return;
    }

    if (*cur_idx >= start_idx) {
        /* append to result */
        keys[(*cur_idx - start_idx)] = &n->key.str;
    }

    (*cur_idx)++;

    if (node->left != sentinel) {
        retrieve_keys(node->left, sentinel, limit, start_idx, cur_idx, keys);
    }

    if (node->right != sentinel) {
        retrieve_keys(node->right, sentinel, limit, start_idx, cur_idx, keys);
    }
}


ngx_int_t
ngx_wa_ffi_shm_iterate_keys(ngx_wa_shm_t *shm, ngx_uint_t page_size,
    ngx_uint_t *clast_idx, ngx_uint_t *cur_idx, ngx_str_t **keys)
{
    ngx_uint_t        last_idx = clast_idx ? *clast_idx : 0;
    ngx_wa_shm_kv_t  *kv;

    ngx_wa_assert(shm->type == NGX_WA_SHM_TYPE_KV
                  || shm->type == NGX_WA_SHM_TYPE_METRICS);

    if (!ngx_wa_shm_locked(shm)) {
        return NGX_ABORT;
    }

    kv = shm->data;

    if (last_idx >= kv->nelts) {
        /* finished */
        return NGX_DONE;
    }

    /* cannot be empty here (kv->nelts > 0) */
    ngx_wa_assert(kv->rbtree.root != kv->rbtree.sentinel);

    retrieve_keys(kv->rbtree.root, kv->rbtree.sentinel, page_size, last_idx,
                  cur_idx, keys);

    *cur_idx -= last_idx;

    if (clast_idx) {
        *clast_idx += *cur_idx;
    }

    return NGX_OK;
}


ngx_uint_t
ngx_wa_ffi_shm_kv_nelts(ngx_wa_shm_t *shm)
{
    ngx_wa_shm_kv_t  *kv;

    ngx_wa_assert(shm->type == NGX_WA_SHM_TYPE_KV
                  || shm->type == NGX_WA_SHM_TYPE_METRICS);

    kv = shm->data;

    return kv->nelts;
}


ngx_int_t
ngx_wa_ffi_shm_kv_get(ngx_wa_shm_t *shm, ngx_str_t *k, ngx_str_t **v,
    uint32_t *cas)
{
    ngx_int_t  rc;
    unsigned   unlock = 0;

    ngx_wa_assert(shm->type == NGX_WA_SHM_TYPE_KV);

    if (!ngx_wa_shm_locked(shm)) {
        /* e.g. already locked if in iterate_keys() */
        ngx_wa_shm_lock(shm);
        unlock = 1;
    }

    rc = ngx_wa_shm_kv_get_locked(shm, k, NULL, v, cas);

    if (unlock) {
        ngx_wa_shm_unlock(shm);
    }

    return rc;
}


ngx_int_t
ngx_wa_ffi_shm_kv_set(ngx_wa_shm_t *shm, ngx_str_t *k, ngx_str_t *v,
    uint32_t cas, unsigned *written)
{
    ngx_int_t  rc;

    ngx_wa_assert(shm->type == NGX_WA_SHM_TYPE_KV);

    if (ngx_wa_shm_locked(shm)) {
        /* shm has already been locked by the current worker */

        return NGX_ABORT;
    }

    ngx_wa_shm_lock(shm);

    rc = ngx_wa_shm_kv_set_locked(shm, k, v, cas, written);

    ngx_wa_shm_unlock(shm);

    return rc;
}


ngx_int_t
ngx_wa_ffi_shm_metric_define(ngx_str_t *name, ngx_wa_metric_type_e type,
    uint32_t *bins, uint16_t n_bins, uint32_t *metric_id)
{
    ngx_int_t          rc;
    ngx_wa_metrics_t  *metrics = ngx_wasmx_metrics((ngx_cycle_t *) ngx_cycle);

    rc = ngx_wa_metrics_define(metrics, name, type, bins, n_bins, metric_id);
    if (rc != NGX_OK) {
        return rc;
    }

    return NGX_OK;
}


ngx_int_t
ngx_wa_ffi_shm_metric_increment(uint32_t metric_id, ngx_uint_t value)
{
    ngx_wa_metrics_t  *metrics = ngx_wasmx_metrics((ngx_cycle_t *) ngx_cycle);

    return ngx_wa_metrics_increment(metrics, metric_id, value);
}


ngx_int_t
ngx_wa_ffi_shm_metric_record(uint32_t metric_id, ngx_uint_t value)
{
    ngx_wa_metrics_t  *metrics = ngx_wasmx_metrics((ngx_cycle_t *) ngx_cycle);

    return ngx_wa_metrics_record(metrics, metric_id, value);
}


ngx_int_t
ngx_wa_ffi_shm_metric_get(uint32_t metric_id, ngx_str_t *name, u_char *m_buf,
    size_t mbs, u_char *h_buf, size_t hbs)
{
    ngx_wa_metrics_t  *metrics = ngx_wasmx_metrics((ngx_cycle_t *) ngx_cycle);
    ngx_wa_metric_t   *m;

    m = (ngx_wa_metric_t *) m_buf;
    ngx_wa_metrics_histogram_set_buffer(m, h_buf, hbs);

    if (metric_id) {
        return ngx_wa_metrics_get(metrics, metric_id, m);
    }

    return ngx_wa_metrics_get(metrics,
                              ngx_crc32_long(name->data, name->len), m);
}
