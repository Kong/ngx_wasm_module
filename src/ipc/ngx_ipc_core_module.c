#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_ipc.h>


static ngx_int_t ngx_ipc_core_init(ngx_cycle_t *cycle);
static ngx_int_t ngx_ipc_core_init_process(ngx_cycle_t *cycle);


static ngx_command_t  ngx_ipc_core_commands[] = {
    ngx_null_command
};


static ngx_ipc_module_t  ngx_ipc_core_module_ctx = {
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


static ngx_int_t
ngx_ipc_core_init(ngx_cycle_t *cycle)
{
    return NGX_OK;
}


static ngx_int_t
ngx_ipc_core_init_process(ngx_cycle_t *cycle)
{
    return NGX_OK;
}
