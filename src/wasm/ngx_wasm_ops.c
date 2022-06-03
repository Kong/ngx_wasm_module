#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_wasm_ops.h>
#ifdef NGX_WASM_HTTP
#include <ngx_http_wasm.h>
#endif


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

        if (pipeline == NULL || pipeline->init) {
            continue;
        }

        /* init pipeline */

        pipeline->nproxy_wasm = 0;

        for (j = 0; j < pipeline->ops->nelts; j++) {
            op = ((ngx_wasm_op_t **) pipeline->ops->elts)[j];

            if (op->init) {
                continue;
            }

            (void) ngx_wavm_module_link(op->module, op->host);

            switch (op->code) {

            case NGX_WASM_OP_CALL:
                op->handler = &ngx_wasm_op_call_handler;
                op->conf.call.funcref = ngx_wavm_module_func_lookup(op->module,
                                                 &op->conf.call.func_name);

#if 0
                if (op->conf.call.funcref == NULL) {
                    ngx_wasm_log_error(NGX_LOG_EMERG, engine->pool->log, 0,
                                       "no \"%V\" function in \"%V\" module",
                                       &op->conf.call.func_name,
                                       &op->module->name);
                }
#endif

                break;

            case NGX_WASM_OP_PROXY_WASM:
                op->handler = &ngx_wasm_op_proxy_wasm_handler;
                op->conf.proxy_wasm.filter->n_filters = &pipeline->nproxy_wasm;

                (void) ngx_proxy_wasm_filter_init(op->conf.proxy_wasm.filter);

                pipeline->nproxy_wasm++;
                break;

            default:
                ngx_wasm_log_error(NGX_LOG_WASM_NYI, engine->pool->log, 0,
                                   "unknown wasm op code: %d", op->code);
                return;

            }

            op->init = 1;
        }

        pipeline->init = 1;
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

            if (!op->init) {
                continue;
            }

            switch (op->code) {

            case NGX_WASM_OP_CALL:
                break;

            case NGX_WASM_OP_PROXY_WASM:
                ngx_proxy_wasm_filter_destroy(op->conf.proxy_wasm.filter);
                break;

            default:
                ngx_wasm_log_error(NGX_LOG_WASM_NYI, engine->pool->log, 0,
                                   "unknown wasm op code: %d", op->code);
                break;

            }

            op->init = 0;
        }
    }
}


