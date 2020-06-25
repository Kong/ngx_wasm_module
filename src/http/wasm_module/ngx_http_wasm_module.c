/*
 * Copyright (C) Thibault Charbonnier
 */

#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_config.h>
#include <ngx_http.h>
#include <ngx_wasm_vm.h>
#include <ngx_wasm_module.h>
#include <ngx_http_wasm_hostfuncs.h>


#define NGX_WASM_HTTP_LAST_PHASE  NGX_HTTP_LOG_PHASE


static void *ngx_http_wasm_create_main_conf(ngx_conf_t *cf);
static void *ngx_http_wasm_create_loc_conf(ngx_conf_t *cf);
static char *ngx_http_wasm_wasm_call_directive(ngx_conf_t *cf,
    ngx_command_t *cmd, void *conf);
static char *ngx_http_wasm_merge_loc_conf(ngx_conf_t *cf, void *parent,
    void *child);
static ngx_int_t ngx_http_wasm_init(ngx_conf_t *cf);
static ngx_int_t ngx_http_wasm_exec_on_phase(ngx_http_request_t *r,
    ngx_http_phases phase);
static ngx_int_t ngx_http_wasm_log_handler(ngx_http_request_t *r);


typedef struct {
    u_short              phases[NGX_WASM_HTTP_LAST_PHASE + 1];
    ngx_wasm_vm_t       *default_vm;
} ngx_http_wasm_main_conf_t;


typedef struct {
    ngx_array_t         *on_phases[NGX_WASM_HTTP_LAST_PHASE + 1];
} ngx_http_wasm_loc_conf_t;


typedef struct {
    ngx_str_t            mname;
    ngx_str_t            fname;
    ngx_array_t         *args;
} ngx_http_wasm_call_t;


/* ngx_http_wasm_module */


static ngx_command_t  ngx_http_wasm_module_cmds[] = {

    { ngx_string("wasm_call_log"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_2MORE,
      ngx_http_wasm_wasm_call_directive,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      (ngx_http_phases *) NGX_HTTP_LOG_PHASE },

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


static ngx_http_handler_pt  phases_handlers[NGX_WASM_HTTP_LAST_PHASE + 1] = {
    NULL,
    NULL,
    NULL,
    NULL,                                /* rewrite phase */
    NULL,
    NULL,
    NULL,                                /* access phase */
    NULL,
    NULL,
    NULL,                                /* content phase */
    ngx_http_wasm_log_handler
};


static void *
ngx_http_wasm_create_main_conf(ngx_conf_t *cf)
{
    ngx_http_wasm_main_conf_t       *mcf;

    mcf = ngx_pcalloc(cf->pool, sizeof(ngx_http_wasm_main_conf_t));
    if (mcf == NULL) {
        return NULL;
    }

    mcf->default_vm = ngx_wasm_core_get_default_vm(cf->cycle);
    if (mcf->default_vm == NULL) {
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

    for (i = 0; i <= NGX_WASM_HTTP_LAST_PHASE; i++) {
        lcf->on_phases[i] = NGX_CONF_UNSET_PTR;
    }

    return lcf;
}


static char *
ngx_http_wasm_wasm_call_directive(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf)
{
    ngx_http_wasm_main_conf_t       *mcf;
    ngx_http_wasm_loc_conf_t        *lcf = conf;
    ngx_http_phases                  phase = (ngx_http_phases) cmd->post;
    ngx_http_wasm_call_t           **pcall, *call;
    ngx_str_t                       *value, *mname, *fname;
    u_char                          *p;

    mcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_wasm_module);

    value = cf->args->elts;
    mname = &value[1];
    fname = &value[2];

    if (mname->len == 0) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "invalid module name \"%V\"",
                           mname);
        return NGX_CONF_ERROR;
    }

    if (fname->len == 0) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "invalid function name \"%V\"",
                           fname);
        return NGX_CONF_ERROR;
    }

    if (ngx_wasm_vm_has_module(mcf->default_vm, mname->data) != NGX_OK) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "[wasm] no \"%V\" module defined", mname);
        return NGX_CONF_ERROR;
    }

    if (lcf->on_phases[phase] == NGX_CONF_UNSET_PTR) {
        lcf->on_phases[phase] = ngx_array_create(cf->pool, 2,
                                    sizeof(ngx_http_wasm_call_t *));
        if (lcf->on_phases[phase] == NULL) {
            return NGX_CONF_ERROR;
        }
    }

    lcf->on_phases[phase] = lcf->on_phases[phase];

    pcall = ngx_array_push(lcf->on_phases[phase]);
    if (pcall == NULL) {
        return NGX_CONF_ERROR;
    }

    *pcall = ngx_pcalloc(cf->pool, sizeof(ngx_http_wasm_call_t));
    if (*pcall == NULL) {
        return NGX_CONF_ERROR;
    }

    call = *pcall;

    call->mname.len = mname->len;
    call->mname.data = ngx_palloc(cf->pool, mname->len + 1);
    if (call->mname.data == NULL) {
        return NGX_CONF_ERROR;
    }

    p = ngx_copy(call->mname.data, mname->data, mname->len);
    *p = '\0';

    call->fname.len = fname->len;
    call->fname.data = ngx_palloc(cf->pool, fname->len + 1);
    if (call->fname.data == NULL) {
        return NGX_CONF_ERROR;
    }

    p = ngx_copy(call->fname.data, fname->data, fname->len);
    *p = '\0';

    mcf->phases[phase] = 1;

    return NGX_CONF_OK;
}


