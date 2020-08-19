/*
 * Copyright (C) Thibault Charbonnier
 */

#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_core.h>
#include <ngx_http.h>
#include <ngx_http_wasm_util.h>

#include <ngx_wasm_vm.h>
#include <ngx_wasm_vm_cache.h>
#include <ngx_wasm_util.h>

#include <ngx_wasm_module.h>


typedef struct {
    ngx_str_t            mod_name;
    ngx_str_t            func_name;
} ngx_http_wasm_call_t;


typedef struct {
    ngx_http_phases      phase;
    ngx_str_t            phase_name;
    ngx_array_t         *calls;
} ngx_http_wasm_phase_engine_t;


typedef struct {
    ngx_http_wasm_phase_engine_t  *phase_engines[NGX_HTTP_LOG_PHASE + 1];
} ngx_http_wasm_loc_conf_t;


typedef struct {
    u_short               phases[NGX_HTTP_LOG_PHASE + 1];
    ngx_wasm_vm_t        *vm;
} ngx_http_wasm_main_conf_t;


typedef struct {
    ngx_http_request_t   *request;
    ngx_wasm_vm_cache_t   vmcache;
} ngx_http_wasm_req_ctx_t;


static void *ngx_http_wasm_create_main_conf(ngx_conf_t *cf);
static void *ngx_http_wasm_create_loc_conf(ngx_conf_t *cf);
static char *ngx_http_wasm_wasm_call_directive(ngx_conf_t *cf,
    ngx_command_t *cmd, void *conf);
static char *ngx_http_wasm_merge_loc_conf(ngx_conf_t *cf, void *parent,
    void *child);
static ngx_int_t ngx_http_wasm_init(ngx_conf_t *cf);
static ngx_int_t ngx_http_wasm_log_handler(ngx_http_request_t *r);
static ngx_int_t ngx_http_wasm_rewrite_handler(ngx_http_request_t *r);
static void ngx_http_wasm_pool_cleanup(void *data);
static ngx_http_wasm_req_ctx_t *ngx_http_wasm_ctx_new(ngx_http_request_t *r);
static ngx_int_t ngx_http_wasm_exec_phase(ngx_http_request_t *r,
    ngx_http_phases phase);


/* ngx_http_wasm_module */


