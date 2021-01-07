#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_http.h>
#include <ngx_wasm_phases.h>
#include <ngx_wasm_util.h>


static ngx_int_t ngx_wasm_phases_op_call_handler(
    ngx_wasm_phases_ctx_t *pctx, ngx_wasm_phase_t *subsys_phase,
    ngx_wasm_phases_op_t *op);
static ngx_int_t ngx_wasm_phases_op_proxy_wasm_handler(
    ngx_wasm_phases_ctx_t *pctx, ngx_wasm_phase_t *subsys_phase,
    ngx_wasm_phases_op_t *op);


static ngx_wasm_phase_t ngx_wasm_phases_http[] = {

    { ngx_string("post_read"),
      NGX_HTTP_POST_READ_PHASE,
      0 },

    { ngx_string("rewrite"),
      NGX_HTTP_REWRITE_PHASE,
      (1 << NGX_HTTP_REWRITE_PHASE) },

    { ngx_string("content"),
      NGX_HTTP_CONTENT_PHASE,
      (1 << NGX_HTTP_CONTENT_PHASE) },

    { ngx_string("log"),
      NGX_HTTP_LOG_PHASE,
      (1 << NGX_HTTP_LOG_PHASE) },

    { ngx_null_string, 0, 0 }
};


static ngx_int_t
ngx_wasm_phases_add_op_helper(ngx_pool_t *pool,
    ngx_wasm_phases_engine_t *phengine, ngx_wasm_phases_op_t *op)
{
    ngx_wasm_phase_t           *subsys_phase = ngx_wasm_phases_http;
    ngx_wasm_phases_phase_t    *phase;
    ngx_wasm_phases_op_t      **opp;

    for (/* void */; subsys_phase->name.len; subsys_phase++) {

        if ((op->on_phases & subsys_phase->on)) {

            /* create phase ops array */

            phase = phengine->phases[subsys_phase->index];
            if (phase == NULL) {
                phase = ngx_pcalloc(pool, sizeof(ngx_wasm_phases_phase_t));
                if (phase == NULL) {
                    return NGX_ERROR;
                }

                phase->subsys_phase = subsys_phase;
                phase->ops = ngx_array_create(pool, 2,
                                              sizeof(ngx_wasm_phases_op_t *));
                if (phase->ops == NULL) {
                    return NGX_ERROR;
                }

                phengine->phases[subsys_phase->index] = phase;
            }

            /* add op */

            opp = ngx_array_push(phase->ops);
            if (opp == NULL) {
                return NGX_ERROR;
            }

            *opp = op;
        }
    }

    return NGX_OK;
}


ngx_wasm_phases_op_t *
ngx_wasm_phases_conf_add_op_call(ngx_conf_t *cf, ngx_wasm_phases_engine_t *phengine,
    ngx_str_t *phase_name, ngx_str_t *mod_name, ngx_str_t *func_name)
{
    u_char                *p;
    ngx_wasm_phases_op_t  *op;
    ngx_wasm_phase_t      *subsys_phase;

    if (phase_name->len == 0) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "invalid phase \"%V\"",
                           phase_name);
        return NULL;
    }

    switch (phengine->subsys) {

    case NGX_WASM_HOST_SUBSYS_HTTP:
        subsys_phase = ngx_wasm_phases_http;
        break;

    default:
        ngx_conf_log_error(NGX_LOG_ALERT, cf, 0,
                           "NYI - phases: unknown subsystem");
        ngx_wasm_assert(0);
        return NULL;

    }

    for (/* void */; subsys_phase->name.len; subsys_phase++) {
        if (ngx_strncmp(phase_name->data, subsys_phase->name.data,
                        subsys_phase->name.len) == 0)
        {
            break;
        }
    }

    if (subsys_phase->name.len == 0) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "unknown phase \"%V\"",
                           phase_name);
        return NULL;
    }

    if (subsys_phase->on == 0) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "unsupported phase \"%V\"",
                           phase_name);
        return NULL;
    }

    if (mod_name->len == 0) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "invalid module name \"%V\"",
                           mod_name);
        return NULL;
    }

    if (func_name->len == 0) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "invalid function name \"%V\"",
                           func_name);
        return NULL;
    }

    op = ngx_pcalloc(cf->pool, sizeof(ngx_wasm_phases_op_t));
    if (op == NULL) {
        return NULL;
    }

    op->code = NGX_WASM_PHASES_OP_CALL;
    op->handler = ngx_wasm_phases_op_call_handler;
    op->on_phases = subsys_phase->on;

    op->conf.call.mod_name.len = mod_name->len;
    op->conf.call.mod_name.data = ngx_pnalloc(cf->pool,
                                              op->conf.call.mod_name.len + 1);
    if (op->conf.call.mod_name.data == NULL) {
        return NULL;
    }

    p = ngx_copy(op->conf.call.mod_name.data, mod_name->data,
                 op->conf.call.mod_name.len);
    *p = '\0';

    op->conf.call.func_name.len = func_name->len;
    op->conf.call.func_name.data = ngx_pnalloc(cf->pool,
                                               op->conf.call.func_name.len + 1);
    if (op->conf.call.func_name.data == NULL) {
        return NULL;
    }

    p = ngx_copy(op->conf.call.func_name.data, func_name->data,
                 op->conf.call.func_name.len);
    *p = '\0';

    if (!ngx_wasm_vm_has_module(phengine->vm, &op->conf.call.mod_name)) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "[wasm] no \"%V\" module defined",
                           &op->conf.call.mod_name);
        return NULL;
    }

    if (ngx_wasm_phases_add_op_helper(cf->pool, phengine, op) != NGX_OK) {
        return NULL;
    }

    return op;
}