static char *
ngx_http_wasm_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child)
{
    ngx_http_wasm_loc_conf_t    *prev = parent;
    ngx_http_wasm_loc_conf_t    *conf = child;
    size_t                       i;

    for (i = 0; i <= NGX_WASM_HTTP_LAST_PHASE; i++) {
        ngx_conf_merge_ptr_value(conf->on_phases[i], prev->on_phases[i], NULL);
    }

    return NGX_CONF_OK;
}


static ngx_int_t
ngx_http_wasm_init(ngx_conf_t *cf)
{
    ngx_http_core_main_conf_t   *cmcf;
    ngx_http_wasm_main_conf_t   *mcf;
    ngx_http_handler_pt         *h;
    size_t                       i;

    cmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);
    mcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_wasm_module);

    for (i = 0; i <= NGX_WASM_HTTP_LAST_PHASE; i++) {
        if (mcf->phases[i]) {
            if (phases_handlers[i] == NULL) {
                ngx_log_error(NGX_LOG_ALERT, cf->log, 0,
                              "NYI - http wasm: no phase handler declared for "
                              "phase \"%d\"", i);
                return NGX_ERROR;
            }

            h = ngx_array_push(&cmcf->phases[i].handlers);
            if (h == NULL) {
                return NGX_ERROR;
            }

            *h = phases_handlers[i];
        }
    }

    ngx_wasm_core_hfuncs_add(cf->cycle, ngx_http_wasm_hfuncs);

    return NGX_OK;
}


static ngx_int_t
ngx_http_wasm_exec_on_phase(ngx_http_request_t *r, ngx_http_phases phase)
{
    ngx_http_wasm_main_conf_t   *mcf;
    ngx_http_wasm_loc_conf_t    *lcf;
    ngx_array_t                 *calls_arr;
    ngx_wasm_instance_t         *instance;
    ngx_wasm_hctx_t              hctx;
    ngx_http_wasm_call_t       **calls, *call;
    ngx_uint_t                   i;
    //ngx_int_t                    rc = NGX_ERROR;

    mcf = ngx_http_get_module_main_conf(r, ngx_http_wasm_module);
    lcf = ngx_http_get_module_loc_conf(r, ngx_http_wasm_module);

    calls_arr = lcf->on_phases[phase];
    if (calls_arr == NULL) {
        return NGX_OK;
    }

    hctx.log = r->connection->log;
    hctx.data.r = r;

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "wasm: executing %d call(s) in log phase",
                   calls_arr->nelts);

    for (calls = calls_arr->elts, i = 0; i < calls_arr->nelts; i++) {
        call = calls[i];

        instance = ngx_wasm_vm_instance_new(mcf->default_vm, &call->mname, &hctx);
        if (instance == NULL) {
            return NGX_ERROR;
        }

        (void) ngx_wasm_vm_instance_call(instance, &call->fname);

        ngx_wasm_vm_instance_free(instance);
    }

    return NGX_OK;
}


static ngx_int_t
ngx_http_wasm_log_handler(ngx_http_request_t *r)
{
    return ngx_http_wasm_exec_on_phase(r, NGX_HTTP_LOG_PHASE);
}
