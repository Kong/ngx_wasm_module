#ifndef _NGX_WAVM_HFUNCS_H_INCLUDED_
#define _NGX_WAVM_HFUNCS_H_INCLUDED_


#include <ngx_wasm.h>


#define NGX_WAVM_HFUNCS_MAX_TRAP_LEN   128

#define ngx_wavm_hfunc_null            { ngx_null_string, NULL, NULL, NULL }


typedef struct {
    ngx_str_node_t                     sn;              /* store rbtrees */
    ngx_wavm_hfunc_def_t              *def;
    wasm_functype_t                   *functype;
} ngx_wavm_hfunc_t;


typedef ngx_int_t (*ngx_wavm_hfunc_pt)(ngx_wavm_instance_t *instance,
    const wasm_val_t args[], wasm_val_t rets[]);


struct ngx_wavm_hfunc_def_s {
    ngx_str_t                          name;
    ngx_wavm_hfunc_pt                  ptr;
    const wasm_valkind_t             **args;
    const wasm_valkind_t             **rets;
};


typedef struct {
    ngx_wavm_hfunc_t                  *hfunc;
    ngx_wavm_instance_t               *instance;
} ngx_wavm_hfunc_tctx_t;


wasm_trap_t *ngx_wavm_hfuncs_trampoline(void *env,
#if (NGX_WASM_HAVE_WASMTIME)
    const wasm_val_t args[], wasm_val_t rets[]
#else
    const wasm_val_vec_t* args, wasm_val_vec_t* rets
#endif
    );


void ngx_wavm_instance_trap_printf(ngx_wavm_instance_t *instance,
#if (NGX_HAVE_VARIADIC_MACROS)
    const char *fmt, ...);

#else
    const char *fmt, va_list args);
#endif


extern const wasm_valkind_t * ngx_wavm_arity_i32[];
extern const wasm_valkind_t * ngx_wavm_arity_i32_i32[];
extern const wasm_valkind_t * ngx_wavm_arity_i32_i32_i32[];


/* hfuncs store */


typedef struct {
    ngx_rbtree_t                       rbtree;
    ngx_rbtree_node_t                  sentinel;
} ngx_wavm_hfuncs_tree_t;


typedef struct {
    ngx_pool_t                        *pool;
    ngx_log_t                         *log;
    ngx_uint_t                         global;
    ngx_wavm_hfuncs_tree_t           **trees;
} ngx_wavm_hfuncs_t;


ngx_wavm_hfuncs_t *ngx_wavm_hfuncs_new(ngx_cycle_t *cycle);

void ngx_wavm_hfuncs_free(ngx_wavm_hfuncs_t *hfuncs);

ngx_int_t ngx_wavm_hfuncs_add(ngx_wavm_hfuncs_t *hfuncs,
    ngx_wavm_hfunc_def_t *hdefs, ngx_module_t *m);

ngx_wavm_hfunc_t *ngx_wavm_hfuncs_lookup(ngx_wavm_hfuncs_t *hfuncs,
    ngx_str_t *name, ngx_module_t *m);


#endif /* _NGX_WAVM_HFUNCS_H_INCLUDED_ */
