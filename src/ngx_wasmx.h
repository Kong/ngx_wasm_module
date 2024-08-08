#ifndef _NGX_WA_H_INCLUDED_
#define _NGX_WA_H_INCLUDED_


#include <ngx_core.h>
#include <ngx_wasm_shm.h>
#include <ngx_wa_metrics.h>


#if (NGX_DEBUG)
#include <assert.h>
#   define ngx_wa_assert(a)         assert(a)
#else
#   define ngx_wa_assert(a)
#endif

#define NGX_WA_BAD_FD               (ngx_socket_t) -1

#define NGX_WA_WASM_CONF_OFFSET     offsetof(ngx_wa_conf_t, wasm_confs)
#define NGX_WA_IPC_CONF_OFFSET      offsetof(ngx_wa_conf_t, ipc_confs)

#define NGX_WA_CONF_ERR_DUPLICATE   "is duplicate"

#define NGX_WA_METRICS_DEFAULT_MAX_NAME_LEN  256
#define NGX_WA_METRICS_DEFAULT_SLAB_SIZE     1024 * 1024 * 5   /* 5 MiB */

#define ngx_wa_cycle_get_conf(cycle)                                         \
    (ngx_wa_conf_t *) ngx_get_conf(cycle->conf_ctx, ngx_wasmx_module)


typedef void *(*ngx_wa_create_conf_pt)(ngx_conf_t *cf);
typedef char *(*ngx_wa_init_conf_pt)(ngx_conf_t *cf, void *conf);
typedef ngx_int_t (*ngx_wa_init_pt)(ngx_cycle_t *cycle);


typedef struct {
    ngx_uint_t               initialized_types;
    void                   **wasm_confs;
#ifdef NGX_WA_IPC
    void                   **ipc_confs;
#endif
    ngx_array_t              shms;     /* ngx_wasm_shm_mapping_t */
    ngx_wa_metrics_t        *metrics;
} ngx_wa_conf_t;


ngx_array_t *ngx_wasmx_shms(ngx_cycle_t *cycle);
ngx_wa_metrics_t *ngx_wasmx_metrics(ngx_cycle_t *cycle);


extern ngx_module_t  ngx_wasmx_module;

extern ngx_uint_t    ngx_wasm_max_module;
extern ngx_uint_t    ngx_ipc_max_module;


#endif /* _NGX_WA_H_INCLUDED_ */
