#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_wasm_ops.h>
#ifdef NGX_WASM_HTTP
#include <ngx_http_wasm.h>
#include <ngx_http_proxy_wasm.h>
#endif
#ifdef NGX_WASM_STREAM
#include <ngx_stream_wasm.h>
#endif


static ngx_int_t ngx_wasm_op_call_handler(ngx_wasm_op_ctx_t *ctx,
    ngx_wasm_phase_t *phase, ngx_wasm_op_t *op);
static ngx_int_t ngx_wasm_op_proxy_wasm_handler(ngx_wasm_op_ctx_t *ctx,
    ngx_wasm_phase_t *phase, ngx_wasm_op_t *op);


static ngx_inline ngx_wasm_phase_t *
ngx_wasm_ops_phase_lookup(ngx_wasm_ops_t *ops,
    ngx_uint_t phaseidx)
{
    ngx_wasm_phase_t  *phase = ops->subsystem->phases;

    while (phase->index != phaseidx) {
        phase++;
    }

    if (phase->name.len == 0) {
        return NULL;
    }

    return phase;
}


ngx_wasm_ops_t *
ngx_wasm_ops_new(ngx_pool_t *pool, ngx_log_t *log, ngx_wavm_t *vm,
    ngx_wasm_subsystem_t *subsystem)
{
    ngx_wasm_ops_t  *ops;

    ops = ngx_pcalloc(pool, sizeof(ngx_wasm_ops_t));
    if (ops == NULL) {
        return NULL;
    }

    ops->pool = pool;
    ops->log = log;
    ops->vm = vm;
    ops->subsystem = subsystem;

    return ops;
}


void
ngx_wasm_ops_destroy(ngx_wasm_ops_t *ops)
{
    ngx_pfree(ops->pool, ops);
}


ngx_wasm_ops_plan_t *
ngx_wasm_ops_plan_new(ngx_pool_t *pool, ngx_wasm_subsystem_t *subsystem)
{
    size_t                    i;
    ngx_wasm_ops_plan_t      *plan;
    ngx_wasm_ops_pipeline_t  *pipeline;

    plan = ngx_pcalloc(pool, sizeof(ngx_wasm_ops_plan_t));
    if (plan == NULL) {
        return NULL;
    }

    plan->pool = pool;
    plan->subsystem = subsystem;
    plan->pipelines = ngx_pcalloc(pool, subsystem->nphases
                                  * sizeof(ngx_wasm_ops_pipeline_t));
    if (plan->pipelines == NULL) {
        ngx_pfree(pool, plan);
        return NULL;
    }

    for (i = 0; i < plan->subsystem->nphases; i++) {
        pipeline = &plan->pipelines[i];

        ngx_array_init(&pipeline->ops, plan->pool, 2,
                       sizeof(ngx_wasm_op_t *));
    }

    return plan;
}


ngx_int_t
ngx_wasm_ops_plan_add(ngx_wasm_ops_plan_t *plan,
    ngx_wasm_op_t **ops_list, size_t nops)
{
    size_t                     i;
    ngx_wasm_phase_t          *phase;
    ngx_wasm_op_t            **opp, *op;
    ngx_wasm_ops_pipeline_t   *pipeline;

    /* populate pipelines */

    for (i = 0; i < nops; i++) {
        op = ops_list[i];
        phase = plan->subsystem->phases;

        if (op->code == NGX_WASM_OP_PROXY_WASM) {
            plan->conf.proxy_wasm.nfilters++;
        }

        for (/* void */; phase->name.len; phase++) {
            pipeline = &plan->pipelines[phase->index];

            if (op->on_phases & phase->on) {
                if (op->code == NGX_WASM_OP_PROXY_WASM) {
                    op->conf.proxy_wasm.filter->data =
                        (uintptr_t) &op->conf.proxy_wasm.filter;
                }

                opp = ngx_array_push(&pipeline->ops);
                if (opp == NULL) {
                    return NGX_ERROR;
                }

                *opp = op;

                plan->populated = 1;
            }
        }
    }

    return NGX_OK;
}


