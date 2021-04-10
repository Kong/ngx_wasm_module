#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_http_wasm.h>
#include <ngx_wasm_ops.h>


static ngx_int_t ngx_wasm_op_call_handler(ngx_wasm_op_ctx_t *ctx,
    ngx_wasm_phase_t *phase, ngx_wasm_op_t *op);
static ngx_int_t ngx_wasm_op_proxy_wasm_handler(ngx_wasm_op_ctx_t *ctx,
    ngx_wasm_phase_t *phase, ngx_wasm_op_t *op);


static ngx_inline ngx_wasm_phase_t *
ngx_wasm_ops_engine_phase_lookup(ngx_wasm_ops_engine_t *ops_engine,
    ngx_uint_t phaseidx)
{
    ngx_wasm_phase_t  *phase = ops_engine->subsystem->phases;

    while (phase->index != phaseidx) {
        phase++;
    }

    if (phase->name.len == 0) {
        return NULL;
    }

    return phase;
}


ngx_wasm_ops_engine_t *
ngx_wasm_ops_engine_new(ngx_pool_t *pool, ngx_wavm_t *vm,
    ngx_wasm_subsystem_t *subsystem)
{
    ngx_wasm_ops_engine_t   *ops_engine;

    ops_engine = ngx_pcalloc(pool, sizeof(ngx_wasm_ops_engine_t));
    if (ops_engine == NULL) {
        return NULL;
    }

    ops_engine->pool = pool;
    ops_engine->vm = vm;
    ops_engine->subsystem = subsystem;
    ops_engine->pipelines = ngx_pcalloc(pool, subsystem->nphases *
                                        sizeof(ngx_wasm_ops_pipeline_t));
    if (ops_engine->pipelines == NULL) {
        ngx_pfree(pool, ops_engine);
        return NULL;
    }

    return ops_engine;
}


void
ngx_wasm_ops_engine_init(ngx_wasm_ops_engine_t *engine)
{
    size_t                    i, j;
    ngx_wasm_op_t            *op;
    ngx_wasm_ops_pipeline_t  *pipeline;

    for (i = 0; i < engine->subsystem->nphases; i++) {
        pipeline = engine->pipelines[i];

        if (pipeline == NULL) {
            continue;
        }

        for (j = 0; j < pipeline->ops->nelts; j++) {
            op = ((ngx_wasm_op_t **) pipeline->ops->elts)[j];

            switch (op->code) {

            case NGX_WASM_OP_CALL:
                op->lmodule = ngx_wavm_module_link(op->module, op->host);
                if (op->lmodule == NULL) {
                    return;
                }

                op->conf.call.funcref = ngx_wavm_module_func_lookup(op->module,
                                            &op->conf.call.func_name);
                if (op->conf.call.funcref == NULL) {
                    ngx_wasm_log_error(NGX_LOG_EMERG, engine->pool->log, 0,
                                       "no \"%V\" function in \"%V\" module",
                                       &op->conf.call.func_name,
                                       &op->module->name);
                }

                break;

            case NGX_WASM_OP_PROXY_WASM:
                if (op->conf.proxy_wasm.pwmodule->lmodule) {
                    break;
                }

                op->lmodule = ngx_wavm_module_link(op->module, op->host);
                if (op->lmodule == NULL) {
                    return;
                }

                op->conf.proxy_wasm.pwmodule->lmodule = op->lmodule;

                if (ngx_proxy_wasm_init(op->conf.proxy_wasm.pwmodule)
                    != NGX_OK)
                {
                    return;
                }

                break;

            default:
                ngx_wasm_log_error(NGX_LOG_WASM_NYI, engine->pool->log, 0,
                                   "unknown wasm op code: %d", op->code);
                return;

            }
        }
    }
}


void
ngx_wasm_ops_engine_destroy(ngx_wasm_ops_engine_t *engine)
{
    size_t                    i, j;
    ngx_wasm_op_t            *op;
    ngx_wasm_ops_pipeline_t  *pipeline;

    for (i = 0; i < engine->subsystem->nphases; i++) {
        pipeline = engine->pipelines[i];

        if (pipeline == NULL) {
            continue;
        }

        for (j = 0; j < pipeline->ops->nelts; j++) {
            op = ((ngx_wasm_op_t **) pipeline->ops->elts)[j];

            switch (op->code) {

            case NGX_WASM_OP_CALL:
                break;

            case NGX_WASM_OP_PROXY_WASM:
                ngx_proxy_wasm_destroy(op->conf.proxy_wasm.pwmodule);
                break;

            default:
                ngx_wasm_log_error(NGX_LOG_WASM_NYI, engine->pool->log, 0,
                                   "unknown wasm op code: %d", op->code);
                break;

            }
        }
    }
}


