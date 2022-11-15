#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_http_wasm.h>
#if (NGX_WASM_LUA)
#include <ngx_http_lua_util.h>
#endif


#define NGX_HTTP_WASM_DONE_IN_LOG 0


static void *ngx_http_wasm_create_main_conf(ngx_conf_t *cf);
static char *ngx_http_wasm_init_main_conf(ngx_conf_t *cf, void *conf);
static void *ngx_http_wasm_create_loc_conf(ngx_conf_t *cf);
static char *ngx_http_wasm_merge_loc_conf(ngx_conf_t *cf, void *parent,
    void *child);
static ngx_int_t ngx_http_wasm_postconfig(ngx_conf_t *cf);
static ngx_int_t ngx_http_wasm_init_process(ngx_cycle_t *cycle);
static void ngx_http_wasm_exit_process(ngx_cycle_t *cycle);
static ngx_int_t ngx_http_wasm_rewrite_handler(ngx_http_request_t *r);
#if (NGX_DEBUG)
static ngx_int_t ngx_http_wasm_preaccess_handler(ngx_http_request_t *r);
#endif
static ngx_int_t ngx_http_wasm_access_handler(ngx_http_request_t *r);
static ngx_int_t ngx_http_wasm_content(ngx_http_wasm_req_ctx_t *rctx);
static ngx_int_t ngx_http_wasm_content_handler(ngx_http_request_t *r);
static ngx_int_t ngx_http_wasm_log_handler(ngx_http_request_t *r);
static ngx_int_t ngx_http_wasm_check_finalize(ngx_http_wasm_req_ctx_t *rctx,
    ngx_int_t rc);


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

    { ngx_string("trailer_filter"),
      NGX_HTTP_WASM_TRAILER_FILTER_PHASE,
      (1 << NGX_HTTP_WASM_TRAILER_FILTER_PHASE) },

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
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_http_wasm_proxy_wasm_isolation_directive,
      NGX_HTTP_LOC_CONF_OFFSET,
      NGX_HTTP_MODULE,
      NULL },

    { ngx_string("resolver_add"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE2,
      ngx_http_wasm_resolver_add_directive,
      NGX_HTTP_LOC_CONF_OFFSET,
      NGX_HTTP_MODULE,
      NULL },

    { ngx_string("proxy_wasm_request_headers_in_access"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_wasm_loc_conf_t, pwm_req_headers_in_access),
      NULL },

    { ngx_string("proxy_wasm_lua_resolver"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_wasm_loc_conf_t, pwm_lua_resolver),
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
#else
    NULL,
#endif
    ngx_http_wasm_access_handler,        /* access */
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

    mcf->vm = ngx_wasm_main_vm(cf->cycle);

    ngx_queue_init(&mcf->plans);

    return mcf;
}


static char *
ngx_http_wasm_init_main_conf(ngx_conf_t *cf, void *conf)
{
    ngx_http_wasm_main_conf_t  *mcf = conf;

    mcf->ops = ngx_wasm_ops_new(cf->pool, &cf->cycle->new_log, mcf->vm,
                                &ngx_http_wasm_subsystem);
    if (mcf->ops == NULL) {
        return NGX_CONF_ERROR;
    }

    ngx_proxy_wasm_init(cf);
    ngx_proxy_wasm_store_init(&mcf->store, cf->pool);

    return NGX_CONF_OK;
}


static void *
ngx_http_wasm_create_loc_conf(ngx_conf_t *cf)
{
    ngx_http_wasm_loc_conf_t  *loc;

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
    loc->pwm_req_headers_in_access = NGX_CONF_UNSET;
    loc->pwm_lua_resolver = NGX_CONF_UNSET;

    if (ngx_wasm_main_vm(cf->cycle)) {
        loc->plan = ngx_wasm_ops_plan_new(cf->pool, &ngx_http_wasm_subsystem);
        if (loc->plan == NULL) {
            return NULL;
        }
    }

    return loc;
}


static char *
ngx_http_wasm_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child)
{
    ngx_http_wasm_main_conf_t  *mcf;
    ngx_http_wasm_loc_conf_t   *prev = parent;
    ngx_http_wasm_loc_conf_t   *conf = child;

    mcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_wasm_module);

    ngx_conf_merge_ptr_value(conf->plan, prev->plan, NULL);

    ngx_conf_merge_uint_value(conf->isolation, prev->isolation,
                              NGX_PROXY_WASM_ISOLATION_NONE);

    ngx_conf_merge_msec_value(conf->connect_timeout,
                              prev->connect_timeout,
                              NGX_WASM_DEFAULT_SOCK_CONN_TIMEOUT);
    ngx_conf_merge_msec_value(conf->send_timeout,
                              prev->send_timeout,
                              NGX_WASM_DEFAULT_SOCK_SEND_TIMEOUT);
    ngx_conf_merge_msec_value(conf->recv_timeout,
                              prev->recv_timeout,
                              NGX_WASM_DEFAULT_RECV_TIMEOUT);

    ngx_conf_merge_size_value(conf->socket_buffer_size,
                              prev->socket_buffer_size,
                              NGX_WASM_DEFAULT_SOCK_BUF_SIZE);
    ngx_conf_merge_bufs_value(conf->socket_large_buffers,
                              prev->socket_large_buffers,
                              NGX_WASM_DEFAULT_SOCK_LARGE_BUF_NUM,
                              NGX_WASM_DEFAULT_SOCK_LARGE_BUF_SIZE);
