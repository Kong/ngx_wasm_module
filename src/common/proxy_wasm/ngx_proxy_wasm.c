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
    ngx_string("incompatible sdk interface"),
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


static ngx_proxy_wasm_abi_version_t
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
    wasm_val_vec_t       *rets;

    ngx_wasm_assert(pwm->pool);
    ngx_wasm_assert(pwm->log);
    ngx_wasm_assert(pwm->module);

    pwm->ecode = 0;
    pwm->tick_period = 0;
    pwm->max_pairs = NGX_HTTP_WASM_MAX_REQ_HEADERS;

    vm = pwm->module->vm;

    /* linked module check */

    if (pwm->lmodule == NULL) {
        pwm->ecode = NGX_PROXY_WASM_ERR_BAD_HOST_INTERFACE;
        return NGX_ERROR;
    }

    /* abi version compat */

    pwm->abi_version = ngx_proxy_wasm_module_abi_version(pwm);

    switch (pwm->abi_version) {

    case NGX_PROXY_WASM_0_1_0:
    case NGX_PROXY_WASM_0_2_1:
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

    /* retrieve SDK */

    pwm->proxy_on_memory_allocate =
        ngx_proxy_wasm_module_func_lookup(pwm, "malloc");

    if (pwm->proxy_on_memory_allocate == NULL) {
        pwm->proxy_on_memory_allocate =
            ngx_proxy_wasm_module_func_lookup(pwm, "proxy_on_memory_allocate");
        if (pwm->proxy_on_memory_allocate == NULL) {
            pwm->ecode = NGX_PROXY_WASM_ERR_BAD_MODULE_INTERFACE;
            ngx_proxy_wasm_log_error(NGX_LOG_EMERG, pwm->log, pwm->ecode,
                                     "missing malloc");
            return NGX_ERROR;
        }
    }

    /* context */

    pwm->proxy_on_context_create =
        ngx_proxy_wasm_module_func_lookup(pwm, "proxy_on_context_create");
    pwm->proxy_on_context_finalize =
        ngx_proxy_wasm_module_func_lookup(pwm, "proxy_on_context_finalize");

    if (pwm->abi_version < NGX_PROXY_WASM_VNEXT) {
        /* 0.1.0 - 0.2.1 */
        pwm->proxy_on_done =
            ngx_proxy_wasm_module_func_lookup(pwm, "proxy_on_done");
        pwm->proxy_on_log =
            ngx_proxy_wasm_module_func_lookup(pwm, "proxy_on_log");
        pwm->proxy_on_context_finalize =
            ngx_proxy_wasm_module_func_lookup(pwm, "proxy_on_delete");
    }

    /* configuration */

    pwm->proxy_on_vm_start =
        ngx_proxy_wasm_module_func_lookup(pwm, "proxy_on_vm_start");
    pwm->proxy_on_plugin_start =
        ngx_proxy_wasm_module_func_lookup(pwm, "proxy_on_plugin_start");

    if (pwm->abi_version < NGX_PROXY_WASM_VNEXT) {
        /* 0.1.0 - 0.2.1 */
        pwm->proxy_on_plugin_start =
            ngx_proxy_wasm_module_func_lookup(pwm, "proxy_on_configure");
    }

    /* stream */

    pwm->proxy_on_new_connection =
        ngx_proxy_wasm_module_func_lookup(pwm, "proxy_on_new_connection");
    pwm->proxy_on_downstream_data =
        ngx_proxy_wasm_module_func_lookup(pwm, "proxy_on_downstream_data");
    pwm->proxy_on_upstream_data =
        ngx_proxy_wasm_module_func_lookup(pwm, "proxy_on_upstream_data");
    pwm->proxy_on_downstream_close =
        ngx_proxy_wasm_module_func_lookup(pwm, "proxy_on_downstream_close");
    pwm->proxy_on_upstream_close =
        ngx_proxy_wasm_module_func_lookup(pwm, "proxy_on_upstream_close");

    if (pwm->abi_version < NGX_PROXY_WASM_VNEXT) {
        /* 0.1.0 - 0.2.1 */
        pwm->proxy_on_downstream_close =
            ngx_proxy_wasm_module_func_lookup(pwm, "proxy_on_downstream_connection_close");
        pwm->proxy_on_upstream_close =
            ngx_proxy_wasm_module_func_lookup(pwm, "proxy_on_upstream_connection_close");
    }

    /* http */

    pwm->proxy_on_http_request_headers =
        ngx_proxy_wasm_module_func_lookup(pwm, "proxy_on_http_request_headers");
    pwm->proxy_on_http_request_body =
        ngx_proxy_wasm_module_func_lookup(pwm, "proxy_on_http_request_body");
    pwm->proxy_on_http_request_trailers =
        ngx_proxy_wasm_module_func_lookup(pwm, "proxy_on_http_request_trailers");
    pwm->proxy_on_http_request_metadata =
        ngx_proxy_wasm_module_func_lookup(pwm, "proxy_on_http_request_metadata");
    pwm->proxy_on_http_response_headers =
        ngx_proxy_wasm_module_func_lookup(pwm, "proxy_on_http_response_headers");
    pwm->proxy_on_http_response_body =
        ngx_proxy_wasm_module_func_lookup(pwm, "proxy_on_http_response_body");
    pwm->proxy_on_http_response_trailers =
        ngx_proxy_wasm_module_func_lookup(pwm, "proxy_on_http_response_trailers");
    pwm->proxy_on_http_response_metadata =
        ngx_proxy_wasm_module_func_lookup(pwm, "proxy_on_http_response_metadata");

    if (pwm->abi_version < NGX_PROXY_WASM_VNEXT) {
        /* 0.1.0 - 0.2.1 */
        pwm->proxy_on_http_request_headers =
            ngx_proxy_wasm_module_func_lookup(pwm, "proxy_on_request_headers");
        pwm->proxy_on_http_request_body =
            ngx_proxy_wasm_module_func_lookup(pwm, "proxy_on_request_body");
        pwm->proxy_on_http_request_trailers =
            ngx_proxy_wasm_module_func_lookup(pwm, "proxy_on_request_trailers");
        pwm->proxy_on_http_request_metadata =
            ngx_proxy_wasm_module_func_lookup(pwm, "proxy_on_request_metadata");
        pwm->proxy_on_http_response_headers =
            ngx_proxy_wasm_module_func_lookup(pwm, "proxy_on_response_headers");
        pwm->proxy_on_http_response_body =
            ngx_proxy_wasm_module_func_lookup(pwm, "proxy_on_response_body");
        pwm->proxy_on_http_response_trailers =
            ngx_proxy_wasm_module_func_lookup(pwm, "proxy_on_response_trailers");
        pwm->proxy_on_http_response_metadata =
            ngx_proxy_wasm_module_func_lookup(pwm, "proxy_on_response_metadata");
    }

    /* shared queue */

    pwm->proxy_on_queue_ready =
        ngx_proxy_wasm_module_func_lookup(pwm, "proxy_on_queue_ready");

    /* timers */

    pwm->proxy_create_timer =
        ngx_proxy_wasm_module_func_lookup(pwm, "proxy_create_timer");
    pwm->proxy_delete_timer =
        ngx_proxy_wasm_module_func_lookup(pwm, "proxy_delete_timer");
    pwm->proxy_on_timer_ready =
        ngx_proxy_wasm_module_func_lookup(pwm, "proxy_on_timer_ready");

    if (pwm->abi_version < NGX_PROXY_WASM_VNEXT) {
        /* 0.1.0 - 0.2.1 */
        pwm->proxy_on_timer_ready =
            ngx_proxy_wasm_module_func_lookup(pwm, "proxy_on_tick");
    }

    /* http callouts */

    pwm->proxy_on_http_call_response =
        ngx_proxy_wasm_module_func_lookup(pwm, "proxy_on_http_call_response");

    /* grpc callouts */

    pwm->proxy_on_grpc_call_response_header_metadata =
        ngx_proxy_wasm_module_func_lookup(pwm, "proxy_on_grpc_call_response_header_metadata");
    pwm->proxy_on_grpc_call_response_message =
        ngx_proxy_wasm_module_func_lookup(pwm, "proxy_on_grpc_call_response_message");
    pwm->proxy_on_grpc_call_response_trailer_metadata =
        ngx_proxy_wasm_module_func_lookup(pwm, "proxy_on_grpc_call_response_trailer_metadata");
    pwm->proxy_on_grpc_call_close =
        ngx_proxy_wasm_module_func_lookup(pwm, "proxy_on_grpc_call_close");

    /* custom extensions */

    pwm->proxy_on_custom_callback =
        ngx_proxy_wasm_module_func_lookup(pwm, "proxy_on_custom_callback");

    if (pwm->abi_version < NGX_PROXY_WASM_VNEXT) {
        /* 0.2.0 - 0.2.1 */
        pwm->proxy_on_custom_callback =
            ngx_proxy_wasm_module_func_lookup(pwm, "proxy_on_foreign_function");
    }

    /* validate */

    if (pwm->proxy_on_context_create == NULL
        || pwm->proxy_on_vm_start == NULL
        || pwm->proxy_on_plugin_start == NULL)
    {
        pwm->ecode = NGX_PROXY_WASM_ERR_BAD_MODULE_INTERFACE;
        ngx_proxy_wasm_log_error(NGX_LOG_EMERG, pwm->log, pwm->ecode,
                                 "missing context init");
        return NGX_ERROR;
    }

    /* init plugin */

    pwm->wv_ctx.pool = pwm->pool;
    pwm->wv_ctx.log = pwm->log;
    pwm->wv_ctx.data = pwm;

    if (ngx_wavm_ctx_init(vm, &pwm->wv_ctx) != NGX_OK) {
        return NGX_ERROR;
    }

    pwm->instance = ngx_wavm_instance_create(pwm->lmodule, &pwm->wv_ctx);
    if (pwm->instance == NULL) {
        goto error;
    }

    pwm->ctxid = ngx_crc32_long(pwm->module->name.data, pwm->module->name.len);

    /* start plugin */

    rc = ngx_wavm_instance_call_funcref(pwm->instance,
                                        pwm->proxy_on_context_create, &rets,
                                        pwm->ctxid, NGX_PROXY_WASM_ROOT_CTX_ID);
    if (rc != NGX_OK) {
        goto error;
    }

    rc = ngx_wavm_instance_call_funcref(pwm->instance,
                                        pwm->proxy_on_vm_start, &rets,
                                        pwm->ctxid, 0); // TODO: size
    if (rc != NGX_OK || !rets->data[0].of.i32) {
        goto error;
    }

    rc = ngx_wavm_instance_call_funcref(pwm->instance,
                                        pwm->proxy_on_plugin_start, &rets,
                                        pwm->ctxid, 0); // TODO: size
    if (rc != NGX_OK || !rets->data[0].of.i32) {
        goto error;
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
                                 "in \"%V\" phase", &phase->name);
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
    wasm_val_vec_t           *rets;

    //if (!pwm->proxy_on_http_request_headers) {
    //    return NGX_OK;
    //}

    ctx = pwm->instance->ctx;

    ngx_wasm_assert(ctx == &pwm->wv_ctx);

    rctx = (ngx_http_wasm_req_ctx_t *) ctx->data;
    r = rctx->r;

    /* TODO: 64 bits ctxid */
    ctxid = r->connection->number;

    dd("pwm->ctxid: %ld, ctxid: %ld", pwm->ctxid, ctxid);

    if (ngx_wavm_instance_call_funcref(pwm->instance, pwm->proxy_on_context_create,
                                       &rets, ctxid, pwm->ctxid) != NGX_OK)
    {
        return NGX_ERROR;
    }

    if (pwm->abi_version == NGX_PROXY_WASM_0_1_0) {
        /* 0.1.0 */
        rc = ngx_wavm_instance_call_funcref(pwm->instance,
                                            pwm->proxy_on_http_request_headers,
                                            &rets, ctxid,
                                            ngx_http_wasm_req_headers_count(r));

    } else {
        /* 0.2.0+ */
        rc = ngx_wavm_instance_call_funcref(pwm->instance,
                                            pwm->proxy_on_http_request_headers,
                                            &rets, ctxid,
                                            ngx_http_wasm_req_headers_count(r),
                                            0); // TODO: end_of_stream
    }

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
    ngx_int_t                 ctxid;
    ngx_http_wasm_req_ctx_t  *rctx;
    ngx_http_request_t       *r;
    ngx_wavm_ctx_t           *ctx;

    ctx = pwm->instance->ctx;

    ngx_wasm_assert(ctx == &pwm->wv_ctx);

    rctx = (ngx_http_wasm_req_ctx_t *) ctx->data;
    r = rctx->r;

    /* TODO: 64 bits ctxid */
    ctxid = r->connection->number;

    dd("pwm->ctxid: %ld, ctxid: %ld", pwm->ctxid, ctxid);

    if (pwm->abi_version < NGX_PROXY_WASM_VNEXT) {
        /* 0.1.0 - 0.2.1 */
        if (ngx_wavm_instance_call_funcref(pwm->instance, pwm->proxy_on_done,
                                           NULL, ctxid) != NGX_OK
            || ngx_wavm_instance_call_funcref(pwm->instance, pwm->proxy_on_log,
                                              NULL, ctxid) != NGX_OK)
        {
            return NGX_ERROR;
        }
    }

    if (ngx_wavm_instance_call_funcref(pwm->instance,
                                       pwm->proxy_on_context_finalize,
                                       NULL, ctxid) != NGX_OK)
    {
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

    if (err) {
        p = ngx_proxy_wasm_strerror(err, p, last - p);
    }

    if (fmt) {
        if (err && p + 2 <= last) {
            *p++ = ':';
            *p++ = ' ';
        }

        va_start(args, fmt);
        p = ngx_vslprintf(p, last, fmt, args);
        va_end(args);
    }

    ngx_wasm_log_error(level, log, 0, "%*s", p - buf, buf);
}
