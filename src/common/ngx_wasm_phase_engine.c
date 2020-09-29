/*
 * Copyright (C) Thibault Charbonnier
 */

#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_wasm_phase_engine.h>


ngx_int_t ngx_wasm_phase_engine_op_call(ngx_wasm_phase_engine_ctx_t *pctx,
    ngx_wasm_phase_engine_op_t *op);


char *
ngx_wasm_phase_engine_add_op(ngx_conf_t *cf, ngx_wasm_phase_engine_t *phengine,
    ngx_wasm_phase_engine_op_t *op)
{
    ngx_wasm_phase_engine_phase_t    *phase;
    ngx_wasm_phase_engine_op_t      **opp;

    /* validate */

    switch (op->code) {

    case NGX_WASM_PHASE_ENGINE_CALL_OP:
        if (op->mod_name.len == 0) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "invalid module name \"%V\"",
                               &op->mod_name);
            return NGX_CONF_ERROR;
        }

        if (op->func_name.len == 0) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "invalid function name \"%V\"",
                               &op->func_name);
            return NGX_CONF_ERROR;
        }

        if (!ngx_wasm_vm_has_module(phengine->vm, &op->mod_name)) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "[wasm] no \"%V\" module defined", &op->mod_name);
            return NGX_CONF_ERROR;
        }

        op->handler = ngx_wasm_phase_engine_op_call;

        break;

    default:
        ngx_conf_log_error(NGX_LOG_ALERT, cf, 0,
                           "NYI - phase_engine op: %d", op->code);
        return NGX_CONF_ERROR;

    }

    /* create phase */

    phase = phengine->phases[op->phase];
    if (phase == NULL) {
        phase = ngx_pcalloc(cf->pool, sizeof(ngx_wasm_phase_engine_phase_t));
        if (phase == NULL) {
            return NGX_CONF_ERROR;
        }

        phase->name = &op->phase_name;
        phase->ops = ngx_array_create(cf->pool, 2, sizeof(ngx_wasm_phase_engine_op_t *));
        if (phase->ops == NULL) {
            return NGX_CONF_ERROR;
        }

        phengine->phases[op->phase] = phase;
    }

    /* add op */

    opp = ngx_array_push(phase->ops);
    if (opp == NULL) {
        return NGX_CONF_ERROR;
    }

    *opp = op;

    return NGX_CONF_OK;
}


ngx_int_t
ngx_wasm_phase_engine_resume(ngx_wasm_phase_engine_ctx_t *pctx)
{
    size_t                          i;
    ngx_uint_t                      rc;
    ngx_wasm_phase_engine_phase_t  *phase;
    ngx_wasm_phase_engine_op_t     *op;

    phase = pctx->phengine->phases[pctx->cur_phase];
    if (phase == NULL) {
        return NGX_DECLINED;
    }

    for (i = 0; i < phase->ops->nelts; i++) {
        op = ((ngx_wasm_phase_engine_op_t **) phase->ops->elts)[i];

        rc = op->handler(pctx, op);
        if (rc != NGX_OK) {
            return NGX_ERROR;
        }
    }

    return NGX_OK;
}


ngx_int_t
ngx_wasm_phase_engine_op_call(ngx_wasm_phase_engine_ctx_t *pctx,
    ngx_wasm_phase_engine_op_t *op)
{
    ngx_wasm_hctx_t           hctx;
    ngx_wasm_phase_engine_t  *phengine = pctx->phengine;
    ngx_wasm_vm_instance_t   *inst;
    ngx_int_t                 rc;

    ngx_wasm_assert(op->code == NGX_WASM_PHASE_ENGINE_CALL_OP);

    ngx_log_debug3(NGX_LOG_DEBUG_WASM, pctx->log, 0,
                   "wasm calling \"%V.%V\" in \"%V\" phase",
                   &op->mod_name, &op->func_name, &op->phase_name);

    if (pctx->vmcache) {
        inst = ngx_wasm_vm_cache_get_instance(pctx->vmcache, phengine->vm,
                                              &op->mod_name);

    } else {
        inst = ngx_wasm_vm_instance_new(phengine->vm, &op->mod_name);
    }

    if (inst == NULL) {
        return NGX_ERROR;
    }

    hctx.pool = pctx->pool;
    hctx.log = pctx->log;
    hctx.data = pctx->data;
    hctx.subsys = phengine->subsys;

    ngx_wasm_vm_instance_set_hctx(inst, &hctx);

    rc = ngx_wasm_vm_instance_call(inst, &op->func_name);

    if (pctx->vmcache == NULL) {
        ngx_wasm_vm_instance_free(inst);
    }

    return rc;
}