#if (NGX_DEBUG)
    ngx_conf_merge_value(conf->socket_buffer_reuse,
                         prev->socket_buffer_reuse, 1);
#else
    prev->socket_buffer_reuse = 1;
    conf->socket_buffer_reuse = 1;
#endif

    ngx_conf_merge_value(conf->pwm_req_headers_in_access,
                         prev->pwm_req_headers_in_access, 0);

    ngx_conf_merge_value(conf->pwm_lua_resolver,  prev->pwm_lua_resolver, 0);

    if (conf->plan && !conf->plan->populated) {
        conf->plan = prev->plan;
    }

    if (prev->isolation == NGX_CONF_UNSET_UINT) {
        prev->isolation = NGX_PROXY_WASM_ISOLATION_NONE;
    }

    ngx_queue_insert_tail(&mcf->plans, &conf->q);

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
    ngx_http_wasm_loc_conf_t   *loc;
    ngx_http_wasm_main_conf_t  *mcf;

    mcf = ngx_http_cycle_get_module_main_conf(cycle, ngx_http_wasm_module);

    if (!mcf->vm) {
        return NGX_OK;
    }

    for (q = ngx_queue_head(&mcf->plans);
         q != ngx_queue_sentinel(&mcf->plans);
         q = ngx_queue_next(q))
    {
        loc = ngx_queue_data(q, ngx_http_wasm_loc_conf_t, q);

        if (loc->plan) {
            /* ignore error to not exit worker process, logged later */
            if (ngx_wasm_ops_plan_load(loc->plan, &cycle->new_log) != NGX_OK) {
                return NGX_ERROR;
            }
        }
    }

    if (ngx_proxy_wasm_start(cycle) != NGX_OK) {
        return NGX_ERROR;
    }

    return NGX_OK;
}


static void
ngx_http_wasm_exit_process(ngx_cycle_t *cycle)
{
    ngx_http_wasm_main_conf_t  *mcf;

    mcf = ngx_http_cycle_get_module_main_conf(cycle, ngx_http_wasm_module);

    ngx_proxy_wasm_exit();
    ngx_proxy_wasm_store_destroy(&mcf->store);

    ngx_wasm_ops_destroy(mcf->ops);
}


/* phases */


static void
ngx_http_wasm_cleanup(void *data)
{
    ngx_http_wasm_req_ctx_t   *rctx = data;
#if (NGX_HTTP_WASM_DONE_IN_LOG)
    ngx_http_core_loc_conf_t  *clcf;
#else
    ngx_wasm_op_ctx_t         *opctx = &rctx->opctx;
#endif
#if (NGX_HTTP_WASM_DONE_IN_LOG || NGX_DEBUG)
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

#if (!NGX_HTTP_WASM_DONE_IN_LOG)
        (void) ngx_wasm_ops_resume(opctx, NGX_WASM_DONE_PHASE);
#endif
#if (NGX_HTTP_WASM_DONE_IN_LOG)
    }
