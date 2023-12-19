#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_ipc.h>


static void *ngx_ipc_core_create_conf(ngx_conf_t *cf);
static char *ngx_ipc_core_init_conf(ngx_conf_t *cf, void *conf);
static ngx_int_t ngx_ipc_core_init(ngx_cycle_t *cycle);
static ngx_int_t ngx_ipc_core_init_process(ngx_cycle_t *cycle);


static ngx_command_t  ngx_ipc_core_commands[] = {
    ngx_null_command
};


static ngx_ipc_module_t  ngx_ipc_core_module_ctx = {
    ngx_ipc_core_create_conf,              /* create configuration */
    ngx_ipc_core_init_conf,                /* init configuration */
    ngx_ipc_core_init                      /* init module */
};


ngx_module_t  ngx_ipc_core_module = {
    NGX_MODULE_V1,
    &ngx_ipc_core_module_ctx,              /* module context */
    ngx_ipc_core_commands,                 /* module directives */
    NGX_IPC_MODULE,                        /* module type */
    NULL,                                  /* init master */
    NULL,                                  /* init module */
    ngx_ipc_core_init_process,             /* init process */
    NULL,                                  /* init thread */
    NULL,                                  /* exit thread */
    NULL,                                  /* exit process */
    NULL,                                  /* exit master */
    NGX_MODULE_V1_PADDING
};


static void *
ngx_ipc_core_create_conf(ngx_conf_t *cf)
{
    ngx_cycle_t          *cycle = cf->cycle;
    ngx_ipc_core_conf_t  *icf;

    icf = ngx_pcalloc(cycle->pool, sizeof(ngx_ipc_core_conf_t));
    if (icf == NULL) {
        return NULL;
    }

    return icf;
}


static char *
ngx_ipc_core_init_conf(ngx_conf_t *cf, void *conf)
{
    return NGX_CONF_OK;
}


static ngx_int_t
ngx_ipc_core_init(ngx_cycle_t *cycle)
{
    ngx_log_debug0(NGX_LOG_DEBUG_ALL, cycle->log, 0,
                   "ipc core module initializing");
    return NGX_OK;
}


static ngx_int_t
ngx_ipc_core_init_process(ngx_cycle_t *cycle)
{
    return NGX_OK;
}
