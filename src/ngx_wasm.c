/*
 * Copyright (C) Thibault Charbonnier
 */

#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_config.h>
#include <ngx_wasm.h>


#define ngx_wasm_core_cycle_get_conf(cycle)                             \
    (*(ngx_get_conf(cycle->conf_ctx, ngx_wasm_module)))                 \
    [ngx_wasm_core_module.ctx_index]


static char      *ngx_wasm_block(ngx_conf_t *cf, ngx_command_t *cmd,
                                 void *conf);
static void      *ngx_wasm_core_create_conf(ngx_cycle_t *cycle);
static char      *ngx_wasm_core_init_conf(ngx_cycle_t *cycle, void *conf);
static char      *ngx_wasm_core_vm_directive(ngx_conf_t *cf,
                                             ngx_command_t *cmd, void *conf);
static ngx_int_t  ngx_wasm_core_init(ngx_cycle_t *cycle);


static ngx_command_t  ngx_wasm_cmds[] = {

    { ngx_string("wasm"),
      NGX_MAIN_CONF|NGX_CONF_BLOCK|NGX_CONF_NOARGS,
      ngx_wasm_block,
      0,
      0,
      NULL },

    ngx_null_command
};


static ngx_core_module_t  ngx_wasm_module_ctx = {
    ngx_string("wasm"),
    NULL,
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


static ngx_str_t      wasm_core_name = ngx_string("wasm_core");
static ngx_uint_t     ngx_wasm_max_module;
ngx_wasm_actions_t    ngx_wasm_actions;


static ngx_command_t  ngx_wasm_core_commands[] = {

    { ngx_string("vm"),
      NGX_WASM_CONF|NGX_CONF_TAKE1,
      ngx_wasm_core_vm_directive,
      0,
      0,
      NULL },

      ngx_null_command
};


static ngx_wasm_module_t  ngx_wasm_core_module_ctx = {
    &wasm_core_name,
    ngx_wasm_core_create_conf,             /* create configuration */
    ngx_wasm_core_init_conf,               /* init configuration */
    NULL,                                  /* init module */

    { NULL, NULL, NULL, NULL, NULL, NULL, NULL }
};


ngx_module_t  ngx_wasm_core_module = {
    NGX_MODULE_V1,
    &ngx_wasm_core_module_ctx,             /* module context */
    ngx_wasm_core_commands,                /* module directives */
    NGX_WASM_MODULE,                       /* module type */
    NULL,                                  /* init master */
    ngx_wasm_core_init,                    /* init module */
    NULL,                                  /* init process */
    NULL,                                  /* init thread */
    NULL,                                  /* exit thread */
    NULL,                                  /* exit process */
    NULL,                                  /* exit master */
    NGX_MODULE_V1_PADDING
};


static char *
ngx_wasm_block(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    char                        *rv;
    void                      ***ctx;
    ngx_uint_t                   i;
    ngx_conf_t                   pcf;
    ngx_wasm_module_t           *m;

    if (*(void **) conf) {
        return "is duplicate";
    }

    ngx_wasm_max_module = ngx_count_modules(cf->cycle, NGX_WASM_MODULE);

    ctx = ngx_pcalloc(cf->pool, sizeof(void *));
    if (ctx == NULL) {
        return NGX_CONF_ERROR;
    }

    *ctx = ngx_pcalloc(cf->pool, ngx_wasm_max_module * sizeof(void *));
    if (*ctx == NULL) {
        return NGX_CONF_ERROR;
    }

    *(void **) conf = ctx;

    /* NGX_WASM_MODULES create_conf */

    for (i = 0; cf->cycle->modules[i]; i++) {
        if (cf->cycle->modules[i]->type != NGX_WASM_MODULE) {
            continue;
        }

        m = cf->cycle->modules[i]->ctx;

        if (m->create_conf) {
            (*ctx)[cf->cycle->modules[i]->ctx_index] =
                                                     m->create_conf(cf->cycle);
            if ((*ctx)[cf->cycle->modules[i]->ctx_index] == NULL) {
                return NGX_CONF_ERROR;
            }
        }
    }

    /* parse the wasm{} block */

    pcf = *cf;

    cf->ctx = ctx;
    cf->module_type = NGX_WASM_MODULE;
    cf->cmd_type = NGX_WASM_CONF;

    rv = ngx_conf_parse(cf, NULL);

    *cf = pcf;

    if (rv != NGX_CONF_OK) {
        return NGX_CONF_ERROR;
    }

    /* NGX_WASM_MODULES init_conf */

    for (i = 0; cf->cycle->modules[i]; i++) {
        if (cf->cycle->modules[i]->type != NGX_WASM_MODULE) {
            continue;
        }

        m = cf->cycle->modules[i]->ctx;

        if (m->init_conf) {
            rv = m->init_conf(cf->cycle,
                              (*ctx)[cf->cycle->modules[i]->ctx_index]);
            if (rv != NGX_CONF_OK) {
                return rv;
            }
        }
    }

    return NGX_CONF_OK;
}


static char *
ngx_wasm_core_vm_directive(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_wasm_core_conf_t    *wcf = conf;
    ngx_wasm_module_t       *wasm_module;
    ngx_str_t               *value;
    ngx_uint_t               i;

    if (wcf->vm != NGX_CONF_UNSET_UINT) {
        return "is duplicate";
    }

    value = cf->args->elts;

    for (i = 0; cf->cycle->modules[i]; i++) {
        if (cf->cycle->modules[i]->type != NGX_WASM_MODULE) {
            continue;
        }

        wasm_module = cf->cycle->modules[i]->ctx;
        if (wasm_module->name->len == value[1].len
            && ngx_strcmp(wasm_module->name->data, value[1].data) == 0)
        {
            wcf->vm = cf->cycle->modules[i]->ctx_index;
            wcf->vm_name = wasm_module->name->data;

            return NGX_CONF_OK;
        }
    }

    ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                       "invalid wasm VM \"%V\"", &value[1]);

    return NGX_CONF_ERROR;
}


