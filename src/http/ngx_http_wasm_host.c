#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_http_wasm.h>
#include <ngx_wavm.h>


ngx_int_t
ngx_http_wasm_hfuncs_resp_get_status(ngx_wavm_instance_t *instance,
    wasm_val_t args[], wasm_val_t rets[])
{
    ngx_http_wasm_req_ctx_t  *rctx = instance->data;
    ngx_http_request_t       *r = rctx->r;
    uint32_t                  status;

    if (r->connection->fd == NGX_WASM_BAD_FD) {
        return NGX_WAVM_BAD_USAGE;
    }

    if (r->err_status) {
        status = r->err_status;

    } else if (r->headers_out.status) {
        status = r->headers_out.status;

    } else if (r->http_version == NGX_HTTP_VERSION_9) {
        status = 9;

    } else {
        status = 0;
    }

    rets[0].kind = WASM_I32;
    rets[0].of.i32 = status;

    return NGX_WAVM_OK;
}


ngx_int_t
ngx_http_wasm_hfuncs_resp_set_status(ngx_wavm_instance_t *instance,
    wasm_val_t args[], wasm_val_t rets[])
{
    ngx_http_wasm_req_ctx_t  *rctx = instance->data;
    ngx_http_request_t       *r = rctx->r;
    uint32_t                  status;

    if (r->connection->fd == NGX_WASM_BAD_FD) {
        return NGX_WAVM_BAD_USAGE;
    }

    if (r->header_sent) {
        ngx_wavm_instance_trap_printf(instance, "headers already sent");
        return NGX_WAVM_BAD_USAGE;
    }

    status = args[0].of.i32;

    ngx_log_debug1(NGX_LOG_DEBUG_WASM, instance->log, 0,
                   "wasm set resp status to %d", status);

    r->headers_out.status = status;

    if (r->err_status) {
        r->err_status = 0;
    }

    return NGX_WAVM_OK;
}


ngx_int_t
ngx_http_wasm_hfuncs_resp_say(ngx_wavm_instance_t *instance,
    wasm_val_t args[], wasm_val_t rets[])
{
    size_t                    body_len, content_len;
    u_char                   *body;
    ngx_int_t                 rc;
    ngx_buf_t                *b;
    ngx_chain_t              *cl = NULL;
    ngx_http_wasm_req_ctx_t  *rctx = instance->data;
    ngx_http_request_t       *r = rctx->r;

    body_len = args[1].of.i32;
    body = NGX_WAVM_HOST_LIFT_SLICE(instance, args[0].of.i32, body_len);

    if (r->connection->fd == NGX_WASM_BAD_FD) {
        return NGX_WAVM_BAD_USAGE;
    }

    if (r->header_sent) {
        ngx_wavm_instance_trap_printf(instance, "headers already sent");
        return NGX_WAVM_BAD_USAGE;
    }

    content_len = body_len;

    if (body_len) {
        content_len += sizeof(LF);

        rc = ngx_http_set_content_type(r);
        if (rc != NGX_OK) {
            return NGX_WAVM_ERROR;
        }

        b = ngx_create_temp_buf(r->pool, content_len);
        if (b == NULL) {
            return NGX_WAVM_ERROR;
        }

        b->last = ngx_copy(b->last, body, body_len);
        *b->last++ = LF;

        b->last_buf = 1;
        b->last_in_chain = 1;

        cl = ngx_alloc_chain_link(r->connection->pool);
        if (cl == NULL) {
            return NGX_WAVM_ERROR;
        }

        cl->buf = b;
        cl->next = NULL;
    }

    if (ngx_http_wasm_set_resp_content_length(r, content_len) != NGX_OK) {
        return NGX_WAVM_ERROR;
    }

    rc = ngx_http_wasm_send_chain_link(r, cl);
    if (rc == NGX_ERROR) {
        return NGX_WAVM_ERROR;

    } else if (rc == NGX_AGAIN) {
        /* TODO: NYI - NGX_WAVM_AGAIN */
        ngx_wasm_assert(0);
        return NGX_WAVM_NYI;
    }

    return NGX_WAVM_OK;
}


ngx_int_t
ngx_http_wasm_hfuncs_local_response(ngx_wavm_instance_t *instance,
    wasm_val_t args[], wasm_val_t rets[])
{
    size_t                    reason_len;
    u_char                   *reason;
    ngx_int_t                 status;
    ngx_http_wasm_req_ctx_t  *rctx = instance->data;

    status = args[0].of.i32;
    reason_len = args[2].of.i32;
    reason = NGX_WAVM_HOST_LIFT_SLICE(instance, args[1].of.i32, reason_len);

    (void) ngx_http_wasm_stash_local_response(rctx, status, reason, reason_len,
                                              NULL, NULL, 0);

    return NGX_WAVM_OK;
}


static ngx_wavm_host_func_def_t  ngx_http_wasm_hfuncs[] = {

    { ngx_string("ngx_http_resp_get_status"),
      &ngx_http_wasm_hfuncs_resp_get_status,
      NULL,
      ngx_wavm_arity_i32 },

    { ngx_string("ngx_http_resp_set_status"),
      &ngx_http_wasm_hfuncs_resp_set_status,
      ngx_wavm_arity_i32,
      NULL },

    { ngx_string("ngx_http_resp_say"),
      &ngx_http_wasm_hfuncs_resp_say,
      ngx_wavm_arity_i32x2,
      NULL },

    { ngx_string("ngx_http_local_response"),
      &ngx_http_wasm_hfuncs_local_response,
      ngx_wavm_arity_i32x3,
      NULL },

   ngx_wavm_hfunc_null
};


ngx_wavm_host_def_t  ngx_http_wasm_host_interface = {
    ngx_string("ngx_http_wasm"),
    ngx_http_wasm_hfuncs,
};
