#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_http_wasm.h>


static ngx_int_t ngx_http_wasm_filter_init(ngx_conf_t *cf);
static ngx_int_t ngx_http_wasm_header_filter_handler(ngx_http_request_t *r);
static ngx_int_t ngx_http_wasm_body_filter_handler(ngx_http_request_t *r,
    ngx_chain_t *in);


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
static ngx_http_output_body_filter_pt  ngx_http_next_body_filter;


static ngx_int_t
ngx_http_wasm_filter_init(ngx_conf_t *cf)
{
    ngx_http_next_header_filter = ngx_http_top_header_filter;
    ngx_http_top_header_filter = ngx_http_wasm_header_filter_handler;

    ngx_http_next_body_filter = ngx_http_top_body_filter;
    ngx_http_top_body_filter = ngx_http_wasm_body_filter_handler;

    return NGX_OK;
}


static ngx_int_t
ngx_http_wasm_header_filter_handler(ngx_http_request_t *r)
{
    ngx_int_t                  rc;
    ngx_http_wasm_req_ctx_t   *rctx = NULL;

    dd("enter");

    rc = ngx_http_wasm_rctx(r, &rctx);
    if (rc == NGX_ERROR) {
        return NGX_ERROR;

    } else if (rc == NGX_DECLINED) {
        goto next_filter;
    }

    ngx_wasm_assert(rc == NGX_OK);

    if (rctx->resp_header_filter_started) {
        goto next_filter;
    }

    rctx->resp_header_filter_started = 1;
    rctx->resp_content_produced = 1;  /* if not already set */

    rc = ngx_http_wasm_produce_resp_headers(rctx);
    if (rc != NGX_OK) {
        return NGX_ERROR;
    }

    rc = ngx_wasm_ops_resume(&rctx->opctx,
                             NGX_HTTP_WASM_HEADER_FILTER_PHASE);
    dd("header ops resume rc: %ld", rc);
    if (rc == NGX_ERROR || rc > NGX_OK) {
        if (rc == NGX_ERROR) {
            rc = NGX_HTTP_INTERNAL_SERVER_ERROR;
            goto done;
        }

        if (rctx->resp_content_started) {
            dd("resp content started, goto done");
            goto done;
        }

        goto next_filter;

    } else if (rc == NGX_OK) {
        rc = ngx_http_wasm_check_finalize(rctx, rc);
        if (rc == NGX_DONE) {
            ngx_wasm_assert(0);
            goto done;
        }
    }

next_filter:

    (void) ngx_http_next_header_filter(r);

done:

    ngx_log_debug1(NGX_LOG_DEBUG_WASM, r->connection->log, 0,
                   "wasm \"header_filter\" phase rc: %d", rc);

    return rc;
}


static ngx_int_t
ngx_http_wasm_body_filter_handler(ngx_http_request_t *r, ngx_chain_t *in)
{
    ngx_int_t                 rc;
    ngx_http_wasm_req_ctx_t  *rctx;

    dd("enter");

    if (r->header_only) {
        return ngx_http_next_body_filter(r, in);
    }

    rc = ngx_http_wasm_rctx(r, &rctx);
    if (rc == NGX_ERROR) {
        return NGX_ERROR;
    }

    if (rc == NGX_DECLINED) {
        return ngx_http_next_body_filter(r, in);
    }

    ngx_wasm_assert(rc == NGX_OK);

    rctx->resp_chunk = in;
    rctx->resp_chunk_len = ngx_wasm_chain_len(in, &rctx->resp_chunk_eof);

    (void) ngx_wasm_ops_resume(&rctx->opctx,
                               NGX_HTTP_WASM_BODY_FILTER_PHASE);

    rc = ngx_http_next_body_filter(r, rctx->resp_chunk);
    if (rc == NGX_ERROR) {
        return NGX_ERROR;
    }

    rctx->resp_chunk = NULL;

    ngx_chain_update_chains(r->connection->pool,
                            &rctx->free_bufs, &rctx->busy_bufs,
                            &rctx->resp_chunk, buf_tag);

    dd("exit rc: %ld", rc);

    return rc;
}
