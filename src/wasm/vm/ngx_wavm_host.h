#ifndef _NGX_WAVM_HOST_H_INCLUDED_
#define _NGX_WAVM_HOST_H_INCLUDED_


#include <ngx_wasm.h>


/* host definitions */


typedef struct ngx_wavm_host_func_def_s  ngx_wavm_host_func_def_t;
typedef struct ngx_wavm_host_def_s  ngx_wavm_host_def_t;


typedef ngx_int_t (*ngx_wavm_hfunc_pt)(ngx_wavm_instance_t *instance,
    const wasm_val_t args[], wasm_val_t rets[]);


struct ngx_wavm_host_func_def_s {
    ngx_str_t                          name;
    ngx_wavm_hfunc_pt                  ptr;
    const wasm_valkind_t             **args;
    const wasm_valkind_t             **rets;
};


struct ngx_wavm_host_def_s {
    ngx_str_t                          name;
    ngx_wavm_host_func_def_t          *funcs;
};


extern const wasm_valkind_t * ngx_wavm_arity_i32[];
extern const wasm_valkind_t * ngx_wavm_arity_i32_i32[];
extern const wasm_valkind_t * ngx_wavm_arity_i32_i32_i32[];


/* hfuncs */


#define NGX_WAVM_HFUNCS_MAX_TRAP_LEN   128

#define ngx_wavm_hfunc_null            { ngx_null_string, NULL, NULL, NULL }


typedef struct {
    ngx_pool_t                        *pool;
    ngx_wavm_host_func_def_t          *def;
    wasm_functype_t                   *functype;
} ngx_wavm_hfunc_t;


typedef struct {
    ngx_wavm_hfunc_t                  *hfunc;
    ngx_wavm_instance_t               *instance;
} ngx_wavm_hfunc_tctx_t;


ngx_wavm_hfunc_t *ngx_wavm_host_hfunc_create(ngx_pool_t *pool,
    ngx_wavm_host_def_t *host, ngx_str_t *name);
void ngx_wavm_host_hfunc_destroy(ngx_wavm_hfunc_t *hfunc);


wasm_trap_t *ngx_wavm_hfuncs_trampoline(void *env,
#if (NGX_WASM_HAVE_WASMTIME)
    const wasm_val_t args[], wasm_val_t rets[]);
#else
    const wasm_val_vec_t* args, wasm_val_vec_t* rets);
#endif


void ngx_wavm_instance_trap_printf(ngx_wavm_instance_t *instance,
#if (NGX_HAVE_VARIADIC_MACROS)
    const char *fmt, ...);
#else
    const char *fmt, va_list args);
#endif


#endif /* _NGX_WAVM_HOST_H_INCLUDED_ */
