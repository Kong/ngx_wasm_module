/*
 * Copyright (C) Thibault Charbonnier
 */

#ifndef _NGX_WASM_PHASE_ENGINE_H_INCLUDED_
#define _NGX_WASM_PHASE_ENGINE_H_INCLUDED_


#include <ngx_core.h>
#include <ngx_wasm_vm.h>
#include <ngx_wasm_vm_cache.h>


typedef struct ngx_wasm_phase_engine_s  ngx_wasm_phase_engine_t;
typedef struct ngx_wasm_phase_engine_op_s  ngx_wasm_phase_engine_op_t;
typedef struct ngx_wasm_phase_engine_ctx_s  ngx_wasm_phase_engine_ctx_t;


typedef ngx_int_t (*ngx_wasm_phase_engine_op_handler_pt)(
    ngx_wasm_phase_engine_ctx_t *pctx, ngx_wasm_phase_engine_op_t *op);


typedef enum {
    NGX_WASM_PHASE_ENGINE_CALL_OP = 1
} ngx_wasm_phase_engine_op_code_t;


struct ngx_wasm_phase_engine_op_s {
    ngx_wasm_phase_engine_op_code_t          code;
    ngx_wasm_phase_engine_op_handler_pt      handler;
    ngx_uint_t                               phase;
    ngx_str_t                                phase_name;
    ngx_str_t                                mod_name;
    ngx_str_t                                func_name;
};


struct ngx_wasm_phase_engine_ctx_s {
    ngx_uint_t                               cur_phase;
    ngx_pool_t                              *pool;
    ngx_log_t                               *log;
    ngx_wasm_phase_engine_t                 *phengine;
    ngx_wasm_vm_cache_t                     *vmcache;
    void                                    *data;
};


typedef struct {
    ngx_str_t                               *name;
    ngx_array_t                             *ops;
} ngx_wasm_phase_engine_phase_t;


struct ngx_wasm_phase_engine_s {
    ngx_uint_t                               nphases;
    ngx_wasm_subsys_kind                     subsys;
    ngx_wasm_vm_t                           *vm;
    ngx_wasm_phase_engine_phase_t          **phases;
};


char *ngx_wasm_phase_engine_add_op(ngx_conf_t *cf,
    ngx_wasm_phase_engine_t *phengine, ngx_wasm_phase_engine_op_t *op);

ngx_int_t ngx_wasm_phase_engine_resume(ngx_wasm_phase_engine_ctx_t *pctx);


#endif /* _NGX_WASM_PHASE_ENGINE_H_INCLUDED_ */
