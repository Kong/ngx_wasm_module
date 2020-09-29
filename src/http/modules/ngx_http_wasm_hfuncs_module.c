/*
 * Copyright (C) Thibault Charbonnier
 */

#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_wasm.h>
#include <ngx_http.h>
#include <ngx_http_wasm_module.h>
#include <ngx_http_wasm_util.h>


ngx_int_t
ngx_http_wasm_hfunc_resp_get_status(ngx_wasm_hctx_t *hctx,
    const ngx_wasm_val_t args[], ngx_wasm_val_t rets[])
{
    ngx_http_wasm_req_ctx_t  *rctx = hctx->data;
    ngx_http_request_t       *r = rctx->r;
    ngx_int_t                 status;

    if (r->connection->fd == (ngx_socket_t) -1) {
        return NGX_WASM_BAD_CTX;
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

    rets[0].kind = NGX_WASM_I32;
    rets[0].value.I32 = status;

    return NGX_WASM_OK;
}


ngx_int_t
ngx_http_wasm_hfunc_resp_say(ngx_wasm_hctx_t *hctx,
    const ngx_wasm_val_t args[], ngx_wasm_val_t rets[])
{
    int64_t                   body_offset, len;
    ngx_int_t                 rc;
    ngx_buf_t                *b;
    ngx_chain_t              *cl;
    ngx_http_wasm_req_ctx_t  *rctx = hctx->data;
    ngx_http_request_t       *r = rctx->r;

    body_offset = args[0].value.I32;
    len = args[1].value.I32;

    if (r->connection->fd == (ngx_socket_t) -1) {
        return NGX_WASM_BAD_CTX;
    }

    b = ngx_create_temp_buf(r->pool, len + sizeof(LF));
    if (b == NULL) {
        return NGX_WASM_ERROR;
    }

    b->last = ngx_copy(b->last, hctx->mem_off + body_offset, len);
    *b->last++ = LF;

    b->last_buf = 1;
    b->last_in_chain = 1;

    cl = ngx_alloc_chain_link(r->connection->pool);
    if (cl == NULL) {
        return NGX_WASM_ERROR;
    }

    cl->buf = b;
    cl->next = NULL;

    rc = ngx_http_wasm_send_chain_link(r, cl);
    if (rc == NGX_ERROR) {
        return NGX_WASM_ERROR;

    } else if (rc == NGX_AGAIN) {
        /* TODO: NYI - NGX_WASM_AGAIN */
        return NGX_AGAIN;
    }

    rctx->sent_last = 1;

    return NGX_WASM_OK;
}


static ngx_wasm_hdecls_t  ngx_http_wasm_hdecls = {
    NGX_WASM_SUBSYS_HTTP,

    {
      { ngx_string("ngx_http_resp_get_status"),
        &ngx_http_wasm_hfunc_resp_get_status,
        ngx_wasm_args_none,
        ngx_wasm_rets_i32 },

      { ngx_string("ngx_http_resp_say"),
        &ngx_http_wasm_hfunc_resp_say,
        ngx_wasm_args_i32_i32,
        ngx_wasm_rets_none },

      ngx_wasm_hfunc_null
    }
};


static ngx_wasm_module_t  ngx_http_wasm_hfuncs_module_ctx = {
    NULL,                                  /* runtime */
    &ngx_http_wasm_hdecls,                 /* hdecls */
    NULL,                                  /* create configuration */
    NULL,                                  /* init configuration */
    NULL,                                  /* init module */
};


ngx_module_t  ngx_http_wasm_hfuncs_module = {
    NGX_MODULE_V1,
    &ngx_http_wasm_hfuncs_module_ctx,    /* module context */
    NULL,                                /* module directives */
    NGX_WASM_MODULE,                     /* module type */
    NULL,                                /* init master */
    NULL,                                /* init module */
    NULL,                                /* init process */
    NULL,                                /* init thread */
    NULL,                                /* exit thread */
    NULL,                                /* exit process */
    NULL,                                /* exit master */
    NGX_MODULE_V1_PADDING
};
