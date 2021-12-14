#ifndef DDEBUG
#define DDEBUG 1
#endif
#include "ddebug.h"

#include <ngx_http_wasm.h>


static void *ngx_http_wasm_create_main_conf(ngx_conf_t *cf);
static void *ngx_http_wasm_create_loc_conf(ngx_conf_t *cf);
static char *ngx_http_wasm_merge_loc_conf(ngx_conf_t *cf, void *parent,
    void *child);
static ngx_int_t ngx_http_wasm_postconfig(ngx_conf_t *cf);
static ngx_int_t ngx_http_wasm_init_process(ngx_cycle_t *cycle);
static void ngx_http_wasm_exit_process(ngx_cycle_t *cycle);
static ngx_int_t ngx_http_wasm_rewrite_handler(ngx_http_request_t *r);
#if (NGX_DEBUG)
static ngx_int_t ngx_http_wasm_preaccess_handler(ngx_http_request_t *r);
static ngx_int_t ngx_http_wasm_access_handler(ngx_http_request_t *r);
#endif
static ngx_int_t ngx_http_wasm_content_handler(ngx_http_request_t *r);
static ngx_int_t ngx_http_wasm_log_handler(ngx_http_request_t *r);


static ngx_wasm_phase_t  ngx_http_wasm_phases[] = {

    { ngx_string("post_read"),
      NGX_HTTP_POST_READ_PHASE,
      0 },

    /* server_rewrite */
    /* find_config */

    { ngx_string("rewrite"),
      NGX_HTTP_REWRITE_PHASE,
      (1 << NGX_HTTP_REWRITE_PHASE) },

    /* post_rewrite */

    { ngx_string("preaccess"),
      NGX_HTTP_PREACCESS_PHASE,
      (1 << NGX_HTTP_PREACCESS_PHASE) },

    { ngx_string("access"),
      NGX_HTTP_ACCESS_PHASE,
      (1 << NGX_HTTP_ACCESS_PHASE) },

    /* post_access */
    /* precontent */

    { ngx_string("content"),
      NGX_HTTP_CONTENT_PHASE,
      (1 << NGX_HTTP_CONTENT_PHASE) },

    { ngx_string("header_filter"),
      NGX_HTTP_WASM_HEADER_FILTER_PHASE,
      (1 << NGX_HTTP_WASM_HEADER_FILTER_PHASE) },

    { ngx_string("body_filter"),
      NGX_HTTP_WASM_BODY_FILTER_PHASE,
      (1 << NGX_HTTP_WASM_BODY_FILTER_PHASE) },

    { ngx_string("log"),
      NGX_HTTP_LOG_PHASE,
      (1 << NGX_HTTP_LOG_PHASE) },

    { ngx_string("done"),
      NGX_WASM_DONE_PHASE,
      (1 << NGX_WASM_DONE_PHASE) },

    { ngx_null_string, 0, 0 }
};


ngx_wasm_subsystem_t  ngx_http_wasm_subsystem = {
    NGX_WASM_DONE_PHASE + 1,
    ngx_http_wasm_phases,
};


