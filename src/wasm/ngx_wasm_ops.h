#ifndef _NGX_WASM_OPS_H_INCLUDED_
#define _NGX_WASM_OPS_H_INCLUDED_


#include <ngx_wavm.h>
#include <ngx_proxy_wasm.h>


typedef struct ngx_wasm_op_ctx_s  ngx_wasm_op_ctx_t;
typedef struct ngx_wasm_ops_s  ngx_wasm_ops_t;
typedef struct ngx_wasm_op_s  ngx_wasm_op_t;


typedef ngx_int_t (*ngx_wasm_op_handler_pt)(ngx_wasm_op_ctx_t *ctx,
    ngx_wasm_phase_t *phase, ngx_wasm_op_t *op);


typedef enum {
    NGX_WASM_OPS_RUN_ALL = 1,
} ngx_wasm_ops_resume_mode_e;


typedef enum {
    NGX_WASM_OP_CALL = 1,
    NGX_WASM_OP_PROXY_WASM
} ngx_wasm_op_code_t;


typedef struct {
    ngx_proxy_wasm_filter_t                 *filter;
} ngx_wasm_op_proxy_wasm_t;


typedef struct {
    ngx_uint_t                               isolation;
    unsigned                                 req_headers_in_access:1;
} ngx_wasm_op_proxy_wasm_ctx_t;


typedef struct {
    ngx_uint_t                               idx;
    ngx_str_t                                func_name;
    ngx_wavm_funcref_t                      *funcref;
} ngx_wasm_op_call_t;


typedef struct {
    void                                    *nop;
} ngx_wasm_op_call_ctx_t;


struct ngx_wasm_op_s {
    ngx_uint_t                               on_phases;
    ngx_uint_t                               code;
    ngx_wasm_op_handler_pt                   handler;
    ngx_wavm_host_def_t                     *host;
    ngx_wavm_module_t                       *module;

    union {
        ngx_wasm_op_call_t                   call;
        ngx_wasm_op_proxy_wasm_t             proxy_wasm;
    } conf;
};


typedef struct {
    ngx_array_t                              ops;
} ngx_wasm_ops_pipeline_t;


typedef struct {
    void                                    *nop;
} ngx_wasm_ops_plan_call_t;


typedef struct {
    ngx_array_t                              filter_ids;
    ngx_proxy_wasm_filters_root_t           *pwroot;
    ngx_proxy_wasm_filters_root_t           *worker_pwroot;  /* &mcf->pwroot */
} ngx_wasm_ops_plan_proxy_wasm_t;


typedef struct {
    ngx_wasm_ops_t                          *ops;
    ngx_pool_t                              *pool;
    ngx_wasm_subsystem_t                    *subsystem;
    ngx_wasm_ops_pipeline_t                 *pipelines;

    union {
        ngx_wasm_ops_plan_call_t             call;
        ngx_wasm_ops_plan_proxy_wasm_t       proxy_wasm;
    } conf;

    unsigned                                 populated:1;
    unsigned                                 loaded:1;
} ngx_wasm_ops_plan_t;


struct ngx_wasm_op_ctx_s {
    ngx_pool_t                              *pool;
    ngx_log_t                               *log;
    ngx_wasm_subsys_env_t                   *env;
    ngx_wasm_ops_t                          *ops;
    ngx_wasm_ops_plan_t                     *plan;
    ngx_wasm_phase_t                        *last_phase;
    void                                    *data;
    ngx_uint_t                               cur_idx;

    union {
        ngx_wasm_op_call_ctx_t               call;
        ngx_wasm_op_proxy_wasm_ctx_t         proxy_wasm;
    } ctx;
};


struct ngx_wasm_ops_s {
    ngx_wasm_subsystem_t                    *subsystem;
    ngx_pool_t                              *pool;
    ngx_log_t                               *log;
    ngx_wavm_t                              *vm;
};


ngx_wasm_ops_t *ngx_wasm_ops_new(ngx_pool_t *pool, ngx_log_t *log,
    ngx_wavm_t *vm, ngx_wasm_subsystem_t *subsystem);
void ngx_wasm_ops_destroy(ngx_wasm_ops_t *ops);
ngx_wasm_ops_plan_t *ngx_wasm_ops_plan_new(ngx_pool_t *pool,
    ngx_wasm_subsystem_t *subsystem);
void ngx_wasm_ops_plan_destroy(ngx_wasm_ops_plan_t *plan);
ngx_int_t ngx_wasm_ops_plan_add(ngx_wasm_ops_plan_t *plan,
    ngx_wasm_op_t **ops_list, size_t nops);
ngx_int_t ngx_wasm_ops_plan_load(ngx_wasm_ops_plan_t *plan, ngx_log_t *log);
ngx_int_t ngx_wasm_ops_plan_attach(ngx_wasm_ops_plan_t *plan,
    ngx_wasm_op_ctx_t *ctx);
ngx_int_t ngx_wasm_ops_resume(ngx_wasm_op_ctx_t *ctx, ngx_uint_t phaseidx);

#endif /* _NGX_WASM_OPS_H_INCLUDED_ */
