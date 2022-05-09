#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_http_wasm.h>


#define NGX_HTTP_WASM_DONE_IN_LOG 0


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
static ngx_int_t ngx_http_wasm_content(ngx_http_wasm_req_ctx_t *rctx);
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

    { ngx_string("wasm_socket_connect_timeout"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_msec_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_wasm_loc_conf_t, connect_timeout),
      NULL },

    { ngx_string("wasm_socket_send_timeout"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_msec_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_wasm_loc_conf_t, send_timeout),
      NULL },

    { ngx_string("wasm_socket_read_timeout"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_msec_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_wasm_loc_conf_t, recv_timeout),
      NULL },

    { ngx_string("wasm_socket_buffer_size"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_size_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_wasm_loc_conf_t, socket_buffer_size),
      NULL },

    { ngx_string("wasm_socket_buffer_reuse"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_wasm_loc_conf_t, socket_buffer_reuse),
      NULL },

    { ngx_string("wasm_socket_large_buffers"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE2,
      ngx_conf_set_bufs_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_wasm_loc_conf_t, socket_large_buffers),
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

    ngx_proxy_wasm_store_init(&mcf->store, cf->pool);

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
    loc->connect_timeout = NGX_CONF_UNSET_MSEC;
    loc->send_timeout = NGX_CONF_UNSET_MSEC;
    loc->recv_timeout = NGX_CONF_UNSET_MSEC;
    loc->socket_buffer_size = NGX_CONF_UNSET_SIZE;
    loc->socket_buffer_reuse = NGX_CONF_UNSET;

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

    if (prev->isolation == NGX_CONF_UNSET_UINT) {
        prev->isolation = NGX_PROXY_WASM_ISOLATION_NONE;
    }

    ngx_conf_merge_uint_value(conf->isolation, prev->isolation,
                              NGX_PROXY_WASM_ISOLATION_NONE);

    ngx_conf_merge_msec_value(conf->connect_timeout,
                              prev->connect_timeout, 60000);
    ngx_conf_merge_msec_value(conf->send_timeout,
                              prev->send_timeout, 60000);
    ngx_conf_merge_msec_value(conf->recv_timeout,
                              prev->recv_timeout, 60000);

    ngx_conf_merge_size_value(conf->socket_buffer_size,
                              prev->socket_buffer_size, 1024);
    ngx_conf_merge_bufs_value(conf->socket_large_buffers,
                              prev->socket_large_buffers, 4, 8192);
#if (NGX_DEBUG)
    ngx_conf_merge_value(conf->socket_buffer_reuse,
                         prev->socket_buffer_reuse, 1);
#else
    prev->socket_buffer_reuse = 1;
    conf->socket_buffer_reuse = 1;
#endif

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

    ngx_proxy_wasm_store_destroy(&mcf->store);
}


/* phases */


static void
ngx_http_wasm_cleanup(void *data)
{
    ngx_http_wasm_req_ctx_t   *rctx = data;
    ngx_wasm_op_ctx_t         *opctx = &rctx->opctx;
#if (NGX_HTTP_WASM_DONE_IN_LOG)
    ngx_http_core_loc_conf_t  *clcf;
#endif
#if (NGX_DEBUG)
    ngx_http_request_t        *r = rctx->r;
#endif

#if (NGX_HTTP_WASM_DONE_IN_LOG)
    clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);

    if (r != r->main && !clcf->log_subrequest) {
#endif
        ngx_log_debug3(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                       "wasm cleaning up request pool (stream id: %l, "
                       "r: %p, main: %d)", r->connection->number,
                       r, r->main == r);

        (void) ngx_wasm_ops_resume(opctx, NGX_WASM_DONE_PHASE);
#if (NGX_HTTP_WASM_DONE_IN_LOG)
    }
