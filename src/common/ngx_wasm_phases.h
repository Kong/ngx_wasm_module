#ifndef _NGX_WASM_PHASES_H_INCLUDED_
#define _NGX_WASM_PHASES_H_INCLUDED_


#include <ngx_core.h>
#include <ngx_wasm_vm.h>
#include <ngx_wasm_vm_cache.h>


typedef struct ngx_wasm_phases_op_s  ngx_wasm_phases_op_t;
typedef struct ngx_wasm_phases_ctx_s  ngx_wasm_phases_ctx_t;


typedef struct {
    ngx_str_t                                name;
    ngx_int_t                                index;
    ngx_int_t                                on;
} ngx_wasm_phase_t;


typedef ngx_int_t (*ngx_wasm_phases_op_handler_pt)(
    ngx_wasm_phases_ctx_t *pctx, ngx_wasm_phase_t *subsys_phase,
    ngx_wasm_phases_op_t *op);


typedef enum {
    NGX_WASM_PHASES_OP_CALL = 1,
    NGX_WASM_PHASES_OP_PROXY_WASM
} ngx_wasm_phases_op_code_t;


typedef struct {
   ngx_str_t                                 mod_name;
   ngx_str_t                                 func_name;
} ngx_wasm_phases_op_call_conf_t;


typedef struct {
   ngx_str_t                                 mod_name;
} ngx_wasm_phases_op_proxy_wasm_conf_t;


struct ngx_wasm_phases_op_s {
    ngx_uint_t                               on_phases;
    ngx_wasm_phases_op_code_t                code;
    ngx_wasm_phases_op_handler_pt            handler;

   union {
       ngx_wasm_phases_op_call_conf_t        call;
       ngx_wasm_phases_op_proxy_wasm_conf_t  proxy_wasm;
   } conf;
};


typedef struct {
    ngx_wasm_phase_t                        *subsys_phase;
    ngx_array_t                             *ops;
} ngx_wasm_phases_phase_t;


typedef struct {
    ngx_uint_t                               nphases;
    ngx_wasm_hsubsys_kind                    subsys;
    ngx_wasm_vm_t                           *vm;
    ngx_wasm_phases_phase_t                **phases;
} ngx_wasm_phases_engine_t;


struct ngx_wasm_phases_ctx_s {
    ngx_pool_t                              *pool;
    ngx_log_t                               *log;
    ngx_wasm_phases_engine_t                *phengine;
    ngx_wasm_vm_cache_t                     *vmcache;
    void                                    *data;
};


ngx_wasm_phases_op_t *ngx_wasm_phases_conf_add_op_call(ngx_conf_t *cf,
    ngx_wasm_phases_engine_t *phengine, ngx_str_t *phase_name,
    ngx_str_t *mod_name, ngx_str_t *func_name);

ngx_wasm_phases_op_t *ngx_wasm_phases_conf_add_op_proxy_wasm(ngx_conf_t *cf,
    ngx_wasm_phases_engine_t *phengine, ngx_str_t *mod_name);

ngx_int_t ngx_wasm_phases_resume(ngx_wasm_phases_ctx_t *pctx,
    ngx_int_t phase_idx);


#endif /* _NGX_WASM_PHASES_H_INCLUDED_ */
