#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_http_wasm.h>


static ngx_int_t ngx_http_wasm_filter_init(ngx_conf_t *cf);
static ngx_int_t ngx_http_wasm_header_filter_handler(ngx_http_request_t *r);


static ngx_http_module_t  ngx_http_wasm_module_ctx = {
    NULL,                                /* preconfiguration */
    ngx_http_wasm_filter_init,           /* postconfiguration */
    NULL,                                /* create main configuration */
    NULL,                                /* init main configuration */
    NULL,                                /* create server configuration */
    NULL,                                /* merge server configuration */
    NULL,                                /* create location configuration */
    NULL                                 /* merge location configuration */
};


ngx_module_t  ngx_http_wasm_filter_module = {
    NGX_MODULE_V1,
    &ngx_http_wasm_module_ctx,           /* module context */
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


static ngx_http_output_header_filter_pt  ngx_http_next_header_filter;


static ngx_int_t
ngx_http_wasm_filter_init(ngx_conf_t *cf)
{
    ngx_http_next_header_filter = ngx_http_top_header_filter;
    ngx_http_top_header_filter = ngx_http_wasm_header_filter_handler;

    return NGX_OK;
}


static ngx_int_t
ngx_http_wasm_header_filter_handler(ngx_http_request_t *r)
{
    ngx_int_t                  rc;
    ngx_http_wasm_req_ctx_t   *rctx;

    rc = ngx_http_wasm_rctx(r, &rctx);
    if (rc == NGX_ERROR) {
        return NGX_ERROR;

    } else if (rc == NGX_DECLINED) {
        goto next_filter;
    }

    ngx_wasm_assert(rc == NGX_OK);

    if (rctx->header_filter) {
        goto next_filter;
    }

    rctx->header_filter = 1;
    rctx->reset_resp_shims = 1;

    rc = ngx_http_wasm_produce_resp_headers(rctx);
    if (rc != NGX_OK) {
        return NGX_ERROR;
    }

    rc = ngx_wasm_ops_resume(&rctx->opctx, NGX_HTTP_WASM_HEADER_FILTER_PHASE,
                             NGX_WASM_OPS_RUN_ALL);
    if (rc == NGX_OK) {
        rc = ngx_http_wasm_check_finalize(r, rctx, rc);
        if (rc == NGX_DONE) {
            return NGX_DONE;
        }
    }

next_filter:

    return ngx_http_next_header_filter(r);
}
