/*
 * Copyright (C) Thibault Charbonnier
 */

#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_core.h>
#include <ngx_http.h>
#include <ngx_wasm_hfuncs.h>


ngx_int_t
ngx_http_wasm_hfunc_resp_get_status(ngx_wasm_hctx_t *hctx,
    const ngx_wasm_val_t args[], ngx_wasm_val_t rets[])
{
    ngx_http_request_t  *r = hctx->data.r;
    ngx_int_t            status;

    if (r->connection->fd == (ngx_socket_t) -1) {
        return NGX_DECLINED;
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

    return NGX_OK;
}


static ngx_wasm_hfunc_decl_t  ngx_http_wasm_hfuncs[] = {

    { ngx_string("ngx_http_resp_get_status"),
      &ngx_http_wasm_hfunc_resp_get_status,
      NGX_WASM_ARGS_NONE,
      NGX_WASM_RETS_I32 },

    ngx_wasm_hfunc_null
};


static ngx_int_t
ngx_http_wasm_hfuncs_init(ngx_conf_t *cf)
{
    ngx_wasm_core_hfuncs_add(cf->cycle, ngx_http_wasm_hfuncs);

    return NGX_OK;
}


static ngx_http_module_t  ngx_http_wasm_hfuncs_module_ctx = {
    ngx_http_wasm_hfuncs_init,           /* preconfiguration */
    NULL,                                /* postconfiguration */
    NULL,                                /* create main configuration */
    NULL,                                /* init main configuration */
    NULL,                                /* create server configuration */
    NULL,                                /* merge server configuration */
    NULL,                                /* create location configuration */
    NULL                                 /* merge location configuration */
};


ngx_module_t  ngx_http_wasm_hfuncs_module = {
    NGX_MODULE_V1,
    &ngx_http_wasm_hfuncs_module_ctx,    /* module context */
    NULL,                                /* module directives */
    NGX_HTTP_MODULE,                     /* module type */
    NULL,                                /* init master */
    NULL,                                /* init module */
    NULL,                                /* init process */
    NULL,                                /* init thread */
    NULL,                                /* exit thread */
    NULL,                                /* exit process */
    NULL,                                /* exit master */
    NGX_MODULE_V1_PADDING
};