#endif
}


ngx_int_t
ngx_http_wasm_rctx(ngx_http_request_t *r, ngx_http_wasm_req_ctx_t **out)
{
    ngx_wasm_op_ctx_t          *opctx;
    ngx_pool_cleanup_t         *cln;
    ngx_http_wasm_req_ctx_t    *rctx;
    ngx_http_wasm_main_conf_t  *mcf;
    ngx_http_wasm_loc_conf_t   *loc = NULL;

    rctx = ngx_http_get_module_ctx(r, ngx_http_wasm_module);
    if (rctx == NULL) {
        if (r->loc_conf) {
            loc = ngx_http_get_module_loc_conf(r, ngx_http_wasm_module);
            if (loc->plan == NULL || !loc->plan->populated) {
                return NGX_DECLINED;
            }
        }

        rctx = ngx_pcalloc(r->pool, sizeof(ngx_http_wasm_req_ctx_t));
        if (rctx == NULL) {
            return NGX_ERROR;
        }

        mcf = ngx_http_cycle_get_module_main_conf(ngx_cycle,
                                                  ngx_http_wasm_module);

        ngx_log_debug3(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                       "wasm rctx created: %p (r: %p, main: %d)",
                       rctx, r, r->main == r);

        rctx->r = r;
        rctx->pool = r->pool;
        rctx->connection = r->connection;
        rctx->req_keepalive = r->keepalive;

        if (loc) {
            rctx->sock_buffer_reuse = loc->socket_buffer_reuse;
        }

        ngx_http_set_ctx(r, rctx, ngx_http_wasm_module);

        opctx = &rctx->opctx;
        opctx->ops = mcf->ops;
        opctx->pool = r->pool;
        opctx->log = r->connection->log;
        opctx->data = rctx;

        if (loc) {
            ngx_log_debug2(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                           "wasm attaching plan to rctx (rctx: %p, plan: %p)",
                           rctx, loc->plan);

            cln = ngx_pool_cleanup_add(r->pool, 0);
            if (cln == NULL) {
                return NGX_ERROR;
            }

            cln->handler = ngx_http_wasm_cleanup;
            cln->data = rctx;

            rctx->pwm_lua_resolver = loc->pwm_lua_resolver;

            if (ngx_wasm_ops_plan_attach(loc->plan, opctx) != NGX_OK) {
                return NGX_ERROR;
            }

            if (r->content_handler != ngx_http_wasm_content_handler) {
                ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                               "wasm rctx injecting "
                               "ngx_http_wasm_content_handler");

                rctx->r_content_handler = r->content_handler;
                r->content_handler = ngx_http_wasm_content_handler;
            }

        } else {
            ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                           "wasm rctx for fake request");
            rctx->fake_request = 1;
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


static ngx_int_t
ngx_http_wasm_check_finalize(ngx_http_wasm_req_ctx_t *rctx, ngx_int_t rc)
{
    ngx_int_t            n = 0;
    ngx_http_request_t  *r = rctx->r;

    dd("enter (rc: %ld)", rc);

    if (rc >= NGX_HTTP_SPECIAL_RESPONSE) {
        return rc;
    }

    if (rc == NGX_AGAIN) {
        r->main->count++;
        return NGX_DONE;
    }

    if (rctx->resp_content_sent) {

        /* if not already set */
        rctx->resp_content_chosen = 1;

        rc = r == r->main ? NGX_DONE : NGX_OK;

        if (!rctx->resp_finalized) {
            rctx->resp_finalized = 1;

            if (rctx->resp_content_chosen) {
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

    dd("enter");

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

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, ngx_cycle->log, 0,
                   "wasm \"rewrite\" phase rc: %d", rc);

    return rc;
}


#if (NGX_DEBUG)
static ngx_int_t
ngx_http_wasm_preaccess_handler(ngx_http_request_t *r)
{
    ngx_int_t                 rc;
    ngx_http_wasm_req_ctx_t  *rctx;

    dd("enter");

    rc = ngx_http_wasm_rctx(r, &rctx);
    if (rc != NGX_OK) {
        goto done;
    }

    rc = ngx_wasm_ops_resume(&rctx->opctx, NGX_HTTP_PREACCESS_PHASE);
    rc = ngx_http_wasm_check_finalize(rctx, rc);

done:

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, ngx_cycle->log, 0,
                   "wasm \"preaccess\" phase rc: %d", rc);

    return rc;
}
#endif


static ngx_int_t
ngx_http_wasm_access_handler(ngx_http_request_t *r)
{
    ngx_int_t                 rc;
    ngx_http_wasm_req_ctx_t  *rctx;

    dd("enter");

    rc = ngx_http_wasm_rctx(r, &rctx);
    if (rc != NGX_OK) {
        goto done;
    }

    rc = ngx_wasm_ops_resume(&rctx->opctx, NGX_HTTP_ACCESS_PHASE);
    rc = ngx_http_wasm_check_finalize(rctx, rc);

done:

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, ngx_cycle->log, 0,
                   "wasm \"access\" phase rc: %d", rc);

    return rc;
}


static ngx_int_t
ngx_http_wasm_content(ngx_http_wasm_req_ctx_t *rctx)
{
    ngx_int_t            rc;
    ngx_http_request_t  *r = rctx->r;

    switch (rctx->state) {
    case NGX_HTTP_WASM_REQ_STATE_ERROR:
        dd("error");
        rc = NGX_HTTP_INTERNAL_SERVER_ERROR;
        goto done;
    default:
        dd("enter/continue");
        ngx_wasm_assert(rctx->state == NGX_HTTP_WASM_REQ_STATE_CONTINUE);
        break;
    }

    rc = ngx_http_wasm_flush_local_response(rctx);
    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "wasm flush_local_response pre-ops rc: %d", rc);

    switch (rc) {
    case NGX_ERROR:
        rc = NGX_HTTP_INTERNAL_SERVER_ERROR;
        /* fallthrough */
    case NGX_AGAIN:
        goto done;
    case NGX_OK:
        rc = ngx_http_wasm_check_finalize(rctx, rc);
        goto done;
    case NGX_DECLINED:
        if (rctx->exited_content_phase) {
            /* Content phase already ran, no stashed response.
             * Do not resume ops again and run the orig content
             * handler instead */
            goto orig;
        }
        /* fallthrough */
    default:
        break;
    }

    rc = ngx_wasm_ops_resume(&rctx->opctx, NGX_HTTP_CONTENT_PHASE);
    dd("content ops resume rc: %ld", rc);
    rc = ngx_http_wasm_check_finalize(rctx, rc);
    if (rc == NGX_ERROR
        || rc == NGX_DONE
        || rc >= NGX_HTTP_SPECIAL_RESPONSE)
    {
        goto done;
    }

    rc = ngx_http_wasm_flush_local_response(rctx);

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "wasm flush_local_response post-ops rc: %d", rc);

    switch (rc) {

    case NGX_ERROR:
        rc = NGX_HTTP_INTERNAL_SERVER_ERROR;
        /* fallthrough */

    case NGX_AGAIN:
        goto done;

    case NGX_OK:
        /* fallthrough */

    case NGX_DECLINED:

orig:

        if (rctx->r_content_handler && !rctx->resp_content_chosen) {
            ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                           "wasm running orig \"content\" handler");

            rctx->resp_content_chosen = 1;

            rc = rctx->r_content_handler(r);

        } else if (r->header_sent || rctx->resp_content_sent) {
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

    rctx->exited_content_phase = 1;

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

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, ngx_cycle->log, 0,
                   "wasm \"content\" phase rc: %d", rc);

    return rc;
}