#endif
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
        rctx->pool = r->pool;
        rctx->connection = r->connection;
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
    ngx_int_t            n = 0;
    ngx_http_request_t  *r = rctx->r;

    if (rc >= NGX_HTTP_SPECIAL_RESPONSE) {
        return rc;
    }

    if (rc == NGX_AGAIN) {
        r->main->count++;

        rctx->nyields++;
        rctx->yield = 1;

        ngx_log_debug4(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                       "wasm yielding "
                       "(main: %d, count: %l, uri: \"%V\", rctx: %p, nyields: %ld)",
                       r->main == r, r->main->count, &r->uri, rctx);

        return NGX_DONE;
    }

    if (rctx->resp_content_sent) {

        rctx->resp_content_produced = 1;

        rc = r == r->main ? NGX_DONE : NGX_OK;

        if (!rctx->resp_finalized) {
            rctx->resp_finalized = 1;

            if (r->write_event_handler == ngx_http_wasm_content_wev_handler
                && rctx->nyields)
            {
                n += rctx->nyields;

            } else if (rctx->resp_content_sent) {
                n++;

            }

            if (!rctx->entered_content_phase) {
                n++;
            }

            ngx_log_debug5(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                           "wasm finalizing request "
                           "(main: %d, count: %l, uri: \"%V\","
                           " rctx: %p, n: %d)",
                           r->main == r, r->main->count, &r->uri, rctx, n);

            while (n-- > 0) {
                if (n == 0) {
                    ngx_http_finalize_request(r, NGX_DONE);

                } else {
                    ngx_http_finalize_request(r, rc);
                }
            }
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
        goto done;
    }

    rc = ngx_wasm_ops_resume(&rctx->opctx, NGX_HTTP_REWRITE_PHASE);
    if (rc == NGX_ERROR || rc >= NGX_HTTP_SPECIAL_RESPONSE) {
        r->content_handler = rctx->r_content_handler;
        rc = NGX_HTTP_INTERNAL_SERVER_ERROR;
        goto done;
    }

    ngx_wasm_assert(rc == NGX_OK
                    || rc == NGX_DONE
                    || rc == NGX_AGAIN
                    || rc == NGX_DECLINED);

    /* delegate to content phase */

    rc = NGX_DECLINED;

done:

#if 0
    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "wasm \"rewrite\" phase rc: %d", rc);
#endif

    return rc;
}


#if (NGX_DEBUG)
static ngx_int_t
ngx_http_wasm_preaccess_handler(ngx_http_request_t *r)
{
    ngx_int_t                 rc;
    ngx_http_wasm_req_ctx_t  *rctx;

    if (ngx_http_wasm_rctx(r, &rctx) != NGX_OK) {
        return NGX_ERROR;
    }

    rc = ngx_wasm_ops_resume(&rctx->opctx, NGX_HTTP_PREACCESS_PHASE);
    rc = ngx_http_wasm_check_finalize(rctx, rc);

#if 0
    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "wasm \"preaccess\" phase rc: %d", rc);
#endif

    return rc;
}


static ngx_int_t
ngx_http_wasm_access_handler(ngx_http_request_t *r)
{
    ngx_int_t                 rc;
    ngx_http_wasm_req_ctx_t  *rctx;

    if (ngx_http_wasm_rctx(r, &rctx) != NGX_OK) {
        return NGX_ERROR;
    }

    rc = ngx_wasm_ops_resume(&rctx->opctx, NGX_HTTP_ACCESS_PHASE);
    rc = ngx_http_wasm_check_finalize(rctx, rc);

#if 0
    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "wasm \"access\" phase rc: %d", rc);
#endif

    return rc;
}
#endif


static ngx_int_t
ngx_http_wasm_content(ngx_http_wasm_req_ctx_t *rctx)
{
    ngx_int_t            rc;
    ngx_http_request_t  *r = rctx->r;

    dd("enter");

    if (rctx->yield) {
        dd("resuming");
        goto resume;
    }

    rc = ngx_http_wasm_flush_local_response(rctx);
    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "wasm flush_local_response pre-ops rc: %d", rc);

    switch (rc) {
    case NGX_ERROR:
        rc = NGX_HTTP_INTERNAL_SERVER_ERROR;
        goto done;
    case NGX_OK:
        rc = ngx_http_wasm_check_finalize(rctx, rc);
        goto done;
    default:
        break;
    }

