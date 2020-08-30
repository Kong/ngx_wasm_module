/*
 * Copyright (C) Thibault Charbonnier
 */

#ifndef _NGX_WASM_VM_DEF_H_INCLUDED_
#define _NGX_WASM_VM_DEF_H_INCLUDED_


#include <ngx_core.h>
#include <ngx_wasm_vm.h>
#include <ngx_wasm_util.h>


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
    ngx_wasm_rbtree_named_node_t   rbnode;
    ngx_uint_t                     flags;
    ngx_str_t                      bytes;
    ngx_str_t                      name;
    ngx_str_t                      path;
    ngx_queue_t                    instances_queue;
    ngx_wrt_module_pt              wrt;
};


typedef struct ngx_wasm_vm_modules_s {
    ngx_rbtree_t                   rbtree;
    ngx_rbtree_node_t              sentinel;
} ngx_wasm_vm_modules_t;


struct ngx_wasm_vm_s {
    ngx_str_t                      name;
    ngx_pool_t                    *pool;
    ngx_log_t                     *log;
    ngx_wasm_vm_log_ctx_t          log_ctx;
    ngx_wasm_vm_modules_t          modules;
    ngx_wrt_t                     *runtime;
    ngx_wrt_engine_pt              wrt;
};


#endif /* _NGX_WASM_VM_DEF_H_INCLUDED_ */
