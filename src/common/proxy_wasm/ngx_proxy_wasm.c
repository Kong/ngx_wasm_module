#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_proxy_wasm.h>


static void ngx_proxy_wasm_log_error(ngx_uint_t level, ngx_log_t *log,
    ngx_proxy_wasm_err_t err, const char *fmt, ...);


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


static ngx_inline ngx_wavm_func_t *
ngx_proxy_wasm_module_func_lookup(ngx_proxy_wasm_module_t *pwm, const char *n)
{
    ngx_str_t   name = ngx_string(n);

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
    ngx_wavm_ctx_t        ctx;
    wasm_val_t            args[2], rets[1];

    ngx_wasm_assert(pwm->pool);
    ngx_wasm_assert(pwm->log);
    ngx_wasm_assert(pwm->module);

    pwm->ecode = 0;

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

    pwm->proxy_on_memory_allocate =
        ngx_proxy_wasm_module_func_lookup(pwm, "proxy_on_memory_allocate");

    pwm->proxy_on_context_create =
        ngx_proxy_wasm_module_func_lookup(pwm, "proxy_on_context_create");
    pwm->proxy_on_context_finalize =
        ngx_proxy_wasm_module_func_lookup(pwm, "proxy_on_context_finalize");

    pwm->proxy_on_vm_start =
        ngx_proxy_wasm_module_func_lookup(pwm, "proxy_on_vm_start");
    pwm->proxy_on_configure =
        ngx_proxy_wasm_module_func_lookup(pwm, "proxy_on_configure");

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
        || pwm->proxy_on_configure)
    {
        vm = pwm->module->vm;

        ngx_wavm_ctx_init(vm, &ctx);

        instance = ngx_wavm_instance_create(pwm->lmodule, &ctx);
        if (instance == NULL) {
            goto error;
        }

        if (pwm->proxy_on_context_create) {
            pwm->ctxid = ngx_crc32_long(pwm->module->name.data,
                                        pwm->module->name.len);

            ngx_wasm_set_i32(args[0], pwm->ctxid);
            ngx_wasm_set_i32(args[1], NGX_PROXY_WASM_ROOT_CTX_ID);

            rc = ngx_wavm_instance_call(instance, pwm->proxy_on_vm_start,
                                        args, 2, NULL, 0);
            if (rc != NGX_OK) {
                goto error;
            }
        }

        if (pwm->proxy_on_vm_start) {
            ngx_wasm_set_i32(args[0], pwm->ctxid);
            ngx_wasm_set_i32(args[1], 0);

            rc = ngx_wavm_instance_call(instance, pwm->proxy_on_vm_start,
                                        args, 2, rets, 1);
            if (rc != NGX_OK) {
                goto error;
            }

            if (!rets[0].of.i32) {
                goto error;
            }
        }

        if (pwm->proxy_on_configure) {
            ngx_wasm_set_i32(args[0], pwm->ctxid);
            ngx_wasm_set_i32(args[1], 0);

            rc = ngx_wavm_instance_call(instance, pwm->proxy_on_vm_start,
                                        args, 2, rets, 1);
            if (rc != NGX_OK) {
                goto error;
            }

            if (!rets[0].of.i32) {
                goto error;
            }
        }

        ngx_wavm_ctx_destroy(&ctx);
    }

    return NGX_OK;

error:

    ngx_wavm_ctx_destroy(&ctx);

    return NGX_ERROR;
}


ngx_int_t
ngx_proxy_wasm_module_resume(ngx_proxy_wasm_module_t *pwm,
    ngx_wasm_phase_t *phase)
{
    /* stub */

    if (pwm->ecode) {
        ngx_proxy_wasm_log_error(NGX_LOG_EMERG, pwm->log, pwm->ecode, NULL);
        return NGX_ERROR;
    }

    return NGX_OK;
}


static void
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

    p = ngx_proxy_wasm_strerror(err, buf, last - buf);

    ngx_wasm_log_error(level, log, 0, "%*s", p - buf, buf);
}
