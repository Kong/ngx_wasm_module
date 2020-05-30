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


static void     *ngx_http_proxy_wasm_create_main_conf(ngx_conf_t *cf);
static char     *ngx_http_proxy_wasm_init_main_conf(ngx_conf_t *cf, void *conf);
static void     *ngx_http_proxy_wasm_create_srv_conf(ngx_conf_t *cf);
static void     *ngx_http_proxy_wasm_create_loc_conf(ngx_conf_t *cf);
static char     *ngx_http_proxy_wasm_merge_loc_conf(ngx_conf_t *cf,
                                                    void *parent,
                                                    void *child);
static char     *ngx_http_proxy_wasm_module_directive(ngx_conf_t *cf,
                                                      ngx_command_t *cmd,
                                                      void *conf);
static ngx_int_t ngx_http_proxy_wasm_init(ngx_cycle_t *cycle);


typedef struct {
    ngx_array_t             *modules;
} ngx_http_proxy_wasm_main_conf_t;


typedef struct {

} ngx_http_proxy_wasm_srv_conf_t;


typedef struct {
    ngx_wasm_wmodule_t      *module;
} ngx_http_proxy_wasm_loc_conf_t;


static ngx_command_t  ngx_http_proxy_wasm_module_cmds[] = {

    { ngx_string("wasm_module"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_http_proxy_wasm_module_directive,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },

      ngx_null_command
};


static ngx_http_module_t  ngx_http_proxy_wasm_module_ctx = {
    NULL,                                  /* preconfiguration */
    NULL,                                  /* postconfiguration */

    ngx_http_proxy_wasm_create_main_conf,  /* create main configuration */
    ngx_http_proxy_wasm_init_main_conf,    /* init main configuration */

    ngx_http_proxy_wasm_create_srv_conf,   /* create server configuration */
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
ngx_http_proxy_wasm_create_main_conf(ngx_conf_t *cf)
{
    ngx_http_proxy_wasm_main_conf_t    *pwmcf;

    pwmcf = ngx_pcalloc(cf->pool, sizeof(ngx_http_proxy_wasm_main_conf_t));
    if (pwmcf == NULL) {
        return NULL;
    }

    pwmcf->modules = ngx_array_create(cf->pool, 2,
                                      sizeof(ngx_wasm_wmodule_t *));
    if (pwmcf->modules == NULL) {
        return NGX_CONF_ERROR;
    }

    return pwmcf;
}


static char *
ngx_http_proxy_wasm_init_main_conf(ngx_conf_t *cf, void *conf)
{
    return NGX_CONF_OK;
}


static void *
ngx_http_proxy_wasm_create_srv_conf(ngx_conf_t *cf)
{
    ngx_http_proxy_wasm_srv_conf_t    *pwscf;

    pwscf = ngx_pcalloc(cf->pool, sizeof(ngx_http_proxy_wasm_srv_conf_t));
    if (pwscf == NULL) {
        return NULL;
    }

    return pwscf;
}


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
ngx_http_proxy_wasm_module_directive(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf)
{
    ngx_http_proxy_wasm_loc_conf_t     *pwlcf = conf;
    ngx_http_proxy_wasm_main_conf_t    *pwmcf;
    ngx_str_t                          *value;
    u_char                             *p;
    ngx_wasm_wmodule_t                 *module, **pmodule;

    if (pwlcf->module != NGX_CONF_UNSET_PTR) {
        return "is duplicate";
    }

    value = cf->args->elts;

    if (value[1].len == 0) {
        ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "no file specified");
        return NGX_CONF_ERROR;
    }

    pwmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_proxy_wasm_module);

    module = ngx_palloc(cf->pool, sizeof(ngx_wasm_wmodule_t));
    if (module == NULL) {
        return NGX_CONF_ERROR;
    }

    module->log = cf->log;
    module->pool = cf->pool;
    module->wat = ngx_strcmp(&value[1].data[value[1].len - 4], ".wat") == 0;
    module->path.len = value[1].len;
    module->path.data = ngx_palloc(module->pool, value[1].len + 1);
    if (module->path.data == NULL) {
        return NGX_CONF_ERROR;
    }

    p = ngx_copy(module->path.data, value[1].data, value[1].len);
    *p = '\0';

    pmodule = ngx_array_push(pwmcf->modules);
    if (pmodule == NULL) {
        return NGX_CONF_ERROR;
    }

    *pmodule = module;
    pwlcf->module = module;

    return NGX_CONF_OK;
}


static ngx_int_t
ngx_http_proxy_wasm_init(ngx_cycle_t *cycle)
{
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

        /*
        rc = ngx_wasm_vm_actions.call_instance(instance, "_start",
                                               NULL, 0, NULL, 0);
        if (rc != NGX_OK && rc != NGX_DONE) {
            return NGX_ERROR;
        }
        */

        ngx_wasm_vm_actions.free_instance(instance);
    }

    return NGX_OK;
}


/* vi:set ft=c ts=4 sw=4 et fdm=marker: */