static ngx_int_t
ngx_wasm_op_add_helper(ngx_conf_t *cf, ngx_wasm_ops_engine_t *ops_engine,
    ngx_wasm_op_t *op, ngx_str_t *mod_name)
{
    ngx_wasm_phase_t          *phase = ops_engine->subsystem->phases;
    ngx_wasm_ops_pipeline_t   *pipeline;
    ngx_wasm_op_t            **opp;

    if (mod_name->len == 0) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "invalid module name \"%V\"", mod_name);
        return NGX_ERROR;
    }

    op->module = ngx_wavm_module_lookup(ops_engine->vm, mod_name);
    if (op->module == NULL) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "no \"%V\" module defined", mod_name);
        return NGX_ERROR;
    }

    for (/* void */; phase->name.len; phase++) {

        if ((op->on_phases & phase->on)) {

            pipeline = ops_engine->pipelines[phase->index];
            if (pipeline == NULL) {
                pipeline = ngx_pcalloc(ops_engine->pool,
                                       sizeof(ngx_wasm_ops_pipeline_t));
                if (pipeline == NULL) {
                    return NGX_ERROR;
                }

                pipeline->phase = phase;
                pipeline->ops = ngx_array_create(ops_engine->pool, 2,
                                                 sizeof(ngx_wasm_op_t *));
                if (pipeline->ops == NULL) {
                    return NGX_ERROR;
                }

                ops_engine->pipelines[phase->index] = pipeline;
            }

            opp = ngx_array_push(pipeline->ops);
            if (opp == NULL) {
                return NGX_ERROR;
            }

            *opp = op;
        }
    }

    return NGX_OK;
}


ngx_wasm_op_t *
ngx_wasm_conf_add_op_call(ngx_conf_t *cf, ngx_wasm_ops_engine_t *ops_engine,
    ngx_wavm_host_def_t *host, ngx_str_t *value)
{
    ngx_wasm_op_t     *op;
    ngx_wasm_phase_t  *phase = ops_engine->subsystem->phases;
    ngx_str_t         *phase_name, *mod_name, *func_name;

    phase_name = &value[1];
    mod_name = &value[2];
    func_name = &value[3];

    if (phase_name->len == 0) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "invalid phase \"%V\"",
                           phase_name);
        return NULL;
    }

    if (func_name->len == 0) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "invalid function name \"%V\"",
                           func_name);
        return NULL;
    }

    for (/* void */; phase->name.len; phase++) {
        if (ngx_strncmp(phase_name->data, phase->name.data, phase->name.len)
            == 0)
        {
            break;
        }
    }

    if (phase->name.len == 0) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "unknown phase \"%V\"",
                           phase_name);
        return NULL;
    }

    if (phase->on == 0) {
        ngx_conf_log_error(NGX_LOG_WASM_NYI, cf, 0, "unsupported phase \"%V\"",
                           phase_name);
        return NULL;
    }

    op = ngx_pcalloc(cf->pool, sizeof(ngx_wasm_op_t));
    if (op == NULL) {
        return NULL;
    }

    op->code = NGX_WASM_OP_CALL;
    op->handler = ngx_wasm_op_call_handler;
    op->on_phases = phase->on;
    op->host = host;

    op->conf.call.func_name = *func_name;

    if (ngx_wasm_op_add_helper(cf, ops_engine, op, mod_name) != NGX_OK) {
        return NULL;
    }

    return op;
}


ngx_wasm_op_t *
ngx_wasm_conf_add_op_proxy_wasm(ngx_conf_t *cf,
    ngx_wasm_ops_engine_t *ops_engine, ngx_str_t *value,
    ngx_proxy_wasm_t *pwmodule)
{
    ngx_wasm_op_t  *op;
    ngx_str_t      *mod_name;

    mod_name = &value[1];

    op = ngx_pcalloc(cf->pool, sizeof(ngx_wasm_op_t));
    if (op == NULL) {
        return NULL;
    }

    op->code = NGX_WASM_OP_PROXY_WASM;
    op->handler = ngx_wasm_op_proxy_wasm_handler;
    op->host = &ngx_proxy_wasm_host;
    op->on_phases = (1 << NGX_HTTP_REWRITE_PHASE)
                    | (1 << NGX_HTTP_WASM_HEADER_FILTER_PHASE)
                    | (1 << NGX_HTTP_LOG_PHASE);

    if (ngx_wasm_op_add_helper(cf, ops_engine, op, mod_name) != NGX_OK) {
        return NULL;
    }

    op->conf.proxy_wasm.pwmodule = pwmodule;
    op->conf.proxy_wasm.pwmodule->module = op->module;

    return op;
}


