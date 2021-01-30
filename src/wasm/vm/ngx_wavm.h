#ifndef _NGX_WAVM_H_INCLUDED_
#define _NGX_WAVM_H_INCLUDED_


#include <ngx_wasm.h>

#include <ngx_wavm_hfuncs.h>


typedef struct {
    ngx_wavm_t                        *vm;
    ngx_wavm_instance_t               *instance;
    ngx_log_t                         *orig_log;
} ngx_wavm_log_ctx_t;


struct ngx_wavm_func_s {
    ngx_wavm_module_t                 *module;
    ngx_str_node_t                     sn;         /* module->funcs */

    ngx_str_t                          name;
    ngx_uint_t                         exports_idx;
};


struct ngx_wavm_instance_s {
    ngx_wavm_ctx_t                    *ctx;
    ngx_wavm_module_t                 *module;

    ngx_pool_t                        *pool;
    ngx_log_t                         *log;
    ngx_wavm_log_ctx_t                 log_ctx;
    ngx_wavm_hfunc_tctx_t             *tctxs;
    wasm_instance_t                   *instance;
    wasm_memory_t                     *memory;
    wasm_extern_vec_t                  env;
    wasm_extern_vec_t                  exports;

    ngx_str_t                          trapmsg;
    u_char                            *trapbuf;
    u_char                            *mem_offset;
};


struct ngx_wavm_ctx_s {
    ngx_wavm_t                        *vm;

    ngx_pool_t                        *pool;
    ngx_log_t                         *log;
    void                              *data;

    wasm_store_t                      *store;
    ngx_wavm_instance_t              **instances;
};


#define NGX_WAVM_MODULE_ISWAT       (1 << 0)
#define NGX_WAVM_MODULE_LOADED      (1 << 1)

#define ngx_wavm_module_loaded(m)   (m && (m->state & NGX_WAVM_MODULE_LOADED))
#define ngx_wavm_module_is_wat(m)   (m->state & NGX_WAVM_MODULE_ISWAT)


struct ngx_wavm_module_s {
    ngx_wavm_t                        *vm;
    ngx_str_node_t                     sn;         /* vm->modules_tree */

    ngx_uint_t                         state;
    ngx_uint_t                         ctx_idx;
    ngx_str_t                          name;
    ngx_str_t                          path;
    wasm_module_t                     *module;
    wasm_importtype_vec_t              imports;
    wasm_exporttype_vec_t              exports;
    ngx_int_t                          memory_idx;
    ngx_uint_t                         nhfuncs;
    ngx_wavm_hfunc_t                 **hfuncs;
    ngx_rbtree_t                       funcs_tree;
    ngx_rbtree_node_t                  funcs_sentinel;
};


struct ngx_wavm_s {
    const ngx_str_t                   *name;
    ngx_pool_t                        *pool;
    ngx_log_t                         *log;
    ngx_wavm_log_ctx_t                 log_ctx;
    ngx_wavm_hfuncs_t                 *hfuncs;
    ngx_uint_t                         modules_max;
    ngx_rbtree_t                       modules_tree;
    ngx_rbtree_node_t                  modules_sentinel;
    wasm_config_t                     *config;
    wasm_engine_t                     *engine;
};


ngx_wavm_t *ngx_wavm_new(ngx_cycle_t *cycle, const ngx_str_t *name);

void ngx_wavm_free(ngx_wavm_t *vm);

ngx_wavm_module_t *ngx_wavm_module_lookup(ngx_wavm_t *vm, ngx_str_t *name);

ngx_int_t ngx_wavm_module_add(ngx_wavm_t *vm, ngx_str_t *name, ngx_str_t *path);

ngx_int_t ngx_wavm_init(ngx_wavm_t *vm, ngx_wavm_hfuncs_t *hfuncs);

ngx_wavm_func_t *ngx_wavm_module_function_lookup(ngx_wavm_module_t *module,
    ngx_str_t *name);

ngx_wavm_instance_t *ngx_wavm_module_instantiate(ngx_wavm_module_t *module,
    ngx_wavm_ctx_t *ctx, ngx_module_t *m);

ngx_int_t ngx_wavm_function_call(ngx_wavm_func_t *func,
    ngx_wavm_instance_t *instance);

ngx_int_t ngx_wavm_ctx_init(ngx_wavm_t *vm, ngx_wavm_ctx_t *ctx);

void ngx_wavm_ctx_destroy(ngx_wavm_ctx_t *ctx);

void ngx_wavm_instance_free(ngx_wavm_instance_t *instance);

void ngx_wavm_log_error(ngx_uint_t level, ngx_log_t *log,
    ngx_wrt_res_t *err, wasm_trap_t *trap,
#if (NGX_HAVE_VARIADIC_MACROS)
    const char *fmt, ...);

#else
    const char *fmt, va_list args);
#endif


#endif /* _NGX_WAVM_H_INCLUDED_ */
