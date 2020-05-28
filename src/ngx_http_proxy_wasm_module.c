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


#define NGX_HTTP_PROXY_WASM_ABI_VERSION  0.1.0

static void       *ngx_http_proxy_wasm_create_main_conf(ngx_conf_t *cf);
static void       *ngx_http_proxy_wasm_create_srv_conf(ngx_conf_t *cf);
static void       *ngx_http_proxy_wasm_create_loc_conf(ngx_conf_t *cf);
static ngx_int_t   ngx_http_proxy_wasm_preconfig_init(ngx_conf_t *cf);
static char       *ngx_http_proxy_wasm_module_directive(ngx_conf_t *cf,
                                                        ngx_command_t *cmd,
                                                        void *conf);
static ngx_int_t   ngx_http_proxy_wasm_init(ngx_cycle_t *cycle);


typedef struct {
    ngx_wasm_wvm_t          *vm;
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
    ngx_http_proxy_wasm_preconfig_init,    /* preconfiguration */
    NULL,                                  /* postconfiguration */

    ngx_http_proxy_wasm_create_main_conf,  /* create main configuration */
    NULL,                                  /* init main configuration */

    ngx_http_proxy_wasm_create_srv_conf,   /* create server configuration */
    NULL,                                  /* merge server configuration */

    ngx_http_proxy_wasm_create_loc_conf,   /* create location configuration */
    NULL                                   /* merge location configuration */
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

    return pwmcf;
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

    return pwlcf;
}


static ngx_int_t
ngx_http_proxy_wasm_preconfig_init(ngx_conf_t *cf)
{
    ngx_http_proxy_wasm_main_conf_t    *pwmcf;

    pwmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_proxy_wasm_module);

    pwmcf->vm = ngx_palloc(cf->pool, sizeof(ngx_wasm_wvm_t));
    if (pwmcf->vm == NULL) {
        return NGX_ERROR;
    }

    if (ngx_wasm_vm_actions.init_vm(pwmcf->vm, cf->cycle) != NGX_OK) {
        return NGX_ERROR;
    }

    return NGX_OK;
}


static char *
ngx_http_proxy_wasm_module_directive(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf)
{
    ngx_http_proxy_wasm_loc_conf_t     *pwlcf = conf;
    ngx_http_proxy_wasm_main_conf_t    *pwmcf;
    ngx_str_t                          *value;
    ngx_wasm_winstance_t                instance;

    pwmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_proxy_wasm_module);

    if (pwlcf->module) {
        return "is duplicate";
    }

    pwlcf->module = ngx_palloc(cf->pool, sizeof(ngx_wasm_wmodule_t));
    if (pwlcf->module == NULL) {
        return NGX_CONF_ERROR;
    }

    value = cf->args->elts;

    if (ngx_wasm_vm_actions.load_wasm_module(pwlcf->module, pwmcf->vm,
                                             value[1].data)
        != NGX_OK)
    {
        return NGX_CONF_ERROR;
    }

    if (ngx_wasm_vm_actions.init_instance(&instance, pwlcf->module)
        != NGX_OK)
    {
        return NGX_CONF_ERROR;
    }

    if (ngx_wasm_vm_actions.run_entrypoint(&instance, (u_char *) "_start")
        != NGX_OK)
    {
        return NGX_CONF_ERROR;
    }

    ngx_wasm_vm_actions.shutdown_instance(&instance);

    return NGX_CONF_OK;
}


static ngx_int_t
ngx_http_proxy_wasm_init(ngx_cycle_t *cycle)
{
    /*
    ngx_wasm_wvm_t       *vm;
    ngx_wasm_wmodule_t   *module;
    ngx_wasm_winstance_t *instance;

    vm = ngx_palloc(cycle->pool, sizeof(ngx_wasm_wvm_t));
    if (vm == NULL) {
        goto failed;
    }

    module = ngx_palloc(cycle->pool, sizeof(ngx_wasm_wmodule_t));
    if (module == NULL) {
        goto failed;
    }

    instance = ngx_palloc(cycle->pool, sizeof(ngx_wasm_winstance_t));
    if (instance == NULL) {
        goto failed;
    }

    if (ngx_wasm_vm_actions.init_vm(vm, cycle) != NGX_OK) {
        goto failed;
    }

    if (ngx_wasm_vm_actions.load_wasm_module(module, vm,
            (u_char *) "/home/chasum/code/wasm/hello-world/target/"
                       "wasm32-unknown-unknown/debug/hello.wasm")
        != NGX_OK)
    {
        goto failed;
    }

    if (ngx_wasm_vm_actions.init_instance(instance, module) != NGX_OK) {
        goto failed;
    }

    if (ngx_wasm_vm_actions.run_entrypoint(instance, (u_char *) "_start")
        != NGX_OK)
    {
        goto failed;
    }

    return NGX_OK;

failed:

    if (vm != NULL) {
        ngx_wasm_vm_actions.shutdown_vm(vm);
        ngx_pfree(cycle->pool, vm);
    }

    if (module != NULL) {
        ngx_pfree(cycle->pool, module);
    }

    if (instance != NULL) {
        ngx_pfree(cycle->pool, instance);
    }

    return NGX_ERROR;
    */
    return NGX_OK;
}


/* vi:set ft=c ts=4 sw=4 et fdm=marker: */
