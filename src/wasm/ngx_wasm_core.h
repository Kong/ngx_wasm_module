#ifndef _NGX_WASM_CORE_H_INCLUDED_
#define _NGX_WASM_CORE_H_INCLUDED_


#include <ngx_core.h>
#include <ngx_wasm.h>
#include <ngx_wasm_util.h>


typedef struct ngx_wasm_vm_log_ctx_s {
    ngx_wasm_vm_t                     *vm;
    ngx_wasm_vm_instance_t            *instance;
    ngx_log_t                         *orig_log;
} ngx_wasm_vm_log_ctx_t;


struct ngx_wasm_hfunc_s {
    ngx_str_t                         *name;
    ngx_wasm_hsubsys_kind              subsys;
    ngx_wasm_hfunc_pt                  ptr;
    ngx_str_node_t                     sn;              /* vm->hfuncs_tree */

    wasm_functype_t                   *functype;
};


typedef struct {
    ngx_wasm_hfunc_t                  *hfunc;
    ngx_wasm_vm_instance_t            *instance;
} ngx_wasm_hfunc_tctx_t;


struct ngx_wasm_vm_instance_s {
    ngx_pool_t                        *pool;
    ngx_log_t                          log;
    ngx_wasm_vm_log_ctx_t              log_ctx;
    ngx_wasm_vm_t                     *vm;
    ngx_wasm_vm_module_t              *module;
    ngx_queue_t                        queue;           /* module->instances_queue */

    wasm_instance_t                   *instance;
    wasm_store_t                      *store;
    wasm_memory_t                     *memory;
    wasm_extern_vec_t                  env;
    wasm_extern_vec_t                  exports;

    ngx_wasm_hctx_t                   *hctx;
    ngx_wasm_hfunc_tctx_t            **tctxs;
};


struct ngx_wasm_vm_module_s {
    ngx_uint_t                         flags;
    ngx_str_t                          name;
    ngx_str_t                          path;
    ngx_queue_t                        instances_queue;
    ngx_wasm_vm_t                     *vm;
    ngx_str_node_t                     sn;              /* vm->modules_tree */

    wasm_module_t                     *module;
    wasm_exporttype_vec_t              exports;

    ngx_uint_t                         nhfuncs;
    ngx_wasm_hfunc_t                 **hfuncs;
};


struct ngx_wasm_vm_s {
    ngx_str_t                          name;
    ngx_pool_t                        *pool;
    ngx_log_t                          log;
    ngx_wasm_vm_log_ctx_t              log_ctx;
    ngx_rbtree_t                       modules_tree;
    ngx_rbtree_t                       hfuncs_tree;
    ngx_rbtree_node_t                  modules_sentinel;
    ngx_rbtree_node_t                  hfuncs_sentinel;
    ngx_wrt_t                         *runtime;

    wasm_config_t                     *config;
    wasm_engine_t                     *engine;

    unsigned                           new_module:1;    /* new module(s) to load */
};


#endif /* _NGX_WASM_CORE_H_INCLUDED_ */