ngx_wasm_phases_op_t *
ngx_wasm_phases_conf_add_op_proxy_wasm(ngx_conf_t *cf,
    ngx_wasm_phases_engine_t *phengine, ngx_str_t *mod_name)
{
    u_char                *p;
    ngx_wasm_phases_op_t  *op;

    if (mod_name->len == 0) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "invalid module name \"%V\"",
                           mod_name);
        return NULL;
    }

    op = ngx_pcalloc(cf->pool, sizeof(ngx_wasm_phases_op_t));
    if (op == NULL) {
        return NULL;
    }

    op->code = NGX_WASM_PHASES_OP_PROXY_WASM;
    op->handler = ngx_wasm_phases_op_proxy_wasm_handler;
    op->on_phases = (1 << NGX_HTTP_REWRITE_PHASE)
                    | (1 << NGX_HTTP_LOG_PHASE);

    op->conf.proxy_wasm.mod_name.len = mod_name->len;
    op->conf.proxy_wasm.mod_name.data = ngx_pnalloc(cf->pool,
                                          op->conf.proxy_wasm.mod_name.len + 1);
    if (op->conf.proxy_wasm.mod_name.data == NULL) {
        return NULL;
    }

    p = ngx_copy(op->conf.proxy_wasm.mod_name.data, mod_name->data,
                 op->conf.proxy_wasm.mod_name.len);
    *p = '\0';

    if (ngx_wasm_phases_add_op_helper(cf->pool, phengine, op) != NGX_OK) {
        return NULL;
    }

    return op;
}


ngx_int_t
ngx_wasm_phases_resume(ngx_wasm_phases_ctx_t *pctx, ngx_uint_t phase_idx)
{
    size_t                    i;
    ngx_uint_t                rc;
    ngx_wasm_phase_t         *subsys_phase = ngx_wasm_phases_http;
    ngx_wasm_phases_phase_t  *phase;
    ngx_wasm_phases_op_t     *op;

    for (/* void */; subsys_phase->name.len; subsys_phase++) {
        if (phase_idx == subsys_phase->index) {
            break;
        }
    }

    if (subsys_phase->index == 0) {
        ngx_wasm_log_error(NGX_LOG_ALERT, pctx->log, 0,
                           "phase engine: unknown phase index '%u'",
                           phase_idx);
        return NGX_ERROR;
    }

    phase = pctx->phengine->phases[subsys_phase->index];
    if (phase == NULL) {
        ngx_log_debug1(NGX_LOG_DEBUG_WASM, pctx->log, 0,
                       "wasm phase engine resume: no phase for index '%u'",
                       phase);
        return NGX_DECLINED;
    }

    for (i = 0; i < phase->ops->nelts; i++) {
        op = ((ngx_wasm_phases_op_t **) phase->ops->elts)[i];

        rc = op->handler(pctx, subsys_phase, op);
        if (rc != NGX_OK) {
            return NGX_ERROR;
        }
    }

    return NGX_OK;
}


static ngx_int_t
ngx_wasm_phases_op_call_handler(ngx_wasm_phases_ctx_t *pctx,
    ngx_wasm_phase_t *subsys_phase, ngx_wasm_phases_op_t *op)
{
    ngx_wasm_hctx_t                  hctx;
    ngx_wasm_phases_engine_t        *phengine = pctx->phengine;
    ngx_wasm_phases_op_call_conf_t  *op_conf;
    ngx_wasm_vm_instance_t          *inst;
    ngx_int_t                        rc;

    ngx_wasm_assert(op->code == NGX_WASM_PHASES_OP_CALL);

    op_conf = &op->conf.call;

    ngx_log_debug3(NGX_LOG_DEBUG_WASM, pctx->log, 0,
                   "wasm calling \"%V.%V\" in \"%V\" phase",
                   &op_conf->mod_name, &op_conf->func_name,
                   &subsys_phase->name);

    if (pctx->vmcache) {
        inst = ngx_wasm_vm_cache_get_instance(pctx->vmcache, phengine->vm,
                                              &op_conf->mod_name);

    } else {
        inst = ngx_wasm_vm_instance_new(phengine->vm, &op_conf->mod_name);
    }

    if (inst == NULL) {
        return NGX_ERROR;
    }

    hctx.pool = pctx->pool;
    hctx.log = pctx->log;
    hctx.data = pctx->data;
    hctx.subsys = phengine->subsys;

    ngx_wasm_vm_instance_set_hctx(inst, &hctx);

    rc = ngx_wasm_vm_instance_call(inst, &op_conf->func_name);

    if (pctx->vmcache == NULL) {
        ngx_wasm_vm_instance_free(inst);
    }

    return rc;
}


static ngx_int_t
ngx_wasm_phases_op_proxy_wasm_handler(ngx_wasm_phases_ctx_t *pctx,
    ngx_wasm_phase_t *subsys_phase, ngx_wasm_phases_op_t *op)
{
    return NGX_OK;
}
