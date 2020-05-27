/*
 * Copyright (C) Thibault Charbonnier
 */

#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_config.h>
#include <ngx_wasm.h>


#define NGX_WASM_PROXY_WASM_ABI_VERSION  0.0.0


#define ngx_wasm_proxy_wasm_cycle_get_conf(cycle)                            \
    (*(ngx_get_conf(cycle->conf_ctx, ngx_wasm_module)))                      \
    [ngx_wasm_proxy_wasm_module.ctx_index]


static void       *ngx_wasm_proxy_wasm_create_conf(ngx_cycle_t *cycle);
static ngx_int_t   ngx_wasm_proxy_wasm_init(ngx_cycle_t *cycle);


static ngx_str_t   module_name = ngx_string("proxy_wasm");


typedef struct {

} ngx_wasm_proxy_wasm_conf_t;


static ngx_wasm_module_t  ngx_wasm_proxy_wasm_module_ctx = {
    NGX_WASM_MODULE_SPEC,
    &module_name,
    ngx_wasm_proxy_wasm_create_conf,
    NULL,
    ngx_wasm_proxy_wasm_init,
    NGX_WASM_NO_VM_ACTIONS
};


ngx_module_t  ngx_wasm_proxy_wasm_module = {
    NGX_MODULE_V1,
    &ngx_wasm_proxy_wasm_module_ctx,     /* module context */
    NULL,                                /* module directives */
    NGX_WASM_MODULE,                     /* module type */
    NULL,                                /* init master */
    NULL,                                /* init module */
    NULL,                                /* init process */
    NULL,                                /* init thread */
    NULL,                                /* exit thread */
    NULL,                                /* exit process */
    NULL,                                /* exit master */
    NGX_MODULE_V1_PADDING
};


static void *
ngx_wasm_proxy_wasm_create_conf(ngx_cycle_t *cycle)
{
    ngx_wasm_proxy_wasm_conf_t    *pwcf;

    pwcf = ngx_pcalloc(cycle->pool, sizeof(ngx_wasm_proxy_wasm_conf_t));
    if (pwcf == NULL) {
        return NULL;
    }

    return pwcf;
}


static ngx_int_t
ngx_wasm_proxy_wasm_init(ngx_cycle_t *cycle)
{
    return NGX_OK;
}


/* vi:set ft=c ts=4 sw=4 et fdm=marker: */
