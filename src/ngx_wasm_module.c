/*
 * Copyright (C) Thibault Charbonnier
 */

#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_config.h>

#include "ngx_wasm_module.h"
#include "ngx_wasm_wasmtime.h"


static void *ngx_wasm_module_create_conf(ngx_cycle_t *cycle);
static char *ngx_wasm_wasm_block(ngx_conf_t *cf, ngx_command_t *cmd,
                                 void *conf);

static ngx_int_t ngx_wasm_module_init(ngx_cycle_t *cycle);


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
    ngx_wasm_module_init,              /* init module */
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
    ngx_wasm_conf_t         *nwcf;

    nwcf = ngx_pcalloc(cycle->pool, sizeof(ngx_wasm_conf_t));
    if (nwcf == NULL) {
        return NULL;
    }

    return nwcf;
}


static char *
ngx_wasm_wasm_block(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    char                        *rv;
    ngx_conf_t                   pcf;
    ngx_wasm_conf_t             *nwcf = *(ngx_wasm_conf_t **) conf;

    if (nwcf->parsed_wasm_block) {
        return "is duplicate";
    }

    /* parse the wasm{} block */

    nwcf->parsed_wasm_block = 1;

    pcf = *cf;

    cf->ctx = nwcf;
    cf->module_type = NGX_CORE_MODULE;
    cf->cmd_type = NGX_WASM_CONF;

    rv = ngx_conf_parse(cf, NULL);

    *cf = pcf;

    if (rv != NGX_CONF_OK) {
        return NGX_CONF_ERROR;
    }

    return NGX_CONF_OK;
}


static ngx_int_t
ngx_wasm_module_init(ngx_cycle_t *cycle)
{
    ngx_wasm_conf_t             *nwcf;
    wasm_module_t               *wmodule;
    wasm_instance_t             *winst;
    ngx_int_t                    rc = NGX_ERROR;

    ngx_str_t                    filename = ngx_string("/home/chasum/code/wasm/hello-world/target/wasm32-wasi/debug/hello-world.wasm");

    wasm_trap_t                 *wtrap = NULL;
    wasmtime_error_t            *werror;

    wasm_extern_vec_t            wasm_externs;
    wasm_exporttype_vec_t        wasm_exports;
    wasm_extern_t               *start_extern = NULL;
    wasm_func_t                 *start = NULL;

    nwcf = ngx_wasm_cycle_get_main_conf(cycle);

    if (ngx_wasmtime_init_engine(nwcf, cycle->log) != NGX_OK) {
        goto failed;
    }

    if (ngx_wasmtime_load_module(&wmodule, nwcf->wstore, filename, cycle->log)
        != NGX_OK)
    {
        goto failed;
    }

    if (ngx_wasmtime_load_instance(&winst, nwcf->wasi_inst, wmodule, cycle->log)
        != NGX_OK)
    {
        goto failed;
    }

    /* load & run _start function */

    wasm_instance_exports(winst, &wasm_externs);
    wasm_module_exports(wmodule, &wasm_exports);

    for (size_t i = 0; i < wasm_exports.size; i++) {
        const wasm_name_t *name = wasm_exporttype_name(wasm_exports.data[i]);
        if (ngx_strncmp(name->data, "_start", name->size) == 0) {
            start_extern = wasm_externs.data[i];
        }
    }

    if (start_extern == NULL) {
        ngx_log_error(NGX_LOG_ALERT, cycle->log, 0,
                      "failed to find _start function");
        goto failed;
    }

    start = wasm_extern_as_func(start_extern);
    if (start == NULL) {
        ngx_log_error(NGX_LOG_ALERT, cycle->log, 0,
                      "failed to load _start function");
        goto failed;
    }

    werror = wasmtime_func_call(start, NULL, 0, NULL, 0, &wtrap);
    if (werror != NULL || wtrap != NULL) {
        ngx_wasmtime_log_error(NGX_LOG_ALERT, cycle->log, werror, wtrap,
                               "failed to call _start function");
        goto failed;
    }

    rc = NGX_OK;

failed:

    if (start != NULL) {
        //free(start);
        wasm_func_delete(start);
    }

    if (start_extern != NULL) {
        //free(start_extern);
        //wasm_extern_delete(start_extern);
    }

    if (winst != NULL) {
        wasm_instance_delete(winst);
    }

    if (wmodule != NULL) {
        wasm_module_delete(wmodule);
    }

    ngx_wasmtime_shutdown_engine(nwcf);

    return rc;
}


/* vi:set ft=c ts=4 sw=4 et fdm=marker: */
