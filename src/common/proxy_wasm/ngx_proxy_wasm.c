#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_http_wasm.h>
#include <ngx_proxy_wasm.h>


static ngx_int_t ngx_proxy_wasm_on_http_request_headers(
    ngx_proxy_wasm_module_t *pwm);
static ngx_int_t ngx_proxy_wasm_on_log(ngx_proxy_wasm_module_t *pwm);


static ngx_str_t  ngx_proxy_wasm_errlist[] = {
    ngx_null_string,
    ngx_string("unknown ABI version"),
    ngx_string("incompatible ABI version"),
    ngx_string("incompatible host interface"),
    ngx_string("unknown error")
};


static u_char *
ngx_proxy_wasm_strerror(ngx_proxy_wasm_err_t err, u_char *buf, size_t size)
{
    ngx_str_t  *msg;

    msg = ((ngx_uint_t) err < NGX_PROXY_WASM_ERR_UNKNOWN)
              ? &ngx_proxy_wasm_errlist[err]
              : &ngx_proxy_wasm_errlist[NGX_PROXY_WASM_ERR_UNKNOWN];

    size = ngx_min(size, msg->len);

    return ngx_cpymem(buf, msg->data, size);
}


static ngx_inline ngx_wavm_funcref_t *
ngx_proxy_wasm_module_func_lookup(ngx_proxy_wasm_module_t *pwm, const char *n)
{
    ngx_str_t   name;

    name.data = (u_char *) n;
    name.len = ngx_strlen(n);

    return ngx_wavm_module_func_lookup(pwm->module, &name);
}


static ngx_proxy_wasm_abi_version
ngx_proxy_wasm_module_abi_version(ngx_proxy_wasm_module_t *pwm)
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