static ngx_int_t
ngx_http_wasm_log_handler(ngx_http_request_t *r)
{
    ngx_int_t                  rc;
    ngx_http_wasm_req_ctx_t   *rctx;

    dd("enter");

    if (ngx_http_wasm_rctx(r, &rctx) != NGX_OK) {
        return NGX_ERROR;
    }

    rc = ngx_wasm_ops_resume(&rctx->opctx, NGX_HTTP_LOG_PHASE);

#if (NGX_HTTP_WASM_DONE_IN_LOG)
    (void) ngx_wasm_ops_resume(&rctx->opctx, NGX_WASM_DONE_PHASE);
#endif

    return rc;
}


void
ngx_http_wasm_content_wev_handler(ngx_http_request_t *r)
{
    ngx_int_t                  rc;
    ngx_http_wasm_req_ctx_t   *rctx;
#if (NGX_WASM_LUA)
    ngx_http_lua_ctx_t        *ctx;
#endif

#if (NGX_DEBUG)
    ngx_connection_t          *c = r->connection;
    ngx_event_t               *wev = c->write;
#endif

    if (ngx_http_wasm_rctx(r, &rctx) != NGX_OK) {
        return;
    }

    ngx_log_debug8(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "wasm wev handler \"%V?%V\" - timeout: %ud, ready: %ud "
                   "(main: %d, count: %d, resp_finalized: %d, state: %d)",
                   &r->uri, &r->args, wev->timedout, wev->ready, r == r->main,
                   r->main->count, rctx->resp_finalized, rctx->state);

#if 0
    {
        ngx_http_core_loc_conf_t  *clcf;

        clcf = ngx_http_get_module_loc_conf(r->main, ngx_http_core_module);

        if (wev->timedout) {
            if (!wev->delayed) {
                ngx_log_error(NGX_LOG_INFO, c->log, NGX_ETIMEDOUT,
                              "client timed out");
                c->timedout = 1;
                return;
            }

            wev->timedout = 0;
            wev->delayed = 0;

            if (!wev->ready) {
                ngx_add_timer(wev, clcf->send_timeout);

                if (ngx_handle_write_event(wev, clcf->send_lowat) != NGX_OK) {
                    if (rctx->entered_content_phase) {
                        ngx_http_finalize_request(r, NGX_ERROR);
                    }

                    return;
                }
            }
        }
    }
#endif

#if (NGX_WASM_LUA)
    ctx = ngx_http_get_module_ctx(r, ngx_http_lua_module);

    if (rctx->wasm_lua_ctx
        && rctx->wasm_lua_ctx->co_ctx->co_status != NGX_HTTP_LUA_CO_DEAD)
    {
        ctx->resume_handler(r);

        if (rctx->wasm_lua_ctx->co_ctx
            && rctx->wasm_lua_ctx->co_ctx->co_status != NGX_HTTP_LUA_CO_DEAD)
        {
            r->write_event_handler = ngx_http_wasm_content_wev_handler;
            return;

        } else {
            return;
        }
    }
#endif

    if (rctx->entered_content_phase || rctx->resp_content_chosen) {
        rc = ngx_http_wasm_content(rctx);
        if (rc >= NGX_HTTP_SPECIAL_RESPONSE) {
            ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
            return;

        } else if (rc == NGX_AGAIN) {
            ngx_http_finalize_request(r, NGX_AGAIN);
            return;
        }
    }

    if (rctx->fake_request) {
        dd("last finalize (fake request)");
        ngx_wasm_assert(r->connection->fd == NGX_WASM_BAD_FD);
        ngx_http_wasm_finalize_fake_request(r, NGX_DONE);

    } else {
        dd("last finalize");
        ngx_wasm_assert(r->connection->fd != NGX_WASM_BAD_FD);
        ngx_http_finalize_request(r, r == r->main ? NGX_DONE : NGX_OK);
    }
}


void
ngx_http_wasm_resume(ngx_http_wasm_req_ctx_t *rctx, unsigned main, unsigned wev)
{
    ngx_http_request_t  *r = rctx->r;
    ngx_connection_t    *c = r->connection;

    dd("enter");

    ngx_wasm_assert(wev);

    if (ngx_http_wasm_req_yielded(rctx)) {
        dd("yielded");
        return;
    }

    if (main) {
        if (wev) {
            if (rctx->resp_content_chosen) {
                dd("switching wev handler");
                r->write_event_handler = ngx_http_wasm_content_wev_handler;
            }

            dd("resuming request wev...");
            r->write_event_handler(r);
            dd("...done resuming request");

        }
#if 0
        else {
            dd("resuming request rev...");
            r->read_event_handler(r);
            dd("...done resuming request");
        }
#endif
    }

    dd("running posted requests...");
    ngx_http_run_posted_requests(c);
    dd("...done running posted requests");
}
