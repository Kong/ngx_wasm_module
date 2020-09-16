/*
 * Copyright (C) Thibault Charbonnier
 */

#ifndef _NGX_WASM_CORE_H_INCLUDED_
#define _NGX_WASM_CORE_H_INCLUDED_


#include <ngx_core.h>
#include <ngx_wasm.h>
#include <ngx_wasm_util.h>


typedef struct ngx_wasm_vm_module_s  ngx_wasm_vm_module_t;


typedef struct ngx_wasm_vm_log_ctx_s {
    ngx_wasm_vm_t                 *vm;
    ngx_wasm_vm_instance_t        *instance;
    ngx_log_t                     *orig_log;
} ngx_wasm_vm_log_ctx_t;


struct ngx_wasm_vm_instance_s {
    ngx_pool_t                    *pool;
    ngx_log_t                     *log;
    ngx_wasm_vm_t                 *vm;
    ngx_wasm_vm_module_t          *module;
    ngx_queue_t                    queue;
    ngx_wasm_vm_log_ctx_t          log_ctx;
    ngx_wasm_hctx_t                hctx;
    ngx_wrt_instance_pt            wrt;
};


struct ngx_wasm_vm_module_s {
    ngx_uint_t                     flags;
    ngx_str_t                      bytes;
    ngx_str_t                      name;
    ngx_str_t                      path;
    ngx_queue_t                    instances_queue;
    ngx_wasm_nn_t                  nn;
    ngx_wrt_module_pt              wrt;
};


struct ngx_wasm_vm_s {
    ngx_str_t                      name;
    ngx_pool_t                    *pool;
    ngx_log_t                     *log;
    ngx_rbtree_t                   mtree;
    ngx_rbtree_node_t              sentinel;
    ngx_wasm_hfuncs_t             *hfuncs;
    ngx_wasm_vm_log_ctx_t          log_ctx;
    ngx_wrt_t                     *runtime;
    ngx_wrt_engine_pt              wrt;
};


#endif /* _NGX_WASM_CORE_H_INCLUDED_ */
