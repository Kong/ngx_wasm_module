/*
 * Copyright (C) Thibault Charbonnier
 */

#include <nginx.h>
#include <ngx_core.h>
#include <ngx_config.h>

#include "ngx_wasm_module.h"


static void *ngx_wasm_module_create_conf(ngx_cycle_t *cycle);
static char *ngx_wasm_wasm_block(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);


static ngx_command_t ngx_wasm_cmds[] = {

    { ngx_string("wasm"),
      NGX_MAIN_CONF|NGX_CONF_BLOCK|NGX_CONF_NOARGS,
      ngx_wasm_wasm_block,
      0,
      0,
      NULL },

    ngx_null_command
};


static ngx_core_module_t  ngx_wasm_module_ctx = {
    ngx_string("wasm"),
    ngx_wasm_module_create_conf,
    NULL
};


ngx_module_t  ngx_wasm_module = {
    NGX_MODULE_V1,
    &ngx_wasm_module_ctx,              /* module context */
    ngx_wasm_cmds,                     /* module directives */
    NGX_CORE_MODULE,                   /* module type */
    NULL,                              /* init master */
    NULL,                              /* init module */
    NULL,                              /* init process */
    NULL,                              /* init thread */
    NULL,                              /* exit thread */
    NULL,                              /* exit process */
    NULL,                              /* exit master */
    NGX_MODULE_V1_PADDING
};


static void *
ngx_wasm_module_create_conf(ngx_cycle_t *cycle)
{
    ngx_wasm_conf_t         *wcf;

    wcf = ngx_pcalloc(cycle->pool, sizeof(ngx_wasm_conf_t));
    if (wcf == NULL) {
        return NULL;
    }

    return wcf;
}


static char *
ngx_wasm_wasm_block(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    char                        *rv;
    ngx_conf_t                   pcf;
    ngx_wasm_conf_t             *wcf = *(ngx_wasm_conf_t **) conf;

    if (wcf->parsed_wasm_block) {
        return "is duplicate";
    }

    /* parse the wasm{} block */

    wcf->parsed_wasm_block = 1;

    pcf = *cf;

    cf->ctx = wcf;
    cf->module_type = NGX_CORE_MODULE;
    cf->cmd_type = NGX_WASM_CONF;

    rv = ngx_conf_parse(cf, NULL);

    *cf = pcf;

    if (rv != NGX_CONF_OK) {
        return NGX_CONF_ERROR;
    }

    return NGX_CONF_OK;
}


/* vi:set ft=c ts=4 sw=4 et fdm=marker: */