static ngx_command_t  ngx_http_wasm_module_cmds[] = {

    { ngx_string("wasm_call"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE3,
      ngx_http_wasm_wasm_call_directive,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },

      ngx_null_command
};


static ngx_http_module_t  ngx_http_wasm_module_ctx = {
    NULL,                                /* preconfiguration */
    ngx_http_wasm_init,                  /* postconfiguration */

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
    NULL,                                /* init process */
    NULL,                                /* init thread */
    NULL,                                /* exit thread */
    NULL,                                /* exit process */
    NULL,                                /* exit master */
    NGX_MODULE_V1_PADDING
};


static ngx_http_handler_pt  phase_handlers[NGX_HTTP_LOG_PHASE + 1] = {
    NULL,                          /* post_read */
    NULL,                          /* server_rewrite */
    NULL,                          /* find_config */
    ngx_http_wasm_rewrite_handler, /* rewrite */
    NULL,                          /* post_rewrite */
    NULL,                          /* pre_access */
    NULL,                          /* access */
    NULL,                          /* post_access*/
    NULL,                          /* pre_content */
    NULL,                          /* content */
    ngx_http_wasm_log_handler      /* log */
};


/* configuration & init */


static void *
ngx_http_wasm_create_main_conf(ngx_conf_t *cf)
{
    ngx_http_wasm_main_conf_t       *mcf;

    mcf = ngx_pcalloc(cf->pool, sizeof(ngx_http_wasm_main_conf_t));
    if (mcf == NULL) {
        return NULL;
    }

    mcf->vm = ngx_wasm_core_get_vm(cf->cycle);
    if (mcf->vm == NULL) {
        return NULL;
    }

    return mcf;
}


static void *
ngx_http_wasm_create_loc_conf(ngx_conf_t *cf)
{
    ngx_http_wasm_loc_conf_t        *lcf;
    size_t                           i;

    lcf = ngx_pcalloc(cf->pool, sizeof(ngx_http_wasm_loc_conf_t));
    if (lcf == NULL) {
        return NULL;
    }

    for (i = 0; i < NGX_HTTP_LOG_PHASE + 1; i++) {
        lcf->phase_engines[i] = NGX_CONF_UNSET_PTR;
    }

    return lcf;
}


static char *
ngx_http_wasm_wasm_call_directive(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf)
{
    ngx_str_t                      *value, phase_name;
    ngx_http_phases                 phase;
    ngx_http_wasm_main_conf_t      *mcf;
    ngx_http_wasm_loc_conf_t       *lcf = conf;
    ngx_http_wasm_call_t          **pcall, *call;
    ngx_http_wasm_phase_engine_t   *phase_engine;

    mcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_wasm_module);

    value = cf->args->elts;

    call = ngx_pcalloc(cf->pool, sizeof(ngx_http_wasm_call_t));
    if (call == NULL) {
        return NGX_CONF_ERROR;
    }

    phase_name.data = value[1].data;
    phase_name.len = value[1].len;

    call->mod_name.data = value[2].data;
    call->mod_name.len = value[2].len;

    call->func_name.data = value[3].data;
    call->func_name.len = value[3].len;

    if (phase_name.len == 0) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "invalid phase \"%V\"",
                           &phase_name);
        return NGX_CONF_ERROR;
    }

    if (call->mod_name.len == 0) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "invalid module name \"%V\"",
                           &call->mod_name);
        return NGX_CONF_ERROR;
    }

    if (call->func_name.len == 0) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "invalid function name \"%V\"",
                           &call->func_name);
        return NGX_CONF_ERROR;
    }

    if (ngx_http_wasm_conf_parse_phase(cf, phase_name.data, &phase)
        != NGX_CONF_OK)
    {
        return NGX_CONF_ERROR;
    }

    if (phase_handlers[phase] == NULL) {
        ngx_conf_log_error(NGX_LOG_ALERT, cf, 0, "NYI - http wasm: "
                           "unsupported phase \"%V\"", &phase_name);
        return NGX_CONF_ERROR;
    }

    if (ngx_wasm_vm_get_module(mcf->vm, &call->mod_name) == NULL) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "[wasm] no \"%V\" module defined", &call->mod_name);
        return NGX_CONF_ERROR;
    }

    phase_engine = lcf->phase_engines[phase];

    if (phase_engine == NGX_CONF_UNSET_PTR) {
        phase_engine = ngx_palloc(cf->pool,
                                  sizeof(ngx_http_wasm_phase_engine_t));
        if (phase_engine == NULL) {
            return NGX_CONF_ERROR;
        }

        phase_engine->phase = phase;
        phase_engine->phase_name.data = phase_name.data;
        phase_engine->phase_name.len = phase_name.len;
        phase_engine->calls = ngx_array_create(cf->pool, 2,
                                  sizeof(ngx_http_wasm_call_t *));
        if (phase_engine->calls == NULL) {
            return NGX_CONF_ERROR;
        }

        lcf->phase_engines[phase] = phase_engine;
    }

    pcall = ngx_array_push(phase_engine->calls);
    if (pcall == NULL) {
        return NGX_CONF_ERROR;
    }

    *pcall = call;

    mcf->phases[phase] = 1;

    return NGX_CONF_OK;
}


static char *
ngx_http_wasm_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child)
{
    size_t                         i;
    ngx_http_wasm_loc_conf_t      *prev = parent;
    ngx_http_wasm_loc_conf_t      *conf = child;

    for (i = 0; i < NGX_HTTP_LOG_PHASE + 1; i++) {
        ngx_conf_merge_ptr_value(conf->phase_engines[i],
                                 prev->phase_engines[i], NULL);
    }

    return NGX_CONF_OK;
}