ngx_int_t
ngx_proxy_wasm_module_init(ngx_proxy_wasm_module_t *pwm)
{
    ngx_int_t             rc;
    ngx_wavm_t           *vm;
    ngx_wavm_instance_t  *instance;
    wasm_val_vec_t       *rets;

    ngx_wasm_assert(pwm->pool);
    ngx_wasm_assert(pwm->log);
    ngx_wasm_assert(pwm->module);

    pwm->ecode = 0;
    pwm->tick_period = 0;

    /* linked module check */

    if (pwm->lmodule == NULL) {
        pwm->ecode = NGX_PROXY_WASM_ERR_BAD_HOST_INTERFACE;
        return NGX_ERROR;
    }

    /* abi version compat */

    pwm->abi_version = ngx_proxy_wasm_module_abi_version(pwm);

    switch (pwm->abi_version) {

    case NGX_PROXY_WASM_0_1_0:
        break;

    case NGX_PROXY_WASM_UNKNOWN:
        pwm->ecode = NGX_PROXY_WASM_ERR_UNKNOWN_ABI;
        ngx_proxy_wasm_log_error(NGX_LOG_EMERG, pwm->log, pwm->ecode, NULL);
        return NGX_ERROR;

    default:
        pwm->ecode = NGX_PROXY_WASM_ERR_BAD_ABI;
        ngx_proxy_wasm_log_error(NGX_LOG_EMERG, pwm->log, pwm->ecode, NULL);
        return NGX_ERROR;

    }

    /* integration */

    pwm->proxy_on_memory_allocate =
        ngx_proxy_wasm_module_func_lookup(pwm, "malloc");
    pwm->proxy_on_tick =
        ngx_proxy_wasm_module_func_lookup(pwm, "proxy_on_tick");

    /* context */

    pwm->proxy_on_context_create =
        ngx_proxy_wasm_module_func_lookup(pwm, "proxy_on_context_create");

    if (pwm->abi_version == NGX_PROXY_WASM_0_1_0) {
        pwm->proxy_on_log =
            ngx_proxy_wasm_module_func_lookup(pwm, "proxy_on_log");
        pwm->proxy_on_context_finalize =
            ngx_proxy_wasm_module_func_lookup(pwm, "proxy_on_delete");

    } else {
        pwm->proxy_on_context_finalize =
            ngx_proxy_wasm_module_func_lookup(pwm, "proxy_on_context_finalize");
    }

    /* configuration */

    pwm->proxy_on_vm_start =
        ngx_proxy_wasm_module_func_lookup(pwm, "proxy_on_vm_start");

    if (pwm->abi_version == NGX_PROXY_WASM_0_1_0) {
        pwm->proxy_on_plugin_start =
            ngx_proxy_wasm_module_func_lookup(pwm, "proxy_on_configure");

    } else {
        pwm->proxy_on_plugin_start =
            ngx_proxy_wasm_module_func_lookup(pwm, "proxy_on_plugin_start");
    }

    pwm->proxy_on_new_connection =
        ngx_proxy_wasm_module_func_lookup(pwm, "proxy_on_new_connection");
    pwm->proxy_on_downstream_data =
        ngx_proxy_wasm_module_func_lookup(pwm, "proxy_on_downstream_data");
    pwm->proxy_on_downstream_close =
        ngx_proxy_wasm_module_func_lookup(pwm, "proxy_on_downstream_close");
    pwm->proxy_on_upstream_data =
        ngx_proxy_wasm_module_func_lookup(pwm, "proxy_on_upstream_data");
    pwm->proxy_on_upstream_close =
        ngx_proxy_wasm_module_func_lookup(pwm, "proxy_on_upstream_close");

    pwm->proxy_on_request_headers =
        ngx_proxy_wasm_module_func_lookup(pwm, "proxy_on_request_headers");
    pwm->proxy_on_request_body =
        ngx_proxy_wasm_module_func_lookup(pwm, "proxy_on_request_body");
    pwm->proxy_on_request_trailers =
        ngx_proxy_wasm_module_func_lookup(pwm, "proxy_on_request_trailers");
    pwm->proxy_on_request_metadata =
        ngx_proxy_wasm_module_func_lookup(pwm, "proxy_on_request_metadata");
    pwm->proxy_on_response_headers =
        ngx_proxy_wasm_module_func_lookup(pwm, "proxy_on_response_headers");
    pwm->proxy_on_response_body =
        ngx_proxy_wasm_module_func_lookup(pwm, "proxy_on_response_body");
    pwm->proxy_on_response_trailers =
        ngx_proxy_wasm_module_func_lookup(pwm, "proxy_on_response_trailers");
    pwm->proxy_on_response_metadata =
        ngx_proxy_wasm_module_func_lookup(pwm, "proxy_on_response_metadata");

    pwm->proxy_on_queue_ready =
        ngx_proxy_wasm_module_func_lookup(pwm, "proxy_on_queue_ready");

    pwm->proxy_on_http_call_response =
        ngx_proxy_wasm_module_func_lookup(pwm, "proxy_on_http_call_response");

    pwm->proxy_on_grpc_call_response_header_metadata =
        ngx_proxy_wasm_module_func_lookup(pwm, "proxy_on_grpc_call_response_header_metadata");
    pwm->proxy_on_grpc_call_response_message =
        ngx_proxy_wasm_module_func_lookup(pwm, "proxy_on_grpc_call_response_message");
    pwm->proxy_on_grpc_call_response_trailer_metadata =
        ngx_proxy_wasm_module_func_lookup(pwm, "proxy_on_grpc_call_response_trailer_metadata");
    pwm->proxy_on_grpc_call_close =
        ngx_proxy_wasm_module_func_lookup(pwm, "proxy_on_grpc_call_close");

    pwm->proxy_on_custom_callback =
        ngx_proxy_wasm_module_func_lookup(pwm, "proxy_on_custom_callback");


    if (pwm->proxy_on_context_create
        || pwm->proxy_on_vm_start
        || pwm->proxy_on_plugin_start)
    {
        vm = pwm->module->vm;

        pwm->wv_ctx.pool = pwm->pool;
        pwm->wv_ctx.log = pwm->log;
        pwm->wv_ctx.data = pwm;

        ngx_wavm_ctx_init(vm, &pwm->wv_ctx);

        instance = ngx_wavm_instance_create(pwm->lmodule, &pwm->wv_ctx);
        if (instance == NULL) {
            goto error;
        }

        if (pwm->proxy_on_context_create) {
            pwm->ctxid = ngx_crc32_long(pwm->module->name.data,
                                        pwm->module->name.len);

            rc = ngx_wavm_instance_call_funcref(instance,
                                                pwm->proxy_on_context_create,
                                                &rets, pwm->ctxid,
                                                NGX_PROXY_WASM_ROOT_CTX_ID);
            if (rc != NGX_OK) {
                goto error;
            }
        }

        ngx_wasm_assert(pwm->ctxid);

        if (pwm->proxy_on_vm_start) {
            rc = ngx_wavm_instance_call_funcref(instance,
                                                    pwm->proxy_on_vm_start,
                                                    &rets, pwm->ctxid, 0);
            if (rc != NGX_OK || !rets->data[0].of.i32) {
                goto error;
            }
        }

        if (pwm->proxy_on_plugin_start) {
            rc = ngx_wavm_instance_call_funcref(instance,
                                                pwm->proxy_on_plugin_start,
                                                &rets, pwm->ctxid, 0);
            if (rc != NGX_OK || !rets->data[0].of.i32) {
                goto error;
            }
        }

        pwm->instance = instance;
    }

    return NGX_OK;

error:

    ngx_proxy_wasm_log_error(NGX_LOG_EMERG, pwm->log, 0,
                             "failed initializing proxy_wasm module");

    ngx_wavm_ctx_destroy(&pwm->wv_ctx);

    return NGX_ERROR;
}


void
ngx_proxy_wasm_module_destroy(ngx_proxy_wasm_module_t *pwm)
{
    ngx_wavm_ctx_destroy(&pwm->wv_ctx);
}


/* phases */