static void *
ngx_wasm_core_create_conf(ngx_cycle_t *cycle)
{
    ngx_wasm_core_conf_t    *wcf;

    wcf = ngx_pcalloc(cycle->pool, sizeof(ngx_wasm_core_conf_t));
    if (wcf == NULL) {
        return NULL;
    }

    wcf->vm = NGX_CONF_UNSET_UINT;
    wcf->vm_name = (void *) NGX_CONF_UNSET;

    return wcf;
}


static char *
ngx_wasm_core_init_conf(ngx_cycle_t *cycle, void *conf)
{
    ngx_wasm_core_conf_t    *wcf = conf;
    ngx_uint_t               i;
    ngx_module_t            *module;
    ngx_wasm_module_t       *wasm_module;

    for (i = 0; cycle->modules[i]; i++) {
        if (cycle->modules[i]->type != NGX_WASM_MODULE) {
            continue;
        }

        wasm_module = cycle->modules[i]->ctx;

        if (ngx_strcmp(wasm_module->name->data, wasm_core_name.data) == 0) {
            continue;
        }

        module = cycle->modules[i];
        break;
    }

    if (module == NULL) {
        ngx_log_error(NGX_LOG_EMERG, cycle->log, 0, "no wasm module found");
        return NGX_CONF_ERROR;
    }

    if (wcf->vm == NGX_CONF_UNSET_UINT) {
        ngx_log_error(NGX_LOG_EMERG, cycle->log, 0,
                      "missing \"vm\" directive in wasm configuration");
        return NGX_CONF_ERROR;
    }

    return NGX_CONF_OK;
}


static ngx_int_t
ngx_wasm_core_init(ngx_cycle_t *cycle)
{
    ngx_int_t                    rv;
    ngx_uint_t                   i;
    ngx_wasm_module_t           *m;
    ngx_wasm_core_conf_t        *wcf;

    wcf = ngx_wasm_core_cycle_get_conf(cycle);

    ngx_log_error(NGX_LOG_NOTICE, cycle->log, 0,
                  "initializing wasm (VM=%s)", wcf->vm_name);

    /* NGX_WASM_MODULES init */

    for (i = 0; cycle->modules[i]; i++) {
        if (cycle->modules[i]->type != NGX_WASM_MODULE
            || cycle->modules[i]->ctx_index != wcf->vm)
        {
            continue;
        }

        m = cycle->modules[i]->ctx;

        if (m->init) {
            rv = m->init(cycle);
            if (rv != NGX_OK) {
                return rv;
            }
        }
    }

    if (ngx_wasm_actions.init_engine(cycle) != NGX_OK) {
        return NGX_ERROR;
    }

    if (ngx_wasm_actions.init_store(cycle) != NGX_OK) {
        return NGX_ERROR;
    }

    if (ngx_wasm_actions.load_wasm_module(cycle,
            (u_char *) "/home/chasum/code/wasm/hello-world/target/"
                       "wasm32-wasi/debug/hello-world.wasm")
        != NGX_OK)
    {
        return NGX_ERROR;
    }

    if (ngx_wasm_actions.init_instance(cycle) != NGX_OK) {
        return NGX_ERROR;
    }

    if (ngx_wasm_actions.run_entrypoint(cycle, (u_char *) "_start")
        != NGX_OK)
    {
        return NGX_ERROR;
    }

    return NGX_OK;
}


/* vi:set ft=c ts=4 sw=4 et fdm=marker: */
