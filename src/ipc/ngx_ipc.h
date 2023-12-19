#ifndef _NGX_IPC_H_INCLUDED_
#define _NGX_IPC_H_INCLUDED_


#include <ngx_wasmx.h>


#define NGX_IPC_MODULE              0x00495043   /* "IPC" */
#define NGX_IPC_CONF                0x00400000

#define ngx_ipc_core_cycle_get_conf(cycle)                                   \
    (ngx_get_conf(cycle->conf_ctx, ngx_wasmx_module))                        \
    ? (*(ngx_get_conf(cycle->conf_ctx, ngx_wasmx_module)))                   \
      [ngx_ipc_core_module.ctx_index]                                        \
    : NULL


typedef struct {
    ngx_wa_create_conf_pt        create_conf;
    ngx_wa_init_conf_pt          init_conf;
    ngx_wa_init_pt               init;
} ngx_ipc_module_t;


typedef struct {
    void                        *p;  /* stub */
} ngx_ipc_core_conf_t;


extern ngx_module_t  ngx_ipc_core_module;


#endif /* _NGX_IPC_H_INCLUDED_ */
