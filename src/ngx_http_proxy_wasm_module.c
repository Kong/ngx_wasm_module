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


#define NGX_WASM_PROXY_WASM_ABI_VERSION  0.0.0


#define ngx_http_proxy_wasm_cycle_get_conf(cycle)                            \
    (*(ngx_get_conf(cycle->conf_ctx, ngx_http_module)))                      \
    [ngx_http_proxy_wasm_module.ctx_index]


static void       *ngx_http_proxy_wasm_create_conf(ngx_conf_t *cf);
static ngx_int_t   ngx_http_proxy_wasm_init(ngx_cycle_t *cycle);


typedef struct {

} ngx_http_proxy_wasm_main_conf_t;


static ngx_http_module_t  ngx_http_proxy_wasm_module_ctx = {
    NULL,      /* preconfiguration */
    NULL,               /* postconfiguration */

    ngx_http_proxy_wasm_create_conf,       /* create main configuration */
    NULL,                                  /* init main configuration */

    NULL,                                  /* create server configuration */
    NULL,                                  /* merge server configuration */

    NULL,                                  /* create location configuration */
    NULL                /* merge location configuration */
};


ngx_module_t  ngx_http_proxy_wasm_module = {
    NGX_MODULE_V1,
    &ngx_http_proxy_wasm_module_ctx,     /* module context */
    NULL,                                /* module directives */
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
ngx_http_proxy_wasm_create_conf(ngx_conf_t *cf)
{
    ngx_http_proxy_wasm_main_conf_t    *pwcf;

    pwcf = ngx_pcalloc(cf->pool, sizeof(ngx_http_proxy_wasm_main_conf_t));
    if (pwcf == NULL) {
        return NULL;
    }

    return pwcf;
}


static ngx_int_t
ngx_http_proxy_wasm_init(ngx_cycle_t *cycle)
{
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
}


/* vi:set ft=c ts=4 sw=4 et fdm=marker: */
