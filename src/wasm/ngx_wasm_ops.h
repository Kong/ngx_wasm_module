#ifndef _NGX_WASM_OPS_H_INCLUDED_
#define _NGX_WASM_OPS_H_INCLUDED_


#include <ngx_wavm.h>


typedef struct ngx_wasm_op_s  ngx_wasm_op_t;
typedef struct ngx_wasm_op_ctx_s  ngx_wasm_op_ctx_t;
typedef struct ngx_wasm_ops_engine_s  ngx_wasm_ops_engine_t;


typedef ngx_int_t (*ngx_wasm_op_handler_pt)(ngx_wasm_op_ctx_t *ctx,
    ngx_wavm_instance_t *instance, ngx_wasm_phase_t *phase, ngx_wasm_op_t *op);


struct ngx_wasm_op_ctx_s {
    ngx_wasm_ops_engine_t                   *ops_engine;
    ngx_wavm_ctx_t                           wv_ctx;
    ngx_log_t                               *log;
    ngx_module_t                            *m;
};


typedef enum {
    NGX_WASM_OP_CALL = 1,
    NGX_WASM_OP_PROXY_WASM
} ngx_wasm_op_code_t;


typedef struct {
    ngx_str_t                                func_name;
    ngx_wavm_func_t                         *function;
} ngx_wasm_op_call_t;


typedef struct {
    unsigned                                 i;
} ngx_wasm_op_proxy_wasm_t;


struct ngx_wasm_op_s {
    ngx_uint_t                               on_phases;
    ngx_wasm_op_code_t                       code;
    ngx_wasm_op_handler_pt                   handler;
    ngx_wavm_module_t                       *module;

    union {
        ngx_wasm_op_call_t                   call;
        ngx_wasm_op_proxy_wasm_t             proxy_wasm;
    } conf;
};


typedef struct {
    ngx_wasm_phase_t                        *phase;
    ngx_array_t                             *ops;
} ngx_wasm_ops_pipeline_t;


struct ngx_wasm_ops_engine_s {
    ngx_pool_t                              *pool;
    ngx_wavm_t                              *vm;
    ngx_wasm_subsystem_t                    *subsystem;
    ngx_wasm_ops_pipeline_t                **pipelines;
};


ngx_wasm_ops_engine_t *ngx_wasm_ops_engine_new(ngx_pool_t *pool,
    ngx_wavm_t *vm, ngx_wasm_subsystem_t *subsystem);

ngx_wasm_op_t *ngx_wasm_conf_add_op_call(ngx_conf_t *cf,
    ngx_wasm_ops_engine_t *ops_engine, ngx_str_t phase_name,
    ngx_str_t mod_name, ngx_str_t func_name);

ngx_wasm_op_t *ngx_wasm_conf_add_op_proxy_wasm(ngx_conf_t *cf,
    ngx_wasm_ops_engine_t *ops_engine, ngx_str_t mod_name);

ngx_int_t ngx_wasm_ops_resume(ngx_wasm_op_ctx_t *ctx, ngx_uint_t phaseidx);


#endif /* _NGX_WASM_OPS_H_INCLUDED_ */
