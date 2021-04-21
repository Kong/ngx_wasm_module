#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_http_wasm.h>
#include <ngx_http_proxy_wasm.h>


typedef struct {
    ngx_wavm_t                        *vm;
    ngx_wasm_ops_engine_t             *ops_engine;
    ngx_proxy_wasm_t                  *pwmodule;
    ngx_queue_t                        q;
} ngx_http_wasm_loc_conf_t;


typedef struct {
    ngx_queue_t                        ops_engines;
} ngx_http_wasm_main_conf_t;


static void *ngx_http_wasm_create_main_conf(ngx_conf_t *cf);
static char *ngx_http_wasm_init_main_conf(ngx_conf_t *cf, void *conf);
static void *ngx_http_wasm_create_loc_conf(ngx_conf_t *cf);
char *ngx_http_wasm_call_directive(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
char *ngx_http_wasm_proxy_wasm_directive(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
char *ngx_http_wasm_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child);
static ngx_int_t ngx_http_wasm_postconfig(ngx_conf_t *cf);
static ngx_int_t ngx_http_wasm_init(ngx_cycle_t *cycle);
static ngx_int_t ngx_http_wasm_init_process(ngx_cycle_t *cycle);
static void ngx_http_wasm_exit_process(ngx_cycle_t *cycle);
static ngx_int_t ngx_http_wasm_rewrite_handler(ngx_http_request_t *r);
#if (NGX_DEBUG)
static ngx_int_t ngx_http_wasm_preaccess_handler(ngx_http_request_t *r);
static ngx_int_t ngx_http_wasm_access_handler(ngx_http_request_t *r);
#endif
static ngx_int_t ngx_http_wasm_content_handler(ngx_http_request_t *r);
static ngx_int_t ngx_http_wasm_header_filter_handler(ngx_http_request_t *r);
static ngx_int_t ngx_http_wasm_log_handler(ngx_http_request_t *r);


static ngx_wasm_phase_t  ngx_wasm_subsys_http[] = {

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

    { ngx_string("log"),
      NGX_HTTP_LOG_PHASE,
      (1 << NGX_HTTP_LOG_PHASE) },

    { ngx_null_string, 0, 0 }
};


static ngx_wasm_subsystem_t  ngx_http_wasm_subsystem = {
    NGX_HTTP_LOG_PHASE + 1 + 1,    /* nphases (+1: 0-based, +1: header_filter) */
    ngx_wasm_subsys_http,          /* phases */
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

      ngx_null_command
};


static ngx_http_module_t  ngx_http_wasm_module_ctx = {
    NULL,                                /* preconfiguration */
    ngx_http_wasm_postconfig,            /* postconfiguration */
    ngx_http_wasm_create_main_conf,      /* create main configuration */
    ngx_http_wasm_init_main_conf,        /* init main configuration */
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
    ngx_http_wasm_init,                  /* init module */
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


static ngx_http_output_header_filter_pt  ngx_http_next_header_filter;


/* configuration & init */


static void *
ngx_http_wasm_create_main_conf(ngx_conf_t *cf)
{
    ngx_http_wasm_main_conf_t  *mcf;

    mcf = ngx_pcalloc(cf->pool, sizeof(ngx_http_wasm_main_conf_t));
    if (mcf == NULL) {
        return NULL;
    }

    return mcf;
}


static char *
ngx_http_wasm_init_main_conf(ngx_conf_t *cf, void *conf)
{
    ngx_http_wasm_main_conf_t  *mcf = conf;

    ngx_queue_init(&mcf->ops_engines);

    return NGX_CONF_OK;
}


static void *
ngx_http_wasm_create_loc_conf(ngx_conf_t *cf)
{
    ngx_http_wasm_loc_conf_t   *loc;

    loc = ngx_pcalloc(cf->pool, sizeof(ngx_http_wasm_loc_conf_t));
    if (loc == NULL) {
        return NULL;
    }

    loc->vm = ngx_wasm_main_vm(cf->cycle);
    if (loc->vm) {
        loc->ops_engine = ngx_wasm_ops_engine_new(cf->pool, loc->vm,
                                                  &ngx_http_wasm_subsystem);
        if (loc->ops_engine == NULL) {
            return NGX_CONF_ERROR;
        }
    }

    return loc;
}


char *
ngx_http_wasm_call_directive(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_str_t                 *values, *phase_name, *module_name, *func_name;
    ngx_wasm_op_t             *op;
    ngx_http_wasm_loc_conf_t  *loc = conf;
    ngx_wasm_phase_t          *phase = ngx_wasm_subsys_http;

    if (loc->ops_engine == NULL) {
        return NGX_WASM_CONF_ERR_NO_WASM;
    }

    /* args */

    values = cf->args->elts;

    phase_name = &values[1];
    module_name = &values[2];
    func_name = &values[3];

    if (phase_name->len == 0) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "invalid phase \"%V\"",
                           phase_name);
        return NGX_CONF_ERROR;
    }

    if (func_name->len == 0) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "invalid function name \"%V\"",
                           func_name);
        return NGX_CONF_ERROR;
    }

    if (module_name->len == 0) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "invalid module name \"%V\"",
                           module_name);
        return NGX_CONF_ERROR;
    }

    for (/* void */; phase->name.len; phase++) {
        if (ngx_strncmp(phase_name->data, phase->name.data, phase->name.len)
            == 0)
        {
            break;
        }
    }

    if (phase->name.len == 0) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "unknown phase \"%V\"",
                           phase_name);
        return NGX_CONF_ERROR;
    }

    if (phase->on == 0) {
        ngx_conf_log_error(NGX_LOG_WASM_NYI, cf, 0, "unsupported phase \"%V\"",
                           phase_name);
        return NGX_CONF_ERROR;
    }

    /* op */

    op = ngx_pcalloc(cf->pool, sizeof(ngx_wasm_op_t));
    if (op == NGX_CONF_ERROR) {
        return NGX_CONF_ERROR;
    }

    op->module = ngx_wavm_module_lookup(loc->ops_engine->vm, module_name);
    if (op->module == NULL) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "no \"%V\" module defined", module_name);
        return NGX_CONF_ERROR;
    }

    op->code = NGX_WASM_OP_CALL;
    op->on_phases = phase->on;
    op->host = &ngx_http_wasm_host_interface;

    if (ngx_wasm_ops_add(loc->ops_engine, op) != NGX_OK) {
        return NGX_CONF_ERROR;
    }

    op->conf.call.func_name = *func_name;

    return NGX_CONF_OK;
}