ngx_int_t
ngx_wasm_ops_resume(ngx_wasm_op_ctx_t *ctx, ngx_uint_t phaseidx)
{
    size_t                    i;
    ngx_wasm_phase_t         *phase;
    ngx_wasm_ops_engine_t    *ops_engine = ctx->ops_engine;
    ngx_wasm_ops_pipeline_t  *pipeline;
    ngx_wasm_op_t            *op;
    ngx_int_t                 rc = NGX_DECLINED;

    if (ops_engine == NULL) {
        return NGX_DECLINED;
    }

    phase = ngx_wasm_ops_engine_phase_lookup(ops_engine, phaseidx);
    if (phase == NULL) {
        ngx_wasm_log_error(NGX_LOG_WARN, ctx->log, 0,
                           "ops resume: no phase for index '%ui'",
                           phase);
        goto rc;
    }

    ngx_log_debug2(NGX_LOG_DEBUG_WASM, ctx->log, 0,
                   "wasm ops resuming \"%V\" phase (idx: %ui)",
                   &phase->name, phaseidx);

    pipeline = ctx->ops_engine->pipelines[phase->index];
    if (pipeline == NULL) {
        goto rc;
    }

    for (i = 0; i < pipeline->ops->nelts; i++) {
        op = ((ngx_wasm_op_t **) pipeline->ops->elts)[i];
        if (op->lmodule == NULL) {
            rc = NGX_ERROR;
            goto rc;
        }

        rc = op->handler(ctx, phase, op);

        if (rc == NGX_ERROR || rc > NGX_OK) {
            /* NGX_ERROR, NGX_HTTP_* */
            break;
        }

        if (rc == NGX_OK) {
            goto rc;
        }

        ngx_wasm_assert(rc != NGX_AGAIN);
        ngx_wasm_assert(rc != NGX_DONE);

        /* NGX_DECLINED, NGX_HTTP_* */
    }

rc:

    ngx_log_debug2(NGX_LOG_DEBUG_WASM, ctx->log, 0,
                   "wasm ops \"%V\" phase rc: %d",
                   &phase->name, rc);

    return rc;
}


static ngx_int_t
ngx_wasm_op_call_handler(ngx_wasm_op_ctx_t *ctx, ngx_wasm_phase_t *phase,
    ngx_wasm_op_t *op)
{
    ngx_int_t             rc;
    ngx_wavm_instance_t  *instance;
    ngx_wavm_funcref_t   *funcref;

    ngx_wasm_assert(op->code == NGX_WASM_OP_CALL);

    funcref = op->conf.call.funcref;
    if (funcref == NULL) {
        return NGX_ERROR;
    }

    ngx_log_debug3(NGX_LOG_DEBUG_WASM, ctx->log, 0,
                   "wasm ops calling \"%V.%V\" in \"%V\" phase",
                   &op->module->name, &funcref->name, &phase->name);

    instance = ngx_wavm_instance_create(op->lmodule, &ctx->wvctx);
    if (instance == NULL) {
        return NGX_ERROR;
    }

    rc = ngx_wavm_instance_call_funcref_vec(instance, funcref, NULL, NULL);
    if (rc == NGX_ERROR) {
        return rc;
    }

    ngx_wasm_assert(rc == NGX_OK);

    return NGX_DECLINED;
}


static ngx_int_t
ngx_wasm_op_proxy_wasm_handler(ngx_wasm_op_ctx_t *ctx, ngx_wasm_phase_t *phase,
    ngx_wasm_op_t *op)
{
    ngx_proxy_wasm_t  *pwmodule;

    ngx_wasm_assert(op->code == NGX_WASM_OP_PROXY_WASM);

    pwmodule = op->conf.proxy_wasm.pwmodule;
    if (pwmodule == NULL) {
        return NGX_ERROR;
    }

    return ngx_proxy_wasm_resume(pwmodule, phase, &ctx->wvctx);
}