ngx_int_t
ngx_proxy_wasm_module_resume(ngx_proxy_wasm_module_t *pwm,
    ngx_wasm_phase_t *phase, ngx_wavm_ctx_t *ctx)
{
    ngx_uint_t                rc;
    ngx_http_request_t       *r;
    ngx_http_wasm_req_ctx_t  *rctx;

    rctx = (ngx_http_wasm_req_ctx_t *) ctx->data;
    r = rctx->r;

    if (pwm->ecode) {
        ngx_proxy_wasm_log_error(NGX_LOG_EMERG, ctx->log, pwm->ecode,
                                 "failed to resume proxy_wasm execution "
                                 "in \"%V\" phase: ", &phase->name);
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    ngx_wavm_ctx_update(&pwm->wv_ctx, ctx->log, ctx->data);

    switch (phase->index) {

    case NGX_HTTP_PREACCESS_PHASE:
        rc = ngx_proxy_wasm_on_http_request_headers(pwm);
        if (rc != NGX_OK) {
            return NGX_HTTP_INTERNAL_SERVER_ERROR;
        }

        break;

    case NGX_HTTP_LOG_PHASE:
        rc = ngx_proxy_wasm_on_log(pwm);
        if (rc != NGX_OK) {
            return NGX_ERROR;
        }

        break;

    default:
        rc = NGX_DECLINED;
        break;

    }

    if (rctx->sent_last) {
        rc = NGX_DONE;

        if (!rctx->finalized) {
            rctx->finalized = 1;

            ngx_http_finalize_request(r, rc);
        }
    }

    return rc;
}


static ngx_int_t
ngx_proxy_wasm_on_http_request_headers(ngx_proxy_wasm_module_t *pwm)
{
    ngx_uint_t                rc;
    ngx_int_t                 ctxid;
    ngx_http_wasm_req_ctx_t  *rctx;
    ngx_http_request_t       *r;
    ngx_wavm_ctx_t           *ctx;
    ngx_wavm_instance_t      *instance;
    wasm_val_vec_t           *rets;

    if (!pwm->proxy_on_request_headers) {
        return NGX_OK;
    }

    instance = pwm->instance;
    ctx = instance->ctx;

    ngx_wasm_assert(ctx == &pwm->wv_ctx);

    rctx = (ngx_http_wasm_req_ctx_t *) ctx->data;
    r = rctx->r;

    /* TODO: 64 bits ctxid */
    ctxid = r->connection->number;

    dd("pwm->ctxid: %ld, ctxid: %ld", pwm->ctxid, ctxid);

    ngx_proxy_wasm_pairs_size(&r->headers_in.headers);

    rc = ngx_wavm_instance_call_funcref(instance, pwm->proxy_on_context_create,
                                        &rets, ctxid, pwm->ctxid);
    if (rc != NGX_OK) {
        return NGX_ERROR;
    }

    /* TODO: on_request_headers_abi_ */

    rc = ngx_wavm_instance_call_funcref(instance, pwm->proxy_on_request_headers,
                                        &rets, ctxid,
                                        ngx_http_wasm_req_headers_count(r));
    if (rc != NGX_OK) {
        return NGX_ERROR;
    }

    switch (rets->data[0].of.i32) {

    case NGX_PROXY_WASM_ACTION_CONTINUE:
        break;

    case NGX_PROXY_WASM_ACTION_PAUSE:
        break;

    default:
        break;

    }

    return NGX_OK;
}


static ngx_int_t
ngx_proxy_wasm_on_log(ngx_proxy_wasm_module_t *pwm)
{
    ngx_uint_t                rc;
    ngx_int_t                 ctxid;
    ngx_http_wasm_req_ctx_t  *rctx;
    ngx_http_request_t       *r;
    ngx_wavm_ctx_t           *ctx;
    ngx_wavm_instance_t      *instance;

    instance = pwm->instance;
    ctx = instance->ctx;

    ngx_wasm_assert(ctx == &pwm->wv_ctx);

    rctx = (ngx_http_wasm_req_ctx_t *) ctx->data;
    r = rctx->r;

    /* TODO: 64 bits ctxid */
    ctxid = r->connection->number;

    dd("pwm->ctxid: %ld, ctxid: %ld", pwm->ctxid, ctxid);

    if (pwm->abi_version == NGX_PROXY_WASM_0_1_0) {
        rc = ngx_wavm_instance_call_funcref(instance, pwm->proxy_on_log,
                                            NULL, ctxid);
        if (rc != NGX_OK) {
            return NGX_ERROR;
        }
    }

    rc = ngx_wavm_instance_call_funcref(instance, pwm->proxy_on_context_finalize,
                                        NULL, ctxid);
    if (rc != NGX_OK) {
        return NGX_ERROR;
    }

    return NGX_OK;
}


void
ngx_proxy_wasm_log_error(ngx_uint_t level, ngx_log_t *log,
    ngx_proxy_wasm_err_t err, const char *fmt, ...)
{
    va_list   args;
    u_char   *p, *last, buf[NGX_MAX_ERROR_STR];

    last = buf + NGX_MAX_ERROR_STR;
    p = &buf[0];

    if (fmt) {
        va_start(args, fmt);
        p = ngx_vsnprintf(buf, NGX_MAX_ERROR_STR, fmt, args);
        va_end(args);
    }

    p = ngx_proxy_wasm_strerror(err, p, last - p);

    ngx_wasm_log_error(level, log, 0, "%*s", p - buf, buf);
}
