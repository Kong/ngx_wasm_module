/*
 * Copyright (C) Thibault Charbonnier
 */

#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_wasm_phase_engine.h>


static char *ngx_http_wasm_call_directive_post(ngx_conf_t *cf, void *post,
    void *data);
static ngx_int_t ngx_http_wasm_init(ngx_conf_t *cf);
static ngx_int_t ngx_http_wasm_rewrite_handler(ngx_http_request_t *r);
static ngx_int_t ngx_http_wasm_content_handler(ngx_http_request_t *r);
static ngx_int_t ngx_http_wasm_log_handler(ngx_http_request_t *r);


static ngx_conf_post_t  ngx_http_wasm_call_post =
    { ngx_http_wasm_call_directive_post };


static ngx_command_t  ngx_http_wasm_module_cmds[] = {

    { ngx_string("wasm_isolation"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_wasm_phase_engine_isolation_directive,
      NGX_HTTP_LOC_CONF_OFFSET,
      NGX_HTTP_MODULE,
      NULL },

    { ngx_string("wasm_call"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE3,
      ngx_wasm_phase_engine_call_directive,
      NGX_HTTP_LOC_CONF_OFFSET,
      NGX_HTTP_MODULE,
      &ngx_http_wasm_call_post },

      ngx_null_command
};


static ngx_http_module_t  ngx_http_wasm_module_ctx = {
    NULL,                                         /* preconfiguration */
    ngx_http_wasm_init,                           /* postconfiguration */
    ngx_wasm_phase_engine_create_main_conf_annx,  /* create main configuration */
    NULL,                                         /* init main configuration */
    ngx_wasm_phase_engine_create_leaf_conf_annx,  /* create server configuration */
    ngx_wasm_phase_engine_merge_leaf_conf_annx,   /* merge server configuration */
    ngx_wasm_phase_engine_create_leaf_conf_annx,  /* create location configuration */
    ngx_wasm_phase_engine_merge_leaf_conf_annx    /* merge location configuration */
};


ngx_module_t  ngx_http_wasm_module = {
    NGX_MODULE_V1,
    &ngx_http_wasm_module_ctx,           /* module context */
    ngx_http_wasm_module_cmds,           /* module directives */
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


static ngx_http_handler_pt  phase_handlers[NGX_HTTP_LOG_PHASE + 1] = {
    NULL,                                /* post_read */
    NULL,                                /* server_rewrite */
    NULL,                                /* find_config */
    ngx_http_wasm_rewrite_handler,       /* rewrite */
    NULL,                                /* post_rewrite */
    NULL,                                /* pre_access */
    NULL,                                /* access */
    NULL,                                /* post_access*/
    NULL,                                /* pre_content */
    ngx_http_wasm_content_handler,       /* content */
    ngx_http_wasm_log_handler            /* log */
};


/* configuration & init */


static char *
ngx_http_wasm_call_directive_post(ngx_conf_t *cf, void *post,
    void *data)
{
    ngx_http_core_loc_conf_t             *clcf;
    ngx_wasm_phase_engine_phase_engine_t  *phase_engine = data;

    if (phase_handlers[phase_engine->phase] == NULL) {
        ngx_conf_log_error(NGX_LOG_ALERT, cf, 0, "NYI - http wasm: "
                           "unsupported phase \"%V\"",
                           &phase_engine->phase_name);
        return NGX_CONF_ERROR;
    }

    if (phase_engine->phase == NGX_HTTP_CONTENT_PHASE) {
        clcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);
        if (clcf == NULL) {
            return NGX_CONF_ERROR;
        }

        clcf->handler = ngx_http_wasm_content_handler;
    }

    return NGX_CONF_OK;
}


static ngx_int_t
ngx_http_wasm_init(ngx_conf_t *cf)
{
    size_t                       i;
    ngx_http_handler_pt         *h;
    ngx_http_core_main_conf_t   *cmcf;

    cmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);

    for (i = 0; i <= NGX_HTTP_LOG_PHASE; i++) {
        if (phase_handlers[i]) {
            h = ngx_array_push(&cmcf->phases[i].handlers);
            if (h == NULL) {
                return NGX_ERROR;
            }

            *h = phase_handlers[i];
        }
    }

    return NGX_OK;
}


/* phases */


static ngx_wasm_phase_engine_rctx_t *
ngx_http_wasm_rctx(ngx_http_request_t *r)
{
    ngx_wasm_phase_engine_rctx_t  *rctx;

    rctx = ngx_http_get_module_ctx(r, ngx_http_wasm_module)
           ? ngx_http_get_module_ctx(r, ngx_http_wasm_module)
           : ngx_pcalloc(r->pool, sizeof(ngx_wasm_phase_engine_rctx_t));

    if (rctx == NULL) {
        return NULL;
    }

    rctx->pool = r->connection->pool;

    return rctx;
}


static ngx_inline ngx_int_t
ngx_http_wasm_phase_ctx(ngx_wasm_phase_engine_phase_ctx_t *pctx,
    ngx_http_request_t *r, ngx_uint_t phase)
{
    pctx->phase = phase;
    pctx->pool = r->pool;
    pctx->log = r->connection->log;
    pctx->mcf = ngx_http_get_module_main_conf(r, ngx_http_wasm_module);
    pctx->srv = ngx_http_get_module_srv_conf(r, ngx_http_wasm_module);
    pctx->lcf = ngx_http_get_module_loc_conf(r, ngx_http_wasm_module);
    pctx->data = r;
    pctx->rctx = ngx_http_wasm_rctx(r);
    if (pctx->rctx == NULL) {
        return NGX_ERROR;
    }

    return NGX_OK;
}


static ngx_int_t
ngx_http_wasm_rewrite_handler(ngx_http_request_t *r)
{
    ngx_int_t                          rc;
    ngx_wasm_phase_engine_phase_ctx_t   pctx;

    if (ngx_http_wasm_phase_ctx(&pctx, r, NGX_HTTP_REWRITE_PHASE) != NGX_OK) {
        return NGX_ERROR;
    }

    rc = ngx_wasm_phase_engine_exec(&pctx);
    if (rc != NGX_OK) {
        return rc;
    }

    return NGX_DECLINED;
}


static ngx_int_t
ngx_http_wasm_content_handler(ngx_http_request_t *r)
{
    ngx_int_t                          rc;
    ngx_wasm_phase_engine_phase_ctx_t   pctx;

    if (ngx_http_wasm_phase_ctx(&pctx, r, NGX_HTTP_CONTENT_PHASE) != NGX_OK) {
        return NGX_ERROR;
    }

    rc = ngx_wasm_phase_engine_exec(&pctx);
    if (rc != NGX_OK) {
        return rc;
    }

    /* NYI */
    //return NGX_DECLINED

    return NGX_DONE;
}


static ngx_int_t
ngx_http_wasm_log_handler(ngx_http_request_t *r)
{
    ngx_int_t                          rc;
    ngx_wasm_phase_engine_phase_ctx_t   pctx;

    if (ngx_http_wasm_phase_ctx(&pctx, r, NGX_HTTP_LOG_PHASE) != NGX_OK) {
        return NGX_ERROR;
    }

    rc = ngx_wasm_phase_engine_exec(&pctx);
    if (rc != NGX_OK) {
        return rc;
    }

    return NGX_OK;
}