ngx_int_t
ngx_wasm_ops_plan_load(ngx_wasm_ops_plan_t *plan, ngx_log_t *log)
{
    size_t                    i, j;
    ngx_uint_t               *fid;
    ngx_array_t              *ids;
    ngx_wasm_op_t            *op;
    ngx_wasm_ops_pipeline_t  *pipeline;

    dd("enter");

    if (plan->loaded) {
        dd("plan already loaded (plan: %p)", plan);
        return NGX_OK;
    }

    ngx_log_debug1(NGX_LOG_DEBUG_WASM, log, 0,
                   "wasm loading plan: %p", plan);

    /* initialize pipelines */

    for (i = 0; i < plan->subsystem->nphases; i++) {
        pipeline = &plan->pipelines[i];

        for (j = 0; j < pipeline->ops.nelts; j++) {
            op = ((ngx_wasm_op_t **) pipeline->ops.elts)[j];

            if (ngx_wavm_module_link(op->module, op->host) != NGX_OK) {
                return NGX_ERROR;
            }

            switch (op->code) {
            case NGX_WASM_OP_PROXY_WASM:
                op->handler = &ngx_wasm_op_proxy_wasm_handler;

                if (ngx_proxy_wasm_load(op->conf.proxy_wasm.filter) != NGX_OK) {
                    return NGX_ERROR;
                }

                break;
            case NGX_WASM_OP_CALL:
                op->handler = &ngx_wasm_op_call_handler;
                op->conf.call.funcref =
                    ngx_wavm_module_func_lookup(op->module,
                                                &op->conf.call.func_name);
                break;
            default:
                ngx_wasm_log_error(NGX_LOG_WASM_NYI, log, 0,
                                   "unknown wasm op code: %d", op->code);
                return NGX_ERROR;
            }
        }
    }

    /* create filter_ids list */

    ids = &plan->conf.proxy_wasm.filter_ids;

    ngx_array_init(ids, plan->pool, plan->conf.proxy_wasm.nfilters,
                   sizeof(ngx_uint_t));

#if (NGX_WASM_HTTP)
    pipeline = &plan->pipelines[NGX_HTTP_REWRITE_PHASE];

#elif (NGX_WASM_STREAM)
    /* TODO: currently a stub for compilation */
    pipeline = &plan->pipelines[NGX_STREAM_ACCESS_PHASE];
#endif

    for (i = 0; i < pipeline->ops.nelts; i++) {
        op = ((ngx_wasm_op_t **) pipeline->ops.elts)[i];

        switch (op->code) {
        case NGX_WASM_OP_PROXY_WASM:
            fid = ngx_array_push(ids);
            if (fid == NULL) {
                return NGX_ERROR;
            }

            *fid = op->conf.proxy_wasm.filter->id;
            break;
        default:
            break;
        }
    }

    plan->loaded = 1;

    return NGX_OK;
}


void
ngx_wasm_ops_plan_destroy(ngx_wasm_ops_plan_t *plan)
{
    ngx_log_debug1(NGX_LOG_DEBUG_WASM, ngx_cycle->log, 0,
                   "wasm freeing plan: %p", plan);

    if (plan->loaded) {
        ngx_array_destroy(&plan->conf.proxy_wasm.filter_ids);
    }

    ngx_pfree(plan->pool, plan->pipelines);
}


ngx_int_t
ngx_wasm_ops_plan_attach(ngx_wasm_ops_plan_t *plan, ngx_wasm_op_ctx_t *ctx)
{
    ngx_wasm_assert(plan->loaded);

    ctx->plan = plan;

    return NGX_OK;
}


