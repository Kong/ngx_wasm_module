#ifndef _NGX_WASM_OPS_H_INCLUDED_
#define _NGX_WASM_OPS_H_INCLUDED_


#include <ngx_wavm.h>
#include <ngx_proxy_wasm.h>


typedef struct ngx_wasm_op_s  ngx_wasm_op_t;
typedef struct ngx_wasm_op_ctx_s  ngx_wasm_op_ctx_t;
typedef struct ngx_wasm_ops_engine_s  ngx_wasm_ops_engine_t;


typedef ngx_int_t (*ngx_wasm_op_handler_pt)(ngx_wasm_op_ctx_t *ctx,
    ngx_wasm_phase_t *phase, ngx_wasm_op_t *op);


struct ngx_wasm_op_ctx_s {
    ngx_log_t                               *log;
    ngx_wasm_ops_engine_t                   *ops_engine;
    ngx_wavm_ctx_t                           wvctx;
};


typedef enum {
    NGX_WASM_OPS_RUN_ALL = 1,
} ngx_wasm_ops_resume_mode_e;


typedef enum {
    NGX_WASM_OP_CALL = 1,
    NGX_WASM_OP_PROXY_WASM
} ngx_wasm_op_code_t;


typedef struct {
    ngx_str_t                                func_name;
    ngx_wavm_funcref_t                      *funcref;
} ngx_wasm_op_call_t;


typedef struct {
    ngx_proxy_wasm_t                        *pwmodule;
} ngx_wasm_op_proxy_wasm_t;


struct ngx_wasm_op_s {
    ngx_uint_t                               on_phases;
    ngx_uint_t                               code;
    ngx_wasm_op_handler_pt                   handler;
    ngx_wavm_host_def_t                     *host;
    ngx_wavm_module_t                       *module;
    ngx_wavm_linked_module_t                *lmodule;

    union {
        ngx_wasm_op_call_t                   call;
        ngx_wasm_op_proxy_wasm_t             proxy_wasm;
    } conf;
};


typedef struct {
    ngx_wasm_phase_t                        *phase;
    ngx_array_t                             *ops;
    ngx_uint_t                               nproxy_wasm;
    unsigned                                 init:1;
} ngx_wasm_ops_pipeline_t;


struct ngx_wasm_ops_engine_s {
    ngx_queue_t                              q;           /* main_conf_t->ops_engines */
    ngx_pool_t                              *pool;
    ngx_wavm_t                              *vm;
    ngx_wasm_subsystem_t                    *subsystem;
    ngx_wasm_ops_pipeline_t                **pipelines;
};


ngx_wasm_ops_engine_t *ngx_wasm_ops_engine_new(ngx_pool_t *pool,
    ngx_wavm_t *vm, ngx_wasm_subsystem_t *subsystem);
void ngx_wasm_ops_engine_init(ngx_wasm_ops_engine_t *engine);
void ngx_wasm_ops_engine_destroy(ngx_wasm_ops_engine_t *engine);
ngx_int_t ngx_wasm_ops_add(ngx_wasm_ops_engine_t *ops_engine,
    ngx_wasm_op_t *op);

ngx_int_t ngx_wasm_ops_resume(ngx_wasm_op_ctx_t *ctx, ngx_uint_t phaseidx,
    ngx_uint_t modes);


#endif /* _NGX_WASM_OPS_H_INCLUDED_ */
