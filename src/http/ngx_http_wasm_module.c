#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_http_wasm.h>


typedef struct {
    ngx_wavm_t                        *vm;
    ngx_wasm_ops_engine_t             *ops_engine;
} ngx_http_wasm_loc_conf_t;


static void *ngx_http_wasm_create_loc_conf(ngx_conf_t *cf);
static char *ngx_http_wasm_op_post_handler(ngx_conf_t *cf, void *post, void *data);
char *ngx_http_wasm_call_directive(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
char *ngx_http_wasm_proxy_wasm_directive(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
char *ngx_http_wasm_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child);
static ngx_int_t ngx_http_wasm_init(ngx_conf_t *cf);
static ngx_int_t ngx_http_wasm_rewrite_handler(ngx_http_request_t *r);
static ngx_int_t ngx_http_wasm_content_handler(ngx_http_request_t *r);
static ngx_int_t ngx_http_wasm_log_handler(ngx_http_request_t *r);



static ngx_wasm_phase_t  ngx_wasm_subsys_http[] = {

    { ngx_string("post_read"),
      NGX_HTTP_POST_READ_PHASE,
      0 },

    { ngx_string("rewrite"),
      NGX_HTTP_REWRITE_PHASE,
      (1 << NGX_HTTP_REWRITE_PHASE) },

    { ngx_string("content"),
      NGX_HTTP_CONTENT_PHASE,
      (1 << NGX_HTTP_CONTENT_PHASE) },

    { ngx_string("log"),
      NGX_HTTP_LOG_PHASE,
      (1 << NGX_HTTP_LOG_PHASE) },

    { ngx_null_string, 0, 0 }
};


static ngx_wasm_subsystem_t  ngx_http_wasm_subsystem = {
    NGX_HTTP_LOG_PHASE + 1,
    ngx_wasm_subsys_http,
};


static ngx_conf_post_t  ngx_http_wasm_op_post =
    { ngx_http_wasm_op_post_handler };


static ngx_command_t  ngx_http_wasm_module_cmds[] = {

    { ngx_string("wasm_call"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE3,
      ngx_http_wasm_call_directive,
      NGX_HTTP_LOC_CONF_OFFSET,
      NGX_HTTP_MODULE,
      &ngx_http_wasm_op_post },

    { ngx_string("proxy_wasm"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_http_wasm_proxy_wasm_directive,
      NGX_HTTP_LOC_CONF_OFFSET,
      NGX_HTTP_MODULE,
      &ngx_http_wasm_op_post },

      ngx_null_command
};


static ngx_http_module_t  ngx_http_wasm_module_ctx = {
    NULL,                                /* preconfiguration */
    ngx_http_wasm_init,                  /* postconfiguration */
    NULL,                                /* create main configuration */
    NULL,                                /* init main configuration */
    NULL,                                /* create server configuration */
    NULL,                                /* merge server configuration */
    ngx_http_wasm_create_loc_conf,       /* create location configuration */
    ngx_http_wasm_merge_loc_conf         /* merge location configuration */
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


static void *
ngx_http_wasm_create_loc_conf(ngx_conf_t *cf)
{
    ngx_http_wasm_loc_conf_t  *loc;

    loc = ngx_pcalloc(cf->pool, sizeof(ngx_http_wasm_loc_conf_t));
    if (loc == NULL) {
        return NULL;
    }

    loc->vm = ngx_wasm_core_get_vm(cf->cycle);
    if (loc->vm) {
        loc->ops_engine = ngx_wasm_ops_engine_new(cf->pool, loc->vm,
                                                  &ngx_http_wasm_subsystem);
        if (loc->ops_engine == NULL) {
            return NGX_CONF_ERROR;
        }
    }

    return loc;
}


static char *
ngx_http_wasm_op_post_handler(ngx_conf_t *cf, void *post, void *data)
{
    ngx_wasm_op_t             *op = data;
    ngx_http_core_loc_conf_t  *clcf;

    if (op->on_phases & NGX_HTTP_CONTENT_PHASE) {
        clcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);
        if (clcf == NULL) {
            return NGX_CONF_ERROR;
        }

        clcf->handler = ngx_http_wasm_content_handler;
    }

    return NGX_CONF_OK;
}


char *
ngx_http_wasm_call_directive(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_str_t                 *value;
    ngx_http_wasm_loc_conf_t  *loc = conf;
    ngx_wasm_op_t             *op;
    ngx_conf_post_t           *post;

    if (loc->ops_engine == NULL) {
        return NGX_WASM_CONF_ERR_NO_WASM;
    }

    value = cf->args->elts;

    op = ngx_wasm_conf_add_op_call(cf, loc->ops_engine, value[1], value[2],
                                   value[3]);
    if (op == NULL) {
        return NGX_CONF_ERROR;
    }

    if (cmd->post) {
        post = cmd->post;
        return post->post_handler(cf, post, op);
    }

    return NGX_CONF_OK;
}


char *
ngx_http_wasm_proxy_wasm_directive(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf)
{
    ngx_str_t                 *value;
    ngx_http_wasm_loc_conf_t  *loc = conf;
    ngx_wasm_op_t             *op;
    ngx_conf_post_t           *post;

    if (loc->ops_engine == NULL) {
        return NGX_WASM_CONF_ERR_NO_WASM;
    }

    value = cf->args->elts;

    op = ngx_wasm_conf_add_op_proxy_wasm(cf, loc->ops_engine, value[1]);
    if (op == NULL) {
        return NGX_CONF_ERROR;
    }

    if (cmd->post) {
        post = cmd->post;
        return post->post_handler(cf, post, op);
    }

    return NGX_CONF_OK;
}


char *
ngx_http_wasm_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child)
{
    size_t                     i;
    ngx_http_wasm_loc_conf_t  *prev = parent;
    ngx_http_wasm_loc_conf_t  *conf = child;

    if (conf->ops_engine) {
        for (i = 0; i < conf->ops_engine->subsystem->nphases; i++) {
            if (conf->ops_engine->pipelines[i] == NULL) {
                conf->ops_engine->pipelines[i] = prev->ops_engine->pipelines[i];
            }
        }
    }

    return NGX_CONF_OK;
}


static ngx_int_t
ngx_http_wasm_init(ngx_conf_t *cf)
{
    size_t                      i;
    ngx_http_handler_pt        *h;
    ngx_http_core_main_conf_t  *cmcf;

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


static void
ngx_http_wasm_cleanup(void *data)
{
    ngx_wavm_ctx_t  *ctx = data;

    ngx_wavm_ctx_destroy(ctx);
}


static ngx_int_t
ngx_http_wasm_rctx(ngx_http_request_t *r, ngx_http_wasm_req_ctx_t **out)
{
    ngx_http_wasm_loc_conf_t  *loc;
    ngx_http_wasm_req_ctx_t   *rctx;
    ngx_wasm_op_ctx_t         *opctx;
    ngx_pool_cleanup_t        *cln;

    rctx = ngx_http_get_module_ctx(r, ngx_http_wasm_module);
    if (rctx == NULL) {
        loc = ngx_http_get_module_loc_conf(r, ngx_http_wasm_module);
        if (loc->vm == NULL) {
            return NGX_DECLINED;
        }

        rctx = ngx_pcalloc(r->pool, sizeof(ngx_http_wasm_req_ctx_t));
        if (rctx == NULL) {
            return NGX_ERROR;
        }

        rctx->r = r;

        opctx = &rctx->opctx;
        opctx->ops_engine = loc->ops_engine;
        opctx->wv_ctx.pool = r->pool;
        opctx->wv_ctx.log = r->connection->log;
        opctx->wv_ctx.data = rctx;
        opctx->m = &ngx_http_wasm_hfuncs_module;

        if (ngx_wavm_ctx_init(loc->vm, &opctx->wv_ctx) != NGX_OK) {
            return NGX_ERROR;
        }

        ngx_http_set_ctx(r, rctx, ngx_http_wasm_module);

        cln = ngx_pool_cleanup_add(r->pool, 0);
        if (cln == NULL) {
            return NGX_ERROR;
        }

        cln->handler = ngx_http_wasm_cleanup;
        cln->data = &opctx->wv_ctx;
    }

    *out = rctx;

    return NGX_OK;
}


static ngx_int_t
ngx_http_wasm_rewrite_handler(ngx_http_request_t *r)
{
    ngx_int_t                 rc;
    ngx_http_wasm_req_ctx_t  *rctx;

    rc = ngx_http_wasm_rctx(r, &rctx);
    if (rc != NGX_OK) {
        return rc;
    }

    rc = ngx_wasm_ops_resume(&rctx->opctx, NGX_HTTP_REWRITE_PHASE);
    if (rc != NGX_OK) {
        return rc;
    }

    if (rctx->sent_last) {
        return NGX_OK;
    }

    return NGX_DECLINED;
}


static ngx_int_t
ngx_http_wasm_content_handler(ngx_http_request_t *r)
{
    ngx_int_t                 rc;
    ngx_http_wasm_req_ctx_t  *rctx;

    rc = ngx_http_wasm_rctx(r, &rctx);
    if (rc != NGX_OK) {
        return rc;
    }

    rc = ngx_wasm_ops_resume(&rctx->opctx, NGX_HTTP_CONTENT_PHASE);
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
    ngx_int_t                 rc;
    ngx_http_wasm_req_ctx_t  *rctx;

    rc = ngx_http_wasm_rctx(r, &rctx);
    if (rc != NGX_OK) {
        return rc;
    }

    rc = ngx_wasm_ops_resume(&rctx->opctx, NGX_HTTP_LOG_PHASE);
    if (rc != NGX_OK) {
        return rc;
    }

    return NGX_OK;
}