ngx_int_t
ngx_wasm_ops_resume(ngx_wasm_op_ctx_t *ctx, ngx_uint_t phaseidx)
{
    size_t                    i;
    ngx_int_t                 rc = NGX_DECLINED;
    ngx_wasm_phase_t         *phase;
    ngx_wasm_op_t            *op;
    ngx_wasm_ops_t           *ops;
    ngx_wasm_ops_plan_t      *plan;
    ngx_wasm_ops_pipeline_t  *pipeline;

    ops = ctx->ops;
    plan = ctx->plan;

    phase = ngx_wasm_ops_phase_lookup(ops, phaseidx);
    if (phase == NULL) {
        ngx_wasm_log_error(NGX_LOG_WASM_NYI, ctx->log, 0,
                           "ops resume: no phase for index '%ui'", phase);
        goto done;
    }

    pipeline = &plan->pipelines[phase->index];

    for (i = 0; i < pipeline->ops.nelts; i++) {
        op = ((ngx_wasm_op_t **) pipeline->ops.elts)[i];

        rc = op->handler(ctx, phase, op);
        dd("op rc: %ld", rc);
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

    dd("rc: %ld", rc);

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
    ngx_int_t                    rc = NGX_ERROR;
    ngx_array_t                 *ids;
    ngx_proxy_wasm_ctx_t        *pwctx;
    ngx_proxy_wasm_subsystem_t  *subsystem = NULL;

    ngx_wasm_assert(op->code == NGX_WASM_OP_PROXY_WASM);

#ifdef NGX_WASM_HTTP
    subsystem = &ngx_http_proxy_wasm;
#endif

    ids = &opctx->plan->conf.proxy_wasm.filter_ids;
    opctx->ctx.proxy_wasm.phase = phase;

    pwctx = ngx_proxy_wasm_ctx((ngx_uint_t *) ids->elts, ids->nelts, subsystem,
                               opctx->data);
    if (pwctx == NULL) {
        goto done;
    }

    switch (phase->index) {

#ifdef NGX_WASM_HTTP
    case NGX_HTTP_REWRITE_PHASE:
        if (!pwctx->req_headers_in_access) {
            rc = ngx_proxy_wasm_resume(pwctx, phase,
                                       NGX_PROXY_WASM_STEP_REQ_HEADERS);

        } else {
            if (!pwctx->main) {
                ngx_wasm_log_error(NGX_LOG_WARN, opctx->log, 0,
                                   "proxy_wasm_request_headers_in_access "
                                   "enabled in a subrequest (no access phase)");
            }

            rc = NGX_OK;
        }

        break;

    case NGX_HTTP_ACCESS_PHASE:
        if (pwctx->req_headers_in_access) {
            rc = ngx_proxy_wasm_resume(pwctx, phase,
                                       NGX_PROXY_WASM_STEP_REQ_HEADERS);

        } else {
            rc = NGX_OK;
        }

        break;

    case NGX_HTTP_CONTENT_PHASE:
        rc = ngx_proxy_wasm_resume(pwctx, phase,
                                   NGX_PROXY_WASM_STEP_REQ_BODY_READ);
        break;

    case NGX_HTTP_WASM_HEADER_FILTER_PHASE:
        rc = ngx_proxy_wasm_resume(pwctx, phase,
                                   NGX_PROXY_WASM_STEP_RESP_HEADERS);
        break;

    case NGX_HTTP_WASM_BODY_FILTER_PHASE:
        rc = ngx_proxy_wasm_resume(pwctx, phase,
                                   NGX_PROXY_WASM_STEP_RESP_BODY);
        break;

#ifdef NGX_WASM_RESPONSE_TRAILERS
    case NGX_HTTP_WASM_TRAILER_FILTER_PHASE:
        rc = ngx_proxy_wasm_resume(pwctx, phase,
                                   NGX_PROXY_WASM_STEP_RESP_TRAILERS);
        break;
#endif

    case NGX_HTTP_LOG_PHASE:
        rc = ngx_proxy_wasm_resume(pwctx, phase,
                                   NGX_PROXY_WASM_STEP_LOG);
        break;
#endif

    case NGX_WASM_DONE_PHASE:
        rc = ngx_proxy_wasm_resume(pwctx, phase,
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