resume:

    rctx->yield = 0;

    rc = ngx_wasm_ops_resume(&rctx->opctx, NGX_HTTP_CONTENT_PHASE);
    dd("content ops resume rc: %ld", rc);
    rc = ngx_http_wasm_check_finalize(rctx, rc);
    if (rc == NGX_ERROR || rc == NGX_DONE) {
        goto done;
    }

    rc = ngx_http_wasm_flush_local_response(rctx);

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "wasm flush_local_response post-ops rc: %d", rc);

    switch (rc) {

    case NGX_ERROR:
        rc = NGX_HTTP_INTERNAL_SERVER_ERROR;
        goto done;

    case NGX_OK:
        /* fallthrough */

    case NGX_DECLINED:
        if (rctx->r_content_handler && !rctx->resp_content_produced) {
            ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                           "wasm running orig \"content\" handler");

            rctx->resp_content_produced = 1;

            rc = rctx->r_content_handler(r);

        } else if (rctx->resp_content_produced) {
            rc = NGX_OK;

        } else {
            rc = NGX_DECLINED;
        }

        break;

    default:
        ngx_wasm_log_error(NGX_LOG_WASM_NYI, r->connection->log, 0,
                           "NYI - \"content\" phase rc: %l", rc);

        rc = NGX_HTTP_INTERNAL_SERVER_ERROR;
        break;

    }

    rc = ngx_http_wasm_check_finalize(rctx, rc);

done:

    return rc;
}


static ngx_int_t
ngx_http_wasm_content_handler(ngx_http_request_t *r)
{
    ngx_int_t                 rc;
    ngx_http_wasm_req_ctx_t  *rctx;

    dd("enter");

    if (ngx_http_wasm_rctx(r, &rctx) != NGX_OK) {
        return NGX_ERROR;
    }

    rctx->entered_content_phase = 1;

    rc = ngx_http_wasm_content(rctx);

#if 0
    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "wasm \"content\" phase rc: %d", rc);
#endif

    dd("exit rc: %ld", rc);

    return rc;
}


static ngx_int_t
ngx_http_wasm_log_handler(ngx_http_request_t *r)
{
    ngx_int_t                  rc;
    ngx_http_wasm_req_ctx_t   *rctx;

    if (ngx_http_wasm_rctx(r, &rctx) != NGX_OK) {
        return NGX_ERROR;
    }

    /* in case response was not produced by us */
    //rctx->resp_sent_last = 1;

    rc = ngx_wasm_ops_resume(&rctx->opctx, NGX_HTTP_LOG_PHASE);

#if (NGX_HTTP_WASM_DONE_IN_LOG)
    (void) ngx_wasm_ops_resume(&rctx->opctx, NGX_WASM_DONE_PHASE);
#endif

    return rc;
}


void
ngx_http_wasm_content_wev_handler(ngx_http_request_t *r)
{
    ngx_int_t                 rc;
    ngx_http_wasm_req_ctx_t  *rctx;
    ngx_connection_t         *c = NULL;

    if (ngx_http_wasm_rctx(r, &rctx) != NGX_OK) {
        return;
    }

    ngx_log_debug5(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "wasm wev handler \"%V?%V\" (main: %d, count: %d, "
                   "resp_finalized: %d)",
                   &r->uri, &r->args, r == r->main, r->main->count,
                   rctx->resp_finalized);

    rc = ngx_http_wasm_content(rctx);
    if (rc == NGX_HTTP_INTERNAL_SERVER_ERROR) {
        ngx_http_finalize_request(r, rc);
        return;
    }

    if (r != r->main) {
        c = r->connection;
    }

    ngx_http_finalize_request(r, r == r->main ? NGX_DONE : NGX_OK);

    if (c) {
        /* subrequest */

        dd("run posted requests...");

        ngx_http_run_posted_requests(c);

        dd("...ran posted requests");
    }

    dd("exit");
}


void
ngx_http_wasm_resume(ngx_http_wasm_req_ctx_t *rctx, unsigned wev)
{
    ngx_http_request_t  *r = rctx->r;

    if (wev) {
        dd("resuming request...");

        r->write_event_handler(r);

        dd("...done resuming request");
    }
}
