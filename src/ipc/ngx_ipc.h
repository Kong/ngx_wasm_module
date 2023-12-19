#ifndef _NGX_IPC_H_INCLUDED_
#define _NGX_IPC_H_INCLUDED_


#include <ngx_core.h>


#define NGX_IPC_MODULE              0x00495043   /* "IPC" */
#define NGX_IPC_CONF                0x00300000


typedef struct {
    ngx_int_t                    (*init)(ngx_cycle_t *cycle);
} ngx_ipc_module_t;


#endif /* _NGX_IPC_H_INCLUDED_ */