ngx_int_t
ngx_wasm_ops_add(ngx_wasm_ops_engine_t *ops_engine, ngx_wasm_op_t *op)
{
    ngx_wasm_phase_t          *phase = ops_engine->subsystem->phases;
    ngx_wasm_ops_pipeline_t   *pipeline;
    ngx_wasm_op_t            **opp;

    ngx_wasm_assert(op->on_phases);
    ngx_wasm_assert(op->module);
    ngx_wasm_assert(op->host);

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


ngx_int_t
ngx_wasm_ops_resume(ngx_wasm_op_ctx_t *ctx, ngx_uint_t phaseidx)
{
    size_t                    i;
    ngx_wasm_phase_t         *phase;
    ngx_wasm_ops_engine_t    *ops_engine = ctx->ops_engine;
    ngx_wasm_ops_pipeline_t  *pipeline;
    ngx_wasm_op_t            *op;
    ngx_int_t                 rc = NGX_DECLINED;

    phase = ngx_wasm_ops_engine_phase_lookup(ops_engine, phaseidx);
    if (phase == NULL) {
        ngx_wasm_log_error(NGX_LOG_WASM_NYI, ctx->log, 0,
                           "ops resume: no phase for index '%ui'", phase);
        goto done;
    }

    pipeline = ctx->ops_engine->pipelines[phase->index];

    if (pipeline == NULL) {
        dd("no pipeline");
        goto done;
    }

#if 0
    ngx_log_debug3(NGX_LOG_DEBUG_WASM, ctx->log, 0,
                   "wasm ops resuming \"%V\" phase (idx: %ui, "
                   "nops: %ui)",
                   &phase->name, phaseidx,
                   pipeline ? pipeline->ops->nelts : 0);
#endif

    for (i = 0; i < pipeline->ops->nelts; i++) {
        op = ((ngx_wasm_op_t **) pipeline->ops->elts)[i];

        rc = op->handler(ctx, phase, op);
        if (rc == NGX_ERROR || rc > NGX_OK) {
            /* NGX_ERROR, NGX_HTTP_* */
            goto done;
        }

        if (rc == NGX_DECLINED) {
            dd("next op");
            continue;
        }

        break;
    }

    ngx_wasm_assert(rc == NGX_OK
                    || rc == NGX_DECLINED
                    || rc == NGX_AGAIN
                    || rc == NGX_DONE);

done:

    dd("ops resume rc: %ld", rc);

    return rc;
}


static ngx_int_t
ngx_wasm_op_call_handler(ngx_wasm_op_ctx_t *opctx, ngx_wasm_phase_t *phase,
    ngx_wasm_op_t *op)
{
    ngx_int_t             rc;
    ngx_wavm_instance_t  *instance;
    ngx_wavm_funcref_t   *funcref;

    ngx_wasm_assert(op->code == NGX_WASM_OP_CALL);

    funcref = op->conf.call.funcref;
    if (funcref == NULL) {
        ngx_wasm_log_error(NGX_LOG_ERR, opctx->log, 0,
                           "no \"%V\" function in \"%V\" module",
                           &op->conf.call.func_name,
                           &op->module->name);
        return NGX_ERROR;
    }

    ngx_log_debug3(NGX_LOG_DEBUG_WASM, opctx->log, 0,
                   "wasm ops calling \"%V.%V\" in \"%V\" phase",
                   &op->module->name, &funcref->name, &phase->name);

    instance = ngx_wavm_instance_create(op->module, opctx->pool, opctx->log,
                                        opctx->data);
    if (instance == NULL) {
        return NGX_ERROR;
    }

    rc = ngx_wavm_instance_call_funcref_vec(instance, funcref, NULL, NULL);

    ngx_wavm_instance_destroy(instance);

    if (rc == NGX_ERROR || rc == NGX_ABORT) {
        return NGX_ERROR;
    }

    ngx_wasm_assert(rc == NGX_OK);

    /* next call op */

    return NGX_DECLINED;
}


static ngx_int_t
ngx_wasm_op_proxy_wasm_handler(ngx_wasm_op_ctx_t *opctx,
    ngx_wasm_phase_t *phase, ngx_wasm_op_t *op)
{
    ngx_int_t                 rc = NGX_ERROR;
    ngx_proxy_wasm_ctx_t     *pwctx;
    ngx_proxy_wasm_filter_t  *filter;

    ngx_wasm_assert(op->code == NGX_WASM_OP_PROXY_WASM);

    opctx->ctx.proxy_wasm.phase = phase;

    filter = op->conf.proxy_wasm.filter;
    if (filter == NULL) {
        goto done;
    }

    pwctx = ngx_proxy_wasm_ctx(filter, opctx->data);
    if (pwctx == NULL) {
        goto done;
    }

    rc = ngx_proxy_wasm_ctx_add(pwctx, filter);
    if (rc == NGX_ERROR) {
        goto done;

    } else if (rc == NGX_OK) {
        /* again */
        rc = NGX_DECLINED;
        goto done;
    }

    ngx_wasm_assert(rc == NGX_DONE);

    switch (phase->index) {

#ifdef NGX_WASM_HTTP
    case NGX_HTTP_REWRITE_PHASE:
        rc = ngx_proxy_wasm_ctx_resume(pwctx, phase,
                                       NGX_PROXY_WASM_STEP_INIT_CTX);
        if (rc != NGX_OK) {
            break;
        }

        rc = ngx_proxy_wasm_ctx_resume(pwctx, phase,
                                       NGX_PROXY_WASM_STEP_REQ_HEADERS);
        break;

    case NGX_HTTP_CONTENT_PHASE:
        rc = ngx_proxy_wasm_ctx_resume(pwctx, phase,
                                       NGX_PROXY_WASM_STEP_REQ_BODY_READ);
        break;

    case NGX_HTTP_WASM_HEADER_FILTER_PHASE:
        rc = ngx_proxy_wasm_ctx_resume(pwctx, phase,
                                       NGX_PROXY_WASM_STEP_RESP_HEADERS);
        break;

    case NGX_HTTP_WASM_BODY_FILTER_PHASE:
        rc = ngx_proxy_wasm_ctx_resume(pwctx, phase,
                                       NGX_PROXY_WASM_STEP_RESP_BODY);
        break;

    case NGX_HTTP_LOG_PHASE:
        rc = ngx_proxy_wasm_ctx_resume(pwctx, phase,
                                       NGX_PROXY_WASM_STEP_LOG);
        break;
#endif

    case NGX_WASM_DONE_PHASE:
        rc = ngx_proxy_wasm_ctx_resume(pwctx, phase,
                                       NGX_PROXY_WASM_STEP_DONE);
        break;

    default:
        ngx_wasm_log_error(NGX_LOG_WASM_NYI, opctx->log, 0,
                           "NYI - ngx_wasm_op_proxy_wasm_handler "
                           "phase index %d", phase->index);

        rc = NGX_DECLINED;
        break;

    }

done:

    ngx_wasm_assert(rc >= NGX_OK           /* step completed */
                    || rc == NGX_ERROR
                    || rc == NGX_AGAIN     /* step yielded */
                    || rc == NGX_DECLINED  /* handled by caller */
                    || rc == NGX_DONE);    /* all steps finished */

    return rc;
}