char *
ngx_http_wasm_proxy_wasm_directive(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf)
{
    u_char                    *p;
    ngx_str_t                 *values, *module_name, *filter_config;
    ngx_wasm_op_t             *op;
    ngx_proxy_wasm_t          *pwm;
    ngx_http_wasm_loc_conf_t  *loc = conf;

    if (loc->ops_engine == NULL) {
        return NGX_WASM_CONF_ERR_NO_WASM;
    }

    if (loc->pwmodule) {
        return NGX_WASM_CONF_ERR_DUPLICATE;
    }

    /* args */

    values = cf->args->elts;

    module_name = &values[1];

    if (module_name->len == 0) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "invalid module name \"%V\"", module_name);
        return NGX_CONF_ERROR;
    }

    /* filter alloc */

    pwm = ngx_pcalloc(cf->pool, sizeof(ngx_proxy_wasm_t));
    if (pwm == NULL) {
        return NGX_CONF_ERROR;
    }

    pwm->pool = cf->pool;
    pwm->log = &cf->cycle->new_log;

    if (cf->args->nelts > 2) {
        filter_config = &values[2];

        pwm->filter_config.len = filter_config->len;
        pwm->filter_config.data = ngx_palloc(pwm->pool,
                                             pwm->filter_config.len + 1);
        if (pwm->filter_config.data == NULL) {
            return NGX_CONF_ERROR;
        }

        p = ngx_copy(pwm->filter_config.data, filter_config->data,
                     pwm->filter_config.len);
        *p = '\0';
    }

    /* filter init */

    pwm->resume_ = ngx_http_proxy_wasm_resume;
    pwm->ctxid_ = ngx_http_proxy_wasm_ctxid;
    pwm->ecode_ = ngx_http_proxy_wasm_ecode;
    pwm->max_pairs = NGX_HTTP_WASM_MAX_REQ_HEADERS;

    pwm->module = ngx_wavm_module_lookup(loc->ops_engine->vm, module_name);
    if (pwm->module == NULL) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "no \"%V\" module defined", module_name);
        return NGX_CONF_ERROR;
    }

    /* op */

    op = ngx_pcalloc(cf->pool, sizeof(ngx_wasm_op_t));
    if (op == NULL) {
        return NGX_CONF_ERROR;
    }

    op->module = pwm->module;
    op->code = NGX_WASM_OP_PROXY_WASM;
    op->host = &ngx_proxy_wasm_host;
    op->on_phases = (1 << NGX_HTTP_REWRITE_PHASE)
                    | (1 << NGX_HTTP_WASM_HEADER_FILTER_PHASE)
                    | (1 << NGX_HTTP_LOG_PHASE);

    if (ngx_wasm_ops_add(loc->ops_engine, op) != NGX_OK) {
        return NGX_CONF_ERROR;
    }

    op->conf.proxy_wasm.pwmodule = pwm;
    op->conf.proxy_wasm.pwmodule->lmodule = op->lmodule;

    loc->pwmodule = pwm;

    return NGX_CONF_OK;
}


