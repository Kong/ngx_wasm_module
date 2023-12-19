#ifndef _NGX_WA_H_INCLUDED_
#define _NGX_WA_H_INCLUDED_


#include <ngx_core.h>


#if (NGX_DEBUG)
#include <assert.h>
#   define ngx_wa_assert(a)        assert(a)
#else
#   define ngx_wa_assert(a)
#endif

#define NGX_WA_BAD_FD              (ngx_socket_t) -1


typedef void *(*ngx_wa_create_conf_pt)(ngx_conf_t *cf);
typedef char *(*ngx_wa_init_conf_pt)(ngx_conf_t *cf, void *conf);
typedef ngx_int_t (*ngx_wa_init_pt)(ngx_cycle_t *cycle);


typedef struct {
    ngx_uint_t              initialized_types;
} ngx_wa_conf_t;


extern ngx_module_t  ngx_wasmx_module;

extern ngx_uint_t    ngx_wasm_max_module;
extern ngx_uint_t    ngx_ipc_max_module;


#endif /* _NGX_WA_H_INCLUDED_ */
