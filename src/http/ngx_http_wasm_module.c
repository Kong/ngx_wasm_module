#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_http_wasm_module.h>
#include <ngx_wasm_util.h>


typedef enum {
    NGX_HTTP_WASM_VMCACHE_NONE = 1,
    NGX_HTTP_WASM_VMCACHE_LOC,
    NGX_HTTP_WASM_VMCACHE_REQ
} ngx_http_wasm_vmcache_mode_t;


typedef struct {
    ngx_uint_t                       vmcache_mode;
    ngx_wasm_vm_cache_t              vmcache;
    ngx_wasm_phases_engine_t         phengine;
} ngx_http_wasm_loc_conf_t;


#if (NGX_DEBUG)
static const char *
ngx_http_wasm_vmcache_mode_name(ngx_uint_t mode)
{
    switch (mode) {

    case NGX_HTTP_WASM_VMCACHE_LOC:
        return "location";

    case NGX_HTTP_WASM_VMCACHE_REQ:
        return "request";

    case NGX_HTTP_WASM_VMCACHE_NONE:
        return "none";

    default:
        ngx_wasm_assert(0);
        break;

    }

    return NULL;
}
#endif


static void *ngx_http_wasm_create_loc_conf(ngx_conf_t *cf);
char *ngx_http_wasm_isolation_directive(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
char *ngx_http_wasm_call_directive(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
char *ngx_http_wasm_proxy_wasm_directive(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
char *ngx_http_wasm_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child);
static ngx_int_t ngx_http_wasm_init(ngx_conf_t *cf);
static ngx_int_t ngx_http_wasm_rewrite_handler(ngx_http_request_t *r);
static ngx_int_t ngx_http_wasm_content_handler(ngx_http_request_t *r);
static ngx_int_t ngx_http_wasm_log_handler(ngx_http_request_t *r);


static ngx_command_t  ngx_http_wasm_module_cmds[] = {

    { ngx_string("wasm_isolation"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_http_wasm_isolation_directive,
      NGX_HTTP_LOC_CONF_OFFSET,
      NGX_HTTP_MODULE,
      NULL },

    { ngx_string("wasm_call"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE3,
      ngx_http_wasm_call_directive,
      NGX_HTTP_LOC_CONF_OFFSET,
      NGX_HTTP_MODULE,
      NULL },

    { ngx_string("proxy_wasm"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_http_wasm_proxy_wasm_directive,
      NGX_HTTP_LOC_CONF_OFFSET,
      NGX_HTTP_MODULE,
      NULL },

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
    ngx_wasm_vm_t             *vm;

    vm = ngx_wasm_core_get_vm(cf->cycle);
    if (vm == NULL) {
        return NULL;
    }

    loc = ngx_pcalloc(cf->pool, sizeof(ngx_http_wasm_loc_conf_t));
    if (loc == NULL) {
        return NULL;
    }

    loc->vmcache_mode = NGX_CONF_UNSET_UINT;
    loc->phengine.vm = vm;
    loc->phengine.subsys = NGX_WASM_HOST_SUBSYS_HTTP;
    loc->phengine.nphases = NGX_HTTP_LOG_PHASE + 1;
    loc->phengine.phases = ngx_pcalloc(cf->pool, loc->phengine.nphases *
                                       sizeof(ngx_wasm_phases_phase_t));
    if (loc->phengine.phases == NULL) {
        return NULL;
    }

    return loc;
}


char *
ngx_http_wasm_isolation_directive(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf)
{
    ngx_str_t                 *value;
    ngx_http_wasm_loc_conf_t  *loc = conf;

    value = cf->args->elts;

    if (ngx_strncmp(value[1].data, "location", 8) == 0) {
        loc->vmcache_mode = NGX_HTTP_WASM_VMCACHE_LOC;

    } else if (ngx_strncmp(value[1].data, "request", 7) == 0) {
        loc->vmcache_mode = NGX_HTTP_WASM_VMCACHE_REQ;

    } else if (ngx_strncmp(value[1].data, "none", 9) == 0) {
        loc->vmcache_mode = NGX_HTTP_WASM_VMCACHE_NONE;

    } else {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "invalid isolation mode \"%V\"", &value[1]);
        return NGX_CONF_ERROR;
    }

    return NGX_CONF_OK;
}


char *
ngx_http_wasm_call_directive(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_str_t                 *value;
    ngx_http_core_loc_conf_t  *clcf;
    ngx_http_wasm_loc_conf_t  *loc = conf;
    ngx_wasm_phases_op_t      *op;

    value = cf->args->elts;

    op = ngx_wasm_phases_conf_add_op_call(cf, &loc->phengine, &value[1],
                                          &value[2], &value[3]);
    if (op == NULL) {
        return NGX_CONF_ERROR;
    }

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
ngx_http_wasm_proxy_wasm_directive(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf)
{
    ngx_str_t                 *value;
    ngx_http_core_loc_conf_t  *clcf;
    ngx_http_wasm_loc_conf_t  *loc = conf;
    ngx_wasm_phases_op_t      *op;

    value = cf->args->elts;

    op = ngx_wasm_phases_conf_add_op_proxy_wasm(cf, &loc->phengine, &value[1]);
    if (op == NULL) {
        return NGX_CONF_ERROR;
    }

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
ngx_http_wasm_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child)
{
    size_t                     i;
    ngx_http_wasm_loc_conf_t  *prev = parent;
    ngx_http_wasm_loc_conf_t  *conf = child;

    ngx_conf_merge_uint_value(conf->vmcache_mode, prev->vmcache_mode,
                              NGX_HTTP_WASM_VMCACHE_NONE);

    for (i = 0; i < conf->phengine.nphases; i++) {
        if (conf->phengine.phases[i] == NULL) {
            conf->phengine.phases[i] = prev->phengine.phases[i];
        }
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


static ngx_http_wasm_req_ctx_t *
ngx_http_wasm_rctx(ngx_http_request_t *r)
{
    ngx_http_wasm_req_ctx_t   *rctx;
    ngx_http_wasm_loc_conf_t  *loc;
    ngx_wasm_phases_ctx_t     *pctx;
    ngx_wasm_vm_cache_t       *vmcache;

    rctx = ngx_http_get_module_ctx(r, ngx_http_wasm_module);
    if (rctx == NULL) {
        loc = ngx_http_get_module_loc_conf(r, ngx_http_wasm_module);

        rctx = ngx_pcalloc(r->pool, sizeof(ngx_http_wasm_req_ctx_t));
        if (rctx == NULL) {
            return NULL;
        }

        rctx->r = r;

        ngx_http_set_ctx(r, rctx, ngx_http_wasm_module);

        switch (loc->vmcache_mode) {

        case NGX_HTTP_WASM_VMCACHE_LOC:
            vmcache = &loc->vmcache;
            break;

        case NGX_HTTP_WASM_VMCACHE_REQ:
            if (rctx->vmcache == NULL) {
                vmcache = ngx_palloc(r->pool, sizeof(ngx_wasm_vm_cache_t));
                if (vmcache == NULL) {
                    ngx_log_error(NGX_LOG_EMERG, r->connection->log, 0,
                                  "failed to allocate vmcache: no memory");
                    goto done;
                }

                ngx_wasm_vm_cache_init(vmcache);
                rctx->vmcache = vmcache;
            }

            vmcache = rctx->vmcache;
            break;

        case NGX_HTTP_WASM_VMCACHE_NONE:
            vmcache = NULL;
            break;

        default:
            ngx_log_error(NGX_LOG_ALERT, r->connection->log, 0,
                          "NYI - http wasm: unsupported  "
                          "isolation mode \"%u\"", loc->vmcache_mode);
            vmcache = NULL;
            ngx_wasm_assert(0);

        }

        pctx = &rctx->pctx;
        pctx->phengine = &loc->phengine;
        pctx->vmcache = vmcache;
        pctx->pool = r->pool;
        pctx->log = r->connection->log;
        pctx->data = rctx;

#if (NGX_DEBUG)
        ngx_log_debug1(NGX_LOG_DEBUG_WASM, pctx->log, 0,
                       "wasm using \"%s\" isolation mode",
                       ngx_http_wasm_vmcache_mode_name(loc->vmcache_mode));
#endif
    }

done:

    return rctx;
}


static ngx_int_t
ngx_http_wasm_rewrite_handler(ngx_http_request_t *r)
{
    ngx_int_t                 rc;
    ngx_http_wasm_req_ctx_t  *rctx;

    rctx = ngx_http_wasm_rctx(r);
    if (rctx == NULL) {
        return NGX_ERROR;
    }

    rc = ngx_wasm_phases_resume(&rctx->pctx, NGX_HTTP_REWRITE_PHASE);
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

    rctx = ngx_http_wasm_rctx(r);
    if (rctx == NULL) {
        return NGX_ERROR;
    }

    rc = ngx_wasm_phases_resume(&rctx->pctx, NGX_HTTP_CONTENT_PHASE);
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

    rctx = ngx_http_wasm_rctx(r);
    if (rctx == NULL) {
        return NGX_ERROR;
    }

    rc = ngx_wasm_phases_resume(&rctx->pctx, NGX_HTTP_LOG_PHASE);
    if (rc != NGX_OK) {
        return rc;
    }

    return NGX_OK;
}