static ngx_command_t  ngx_http_wasm_module_cmds[] = {

    { ngx_string("wasm_call"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE3,
      ngx_http_wasm_call_directive,
      NGX_HTTP_LOC_CONF_OFFSET,
      NGX_HTTP_MODULE,
      NULL },

    { ngx_string("proxy_wasm"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE12,
      ngx_http_wasm_proxy_wasm_directive,
      NGX_HTTP_LOC_CONF_OFFSET,
      NGX_HTTP_MODULE,
      NULL },

    { ngx_string("proxy_wasm_isolation"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE12,
      ngx_http_wasm_proxy_wasm_isolation_directive,
      NGX_HTTP_LOC_CONF_OFFSET,
      NGX_HTTP_MODULE,
      NULL },

      ngx_null_command
};


static ngx_http_module_t  ngx_http_wasm_module_ctx = {
    NULL,                                /* preconfiguration */
    ngx_http_wasm_postconfig,            /* postconfiguration */
    ngx_http_wasm_create_main_conf,      /* create main configuration */
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
    ngx_http_wasm_init_process,          /* init process */
    NULL,                                /* init thread */
    NULL,                                /* exit thread */
    ngx_http_wasm_exit_process,          /* exit process */
    NULL,                                /* exit master */
    NGX_MODULE_V1_PADDING
};


static ngx_http_handler_pt  phase_handlers[NGX_HTTP_LOG_PHASE + 1] = {
    NULL,                                /* post_read */
    NULL,                                /* server_rewrite */
    NULL,                                /* find_config */
    ngx_http_wasm_rewrite_handler,       /* rewrite */
    NULL,                                /* post_rewrite */
#if (NGX_DEBUG)
    ngx_http_wasm_preaccess_handler,     /* pre_access */
    ngx_http_wasm_access_handler,        /* access */
#else
    NULL,
    NULL,
#endif
    NULL,                                /* post_access*/
    NULL,                                /* pre_content */
    ngx_http_wasm_content_handler,       /* content */
    ngx_http_wasm_log_handler            /* log */
};


/* configuration & init */


static void *
ngx_http_wasm_create_main_conf(ngx_conf_t *cf)
{
    ngx_http_wasm_main_conf_t  *mcf;

    mcf = ngx_pcalloc(cf->pool, sizeof(ngx_http_wasm_main_conf_t));
    if (mcf == NULL) {
        return NULL;
    }

    ngx_queue_init(&mcf->ops_engines);

    return mcf;
}


static void *
ngx_http_wasm_create_loc_conf(ngx_conf_t *cf)
{
    ngx_http_wasm_loc_conf_t   *loc;

    loc = ngx_pcalloc(cf->pool, sizeof(ngx_http_wasm_loc_conf_t));
    if (loc == NULL) {
        return NULL;
    }

    loc->isolation = NGX_CONF_UNSET_UINT;

    loc->vm = ngx_wasm_main_vm(cf->cycle);
    if (loc->vm) {
        loc->ops_engine = ngx_wasm_ops_engine_new(cf->pool, loc->vm,
                                                  &ngx_http_wasm_subsystem);
        if (loc->ops_engine == NULL) {
            return NULL;
        }
    }

    return loc;
}


static char *
ngx_http_wasm_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child)
{
    size_t                      i;
    ngx_http_wasm_main_conf_t  *mcf;
    ngx_http_wasm_loc_conf_t   *prev = parent;
    ngx_http_wasm_loc_conf_t   *conf = child;

    ngx_conf_merge_ptr_value(conf->vm, prev->vm, NULL);
    ngx_conf_merge_ptr_value(conf->ops_engine, prev->ops_engine, NULL);
    ngx_conf_merge_uint_value(conf->isolation, prev->isolation,
                              NGX_PROXY_WASM_ISOLATION_UNIQUE);

    if (conf->ops_engine) {
        mcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_wasm_module);

        ngx_queue_insert_tail(&mcf->ops_engines, &conf->ops_engine->q);

        for (i = 0; i < conf->ops_engine->subsystem->nphases; i++) {
            if (conf->ops_engine->pipelines[i] == NULL) {
                conf->ops_engine->pipelines[i] = prev->ops_engine->pipelines[i];
            }
        }
    }

    return NGX_CONF_OK;
}


static ngx_int_t
ngx_http_wasm_postconfig(ngx_conf_t *cf)
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


static ngx_int_t
ngx_http_wasm_init_process(ngx_cycle_t *cycle)
{
    ngx_queue_t                *q;
    ngx_wasm_ops_engine_t      *ops_engine;
    ngx_http_wasm_main_conf_t  *mcf;

    mcf = ngx_http_cycle_get_module_main_conf(cycle, ngx_http_wasm_module);

    for (q = ngx_queue_head(&mcf->ops_engines);
         q != ngx_queue_sentinel(&mcf->ops_engines);
         q = ngx_queue_next(q))
    {
        ops_engine = ngx_queue_data(q, ngx_wasm_ops_engine_t, q);

        ngx_wasm_ops_engine_init(ops_engine);
    }

    return NGX_OK;
}


static void
ngx_http_wasm_exit_process(ngx_cycle_t *cycle)
{
    ngx_queue_t                *q;
    ngx_wasm_ops_engine_t      *ops_engine;
    ngx_http_wasm_main_conf_t  *mcf;

    mcf = ngx_http_cycle_get_module_main_conf(cycle, ngx_http_wasm_module);

    for (q = ngx_queue_head(&mcf->ops_engines);
         q != ngx_queue_sentinel(&mcf->ops_engines);
         q = ngx_queue_next(q))
    {
        ops_engine = ngx_queue_data(q, ngx_wasm_ops_engine_t, q);

        ngx_wasm_ops_engine_destroy(ops_engine);
    }

    ngx_proxy_wasm_store_free(&mcf->store);
}


/* phases */


static void
ngx_http_wasm_cleanup(void *data)
{
    ngx_http_wasm_req_ctx_t   *rctx = data;
    ngx_http_request_t        *r = rctx->r;
    ngx_wasm_op_ctx_t         *opctx = &rctx->opctx;
    ngx_http_core_loc_conf_t  *clcf;

    clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);

    if (r != r->main && !clcf->log_subrequest) {
        ngx_log_debug3(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                       "wasm cleaning up request pool (stream id: %l, "
                       "r: %p, main: %d)", r->connection->number,
                       r, r->main == r);

        (void) ngx_wasm_ops_resume(opctx, NGX_WASM_DONE_PHASE,
                                   NGX_WASM_OPS_RUN_ALL);
    }
}


