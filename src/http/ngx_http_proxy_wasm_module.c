/*
 * Copyright (C) Thibault Charbonnier
 */

#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_config.h>
#include <ngx_http.h>
#include <ngx_wasm.h>


static void *ngx_http_proxy_wasm_create_loc_conf(ngx_conf_t *cf);
static char *ngx_http_proxy_wasm_proxy_wasm_directive(ngx_conf_t *cf,
    ngx_command_t *cmd, void *conf);
static char *ngx_http_proxy_wasm_merge_loc_conf(ngx_conf_t *cf,
    void *parent, void *child);
static ngx_int_t ngx_http_proxy_wasm_init(ngx_cycle_t *cycle);


typedef struct {
    ngx_wasm_wmodule_t      *module;
} ngx_http_proxy_wasm_loc_conf_t;


static ngx_command_t  ngx_http_proxy_wasm_module_cmds[] = {

    { ngx_string("proxy_wasm_module"),
      NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_http_proxy_wasm_proxy_wasm_directive,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },

      ngx_null_command
};


static ngx_http_module_t  ngx_http_proxy_wasm_module_ctx = {
    NULL,                                  /* preconfiguration */
    NULL,                                  /* postconfiguration */

    NULL,                                  /* create main configuration */
    NULL,                                  /* init main configuration */

    NULL,                                  /* create server configuration */
    NULL,                                  /* merge server configuration */

    ngx_http_proxy_wasm_create_loc_conf,   /* create location configuration */
    ngx_http_proxy_wasm_merge_loc_conf     /* merge location configuration */
};


ngx_module_t  ngx_http_proxy_wasm_module = {
    NGX_MODULE_V1,
    &ngx_http_proxy_wasm_module_ctx,     /* module context */
    ngx_http_proxy_wasm_module_cmds,     /* module directives */
    NGX_HTTP_MODULE,                     /* module type */
    NULL,                                /* init master */
    ngx_http_proxy_wasm_init,            /* init module */
    NULL,                                /* init process */
    NULL,                                /* init thread */
    NULL,                                /* exit thread */
    NULL,                                /* exit process */
    NULL,                                /* exit master */
    NGX_MODULE_V1_PADDING
};


static void *
ngx_http_proxy_wasm_create_loc_conf(ngx_conf_t *cf)
{
    ngx_http_proxy_wasm_loc_conf_t    *pwlcf;

    pwlcf = ngx_pcalloc(cf->pool, sizeof(ngx_http_proxy_wasm_loc_conf_t));
    if (pwlcf == NULL) {
        return NULL;
    }

    pwlcf->module = NGX_CONF_UNSET_PTR;

    return pwlcf;
}


static char *
ngx_http_proxy_wasm_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child)
{
    ngx_http_proxy_wasm_loc_conf_t    *prev = parent;
    ngx_http_proxy_wasm_loc_conf_t    *conf = child;

    ngx_conf_merge_ptr_value(conf->module, prev->module, NULL);

    return NGX_CONF_OK;
}


static char *
ngx_http_proxy_wasm_proxy_wasm_directive(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf)
{
    ngx_http_proxy_wasm_loc_conf_t     *pwlcf = conf;
    ngx_str_t                          *value, *name;
    ngx_wasm_wmodule_t                 *module;

    if (pwlcf->module != NGX_CONF_UNSET_PTR) {
        return "is duplicate";
    }

    value = cf->args->elts;
    name = &value[1];

    if (name->len == 0) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "invalid module name \"%V\"",
                           name);
        return NGX_CONF_ERROR;
    }

    module = ngx_wasm_get_module(name->data, cf->cycle);
    if (module == NULL) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                          "no \"%V\" module defined", name);
        return NGX_CONF_ERROR;
    }

    pwlcf->module = module;

    return NGX_CONF_OK;
}


static ngx_int_t
ngx_http_proxy_wasm_init(ngx_cycle_t *cycle)
{
    /*
    ngx_http_proxy_wasm_main_conf_t    *pwmcf;
    ngx_wasm_wmodule_t                 *module, **pmodule;
    ngx_wasm_winstance_t               *instance;
    ngx_uint_t                          i;
    ngx_int_t                           rc;

    pwmcf = ngx_http_cycle_get_module_main_conf(cycle,
                                                ngx_http_proxy_wasm_module);

    pmodule = pwmcf->modules->elts;

    for (i = 0; i < pwmcf->modules->nelts; i++) {
        module = pmodule[i];

        ngx_log_debug1(NGX_LOG_DEBUG_WASM, cycle->log, 0,
                       "loading wasm module at \"%s\"", module->path.data);

        if (ngx_wasm_vm_actions.load_module(module, cycle) != NGX_OK) {
            return NGX_ERROR;
        }

        instance = ngx_wasm_vm_actions.new_instance(module);
        if (instance == NULL) {
            return NGX_ERROR;
        }

        rc = ngx_wasm_vm_actions.call_instance(instance, "_start",
                                               NULL, 0, NULL, 0);
        if (rc != NGX_OK && rc != NGX_DONE) {
            return NGX_ERROR;
        }

        ngx_wasm_vm_actions.free_instance(instance);
    }
    */

    return NGX_OK;
}


/* vi:set ft=c ts=4 sw=4 et fdm=marker: */