static ngx_int_t
ngx_http_wasm_init(ngx_conf_t *cf)
{
    size_t                       i;
    ngx_http_handler_pt         *h;
    ngx_http_core_main_conf_t   *cmcf;
    ngx_http_wasm_main_conf_t   *mcf;

    cmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);
    mcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_wasm_module);

    for (i = 0; i <= NGX_HTTP_LOG_PHASE; i++) {
        if (mcf->phases[i]) {
            if (phase_handlers[i] == NULL) {
                ngx_log_error(NGX_LOG_ALERT, cf->log, 0,
                              "NYI - http wasm: no phase handler declared for "
                              "phase \"%d\"", i);
                return NGX_ERROR;
            }

            h = ngx_array_push(&cmcf->phases[i].handlers);
            if (h == NULL) {
                return NGX_ERROR;
            }

            *h = phase_handlers[i];
        }
    }

    return NGX_OK;
}


/* Runtime phases */


static ngx_int_t
ngx_http_wasm_log_handler(ngx_http_request_t *r)
{
    return ngx_http_wasm_exec_phase(r, NGX_HTTP_LOG_PHASE);
}


static ngx_int_t
ngx_http_wasm_rewrite_handler(ngx_http_request_t *r)
{
    (void) ngx_http_wasm_exec_phase(r, NGX_HTTP_REWRITE_PHASE);

    /* NYI */

    return NGX_DECLINED;
}


static ngx_http_wasm_req_ctx_t *
ngx_http_wasm_ctx_new(ngx_http_request_t *r)
{
    ngx_pool_cleanup_t         *cln;
    ngx_http_wasm_main_conf_t  *mcf;
    ngx_http_wasm_req_ctx_t    *rctx;

    mcf = ngx_http_get_module_main_conf(r, ngx_http_wasm_module);

    rctx = ngx_palloc(r->pool, sizeof(ngx_http_wasm_req_ctx_t));
    if (rctx == NULL) {
        return NULL;
    }

    rctx->request = r;
    rctx->vmcache.pool = r->pool;
    rctx->vmcache.vm = mcf->vm;

    ngx_wasm_vm_cache_init(&rctx->vmcache);

    cln = ngx_pool_cleanup_add(r->pool, 0);
    if (cln == NULL) {
        ngx_pfree(r->pool, rctx);
        return NULL;
    }

    cln->handler = ngx_http_wasm_pool_cleanup;
    cln->data = rctx;

    ngx_http_set_ctx(r, rctx, ngx_http_wasm_module);

    return rctx;
}


static ngx_int_t
ngx_http_wasm_exec_phase(ngx_http_request_t *r, ngx_http_phases phase)
{
    size_t                         i;
    ngx_int_t                      rc;
    ngx_http_wasm_phase_engine_t  *phase_engine;
    ngx_http_wasm_loc_conf_t      *lcf;
    ngx_http_wasm_req_ctx_t       *rctx;
    ngx_http_wasm_call_t          *call;
    ngx_wasm_vm_instance_t        *instance;

    lcf = ngx_http_get_module_loc_conf(r, ngx_http_wasm_module);

    phase_engine = lcf->phase_engines[phase];
    if (phase_engine == NULL) {
        return NGX_OK;
    }

    rctx = ngx_http_get_module_ctx(r, ngx_http_wasm_module);
    if (rctx == NULL) {
        rctx = ngx_http_wasm_ctx_new(r);
        if (rctx == NULL) {
            return NGX_ERROR;
        }
    }

    for (i = 0; i < phase_engine->calls->nelts; i++) {
        call = ((ngx_http_wasm_call_t **) phase_engine->calls->elts)[i];

        ngx_log_debug3(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                       "wasm: calling \"%V.%V\" in \"%V\" phase",
                       &call->mod_name, &call->func_name,
                       &phase_engine->phase_name);

        instance = ngx_wasm_vm_cache_get_instance(&rctx->vmcache,
                                                  &call->mod_name);
        if (instance == NULL) {
            return NGX_ERROR;
        }

        ngx_wasm_vm_instance_bind_request(instance, r);

        rc = ngx_wasm_vm_instance_call(instance, &call->func_name);
        if (rc != NGX_OK) {
            return NGX_ERROR;
        }
    }

    return NGX_OK;
}


static void
ngx_http_wasm_pool_cleanup(void *data)
{
    ngx_http_wasm_req_ctx_t  *rctx = data;

    ngx_wasm_vm_cache_cleanup(&rctx->vmcache);
}