ngx_int_t
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

        ngx_log_debug3(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                       "wasm rctx created: %p (r: %p, main: %d)",
                       rctx, r, r->main == r);

        rctx->r = r;
        rctx->req_keepalive = r->keepalive;

        ngx_http_set_ctx(r, rctx, ngx_http_wasm_module);

        opctx = &rctx->opctx;
        opctx->ops_engine = loc->ops_engine;
        opctx->pool = r->pool;
        opctx->log = r->connection->log;
        opctx->data = rctx;

        cln = ngx_pool_cleanup_add(r->pool, 0);
        if (cln == NULL) {
            return NGX_ERROR;
        }

        cln->handler = ngx_http_wasm_cleanup;
        cln->data = rctx;

        if (r->content_handler != ngx_http_wasm_content_handler) {
            ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                           "wasm rctx injecting ngx_http_wasm_content_handler");

            rctx->r_content_handler = r->content_handler;
            r->content_handler = ngx_http_wasm_content_handler;
        }
    }
#if (NGX_DEBUG)
    else {
        ngx_log_debug3(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                       "wasm rctx reused: %p (r: %p, main: %d)",
                       rctx, r, r->main == r);
     }
#endif

    *out = rctx;

    return NGX_OK;
}


ngx_int_t
ngx_http_wasm_check_finalize(ngx_http_wasm_req_ctx_t *rctx, ngx_int_t rc)
{
    ngx_http_request_t  *r = rctx->r;

    if (rc == NGX_ERROR) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    if (rctx->resp_sent_last) {
        ngx_log_debug2(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                       "wasm finalizing (rc: %d, resp_finalized: %d)",
                       rc, rctx->resp_finalized);

        rc = NGX_DONE;

        if (!rctx->resp_finalized) {
            rctx->resp_finalized = 1;
            r->main->count++;
            ngx_http_finalize_request(r, rc);
        }
    }

    return rc;
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

    rc = ngx_wasm_ops_resume(&rctx->opctx, NGX_HTTP_REWRITE_PHASE, 0);

    if (rctx->resp_sent_last) {
        rc = NGX_OK;

    } else if (rc == NGX_ERROR) {
        rc = NGX_HTTP_INTERNAL_SERVER_ERROR;
        r->content_handler = rctx->r_content_handler;
    }

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "wasm \"rewrite\" phase rc: %d", rc);

    return rc;
}