char *
ngx_http_wasm_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child)
{
    size_t                      i;
    ngx_http_wasm_loc_conf_t   *prev = parent;
    ngx_http_wasm_loc_conf_t   *conf = child;
    ngx_http_wasm_main_conf_t  *mcf;

    if (conf->ops_engine) {
        mcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_wasm_module);

        ngx_queue_insert_tail(&mcf->ops_engines, &conf->q);

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
ngx_http_wasm_init(ngx_cycle_t *cycle)
{
    ngx_http_next_header_filter = ngx_http_top_header_filter;
    ngx_http_top_header_filter = ngx_http_wasm_header_filter_handler;

    return NGX_OK;
}


static ngx_int_t
ngx_http_wasm_init_process(ngx_cycle_t *cycle)
{
    ngx_queue_t                *q;
    ngx_http_wasm_main_conf_t  *mcf;
    ngx_http_wasm_loc_conf_t   *loc;

    mcf = ngx_http_cycle_get_module_main_conf(cycle, ngx_http_wasm_module);

    for (q = ngx_queue_head(&mcf->ops_engines);
         q != ngx_queue_sentinel(&mcf->ops_engines);
         q = ngx_queue_next(q))
    {
        loc = ngx_queue_data(q, ngx_http_wasm_loc_conf_t, q);

        ngx_wasm_ops_engine_init(loc->ops_engine);
    }

    return NGX_OK;
}


static void
ngx_http_wasm_exit_process(ngx_cycle_t *cycle)
{
    ngx_queue_t                *q;
    ngx_http_wasm_main_conf_t  *mcf;
    ngx_http_wasm_loc_conf_t   *loc;

    mcf = ngx_http_cycle_get_module_main_conf(cycle, ngx_http_wasm_module);

    for (q = ngx_queue_head(&mcf->ops_engines);
         q != ngx_queue_sentinel(&mcf->ops_engines);
         q = ngx_queue_next(q))
    {
        loc = ngx_queue_data(q, ngx_http_wasm_loc_conf_t, q);

        ngx_wasm_ops_engine_destroy(loc->ops_engine);
    }
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

        opctx->log = r->connection->log;
        opctx->ops_engine = loc->ops_engine;

        opctx->wvctx.pool = r->pool;
        opctx->wvctx.log = r->connection->log;
        opctx->wvctx.data = rctx;

        if (ngx_wavm_ctx_init(loc->vm, &opctx->wvctx) != NGX_OK) {
            return NGX_ERROR;
        }

        ngx_http_set_ctx(r, rctx, ngx_http_wasm_module);

        cln = ngx_pool_cleanup_add(r->pool, 0);
        if (cln == NULL) {
            return NGX_ERROR;
        }

        cln->handler = ngx_http_wasm_cleanup;
        cln->data = &opctx->wvctx;

        if (r->content_handler != ngx_http_wasm_content_handler) {
            ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                           "wasm rctx injecting ngx_http_wasm_content_handler");

            rctx->r_content_handler = r->content_handler;
            r->content_handler = ngx_http_wasm_content_handler;
        }
    }

    *out = rctx;

    return NGX_OK;
}


static ngx_int_t
ngx_http_wasm_check_finalize(ngx_http_request_t *r,
    ngx_http_wasm_req_ctx_t *rctx, ngx_int_t rc)
{
    if (rc == NGX_ERROR) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    if (rctx->sent_last) {
        rc = NGX_DONE;

        if (!rctx->finalized) {
            rctx->finalized = 1;
            r->main->count++;
            ngx_http_finalize_request(r, rc);
        }

        ngx_log_debug2(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                       "wasm rctx->sent_last: %d, rc: %d",
                       rctx->sent_last, rc);
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

    rc = ngx_wasm_ops_resume(&rctx->opctx, NGX_HTTP_REWRITE_PHASE);

    if (rctx->sent_last) {
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

    rc = ngx_wasm_ops_resume(&rctx->opctx, NGX_HTTP_PREACCESS_PHASE);

    return ngx_http_wasm_check_finalize(r, rctx, rc);
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

    rc = ngx_wasm_ops_resume(&rctx->opctx, NGX_HTTP_ACCESS_PHASE);

    return ngx_http_wasm_check_finalize(r, rctx, rc);
}
#endif


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
    rc = ngx_http_wasm_check_finalize(r, rctx, rc);
    if (rc == NGX_HTTP_INTERNAL_SERVER_ERROR || rc == NGX_DONE) {
        return rc;
    }

    ngx_wasm_assert(rc == NGX_OK || rc == NGX_DECLINED);

    rc = ngx_http_wasm_flush_local_response(r, rctx);

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "wasm flush_local_response rc: %d", rc);

    rctx->entered_content = 1;

    switch (rc) {

    case NGX_OK:
        /* flushed response */
        rc = ngx_http_wasm_check_finalize(r, rctx, rc);
        ngx_wasm_assert(rc == NGX_DONE);

        if (r != r->main) {
            /* subrequests */
            rc = NGX_OK;
        }

        break;

    case NGX_DECLINED:
        if (rctx->r_content_handler) {
            ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                           "wasm running orig \"content\" handler");

            rc = rctx->r_content_handler(r);
        }

        break;

    case NGX_AGAIN: /* TODO: catch */
    case NGX_ERROR:
        break;

    default:
        break;

    }

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "wasm \"content\" phase rc: %d", rc);

    return rc;
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

    rc = ngx_wasm_ops_resume(&rctx->opctx, NGX_HTTP_WASM_HEADER_FILTER_PHASE);
    if (rc == NGX_OK) {
        rc = ngx_http_wasm_check_finalize(r, rctx, rc);
        if (rc == NGX_DONE) {
            return NGX_DONE;
        }
    }

next_filter:

    return ngx_http_next_header_filter(r);
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