#if (NGX_DEBUG)
static ngx_int_t
ngx_http_wasm_preaccess_handler(ngx_http_request_t *r)
{
    ngx_int_t                 rc;
    ngx_http_wasm_req_ctx_t  *rctx;

    rc = ngx_http_wasm_rctx(r, &rctx);
    if (rc != NGX_OK) {
        return rc;
    }

    rc = ngx_wasm_ops_resume(&rctx->opctx, NGX_HTTP_PREACCESS_PHASE, 0);
    rc = ngx_http_wasm_check_finalize(rctx, rc);

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "wasm \"preaccess\" phase rc: %d", rc);

    return rc;
}


static ngx_int_t
ngx_http_wasm_access_handler(ngx_http_request_t *r)
{
    ngx_int_t                 rc;
    ngx_http_wasm_req_ctx_t  *rctx;

    rc = ngx_http_wasm_rctx(r, &rctx);
    if (rc != NGX_OK) {
        return rc;
    }

    rc = ngx_wasm_ops_resume(&rctx->opctx, NGX_HTTP_ACCESS_PHASE, 0);
    rc = ngx_http_wasm_check_finalize(rctx, rc);

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "wasm \"access\" phase rc: %d", rc);

    return rc;
}
#endif


static ngx_int_t
ngx_http_wasm_content_handler(ngx_http_request_t *r)
{
    ngx_int_t                  rc;
    ngx_http_wasm_req_ctx_t   *rctx;

    rc = ngx_http_wasm_rctx(r, &rctx);
    if (rc != NGX_OK) {
        goto done;
    }

    rc = ngx_wasm_ops_resume(&rctx->opctx, NGX_HTTP_CONTENT_PHASE, 0);
    rc = ngx_http_wasm_check_finalize(rctx, rc);
    if (rc == NGX_HTTP_INTERNAL_SERVER_ERROR
        || rc == NGX_DONE
        || rc == NGX_AGAIN)
    {
        ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                       "wasm \"content\" phase: content already produced");
        goto done;
    }

    ngx_wasm_assert(rc == NGX_OK || rc == NGX_DECLINED);

    rc = ngx_http_wasm_flush_local_response(rctx);

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "wasm flush_local_response rc: %d", rc);

    switch (rc) {

    case NGX_OK:
        /* flushed response */
        rc = ngx_http_wasm_check_finalize(rctx, rc);

        ngx_wasm_assert(rc == NGX_DONE);

        if (r != r->main) {
            /* subrequest */
            rc = NGX_OK;
        }

        break;

    case NGX_DECLINED:
        if (rctx->r_content_handler) {
            ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                           "wasm running orig \"content\" handler");

            rctx->resp_content_chosen = 1;

            rc = rctx->r_content_handler(r);
        }

        break;

    default:
        break;

    }

done:

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "wasm \"content\" phase rc: %d", rc);

    return rc;
}


static ngx_int_t
ngx_http_wasm_log_handler(ngx_http_request_t *r)
{
    ngx_int_t                  rc;
    ngx_http_wasm_req_ctx_t   *rctx;

    rc = ngx_http_wasm_rctx(r, &rctx);
    if (rc != NGX_OK) {
        return rc;
    }

    /* in case response was not produced by us */
    rctx->resp_sent_last = 1;

    rc = ngx_wasm_ops_resume(&rctx->opctx, NGX_HTTP_LOG_PHASE,
                             NGX_WASM_OPS_RUN_ALL);

    (void) ngx_wasm_ops_resume(&rctx->opctx, NGX_WASM_DONE_PHASE,
                               NGX_WASM_OPS_RUN_ALL);

    return rc;
}
