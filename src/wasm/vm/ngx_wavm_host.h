#ifndef _NGX_WAVM_HOST_H_INCLUDED_
#define _NGX_WAVM_HOST_H_INCLUDED_


#include <ngx_wasm.h>


/* host definitions */


typedef struct ngx_wavm_host_func_def_s  ngx_wavm_host_func_def_t;
typedef struct ngx_wavm_host_def_s  ngx_wavm_host_def_t;


typedef ngx_int_t (*ngx_wavm_hfunc_pt)(ngx_wavm_instance_t *instance,
    wasm_val_t args[], wasm_val_t rets[]);


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


extern const wasm_valkind_t *ngx_wavm_arity_i32[];
extern const wasm_valkind_t *ngx_wavm_arity_i64[];
extern const wasm_valkind_t *ngx_wavm_arity_i32x2[];
extern const wasm_valkind_t *ngx_wavm_arity_i32x3[];
extern const wasm_valkind_t *ngx_wavm_arity_i32x4[];
extern const wasm_valkind_t *ngx_wavm_arity_i32x5[];
extern const wasm_valkind_t *ngx_wavm_arity_i32x6[];
extern const wasm_valkind_t *ngx_wavm_arity_i32x8[];
extern const wasm_valkind_t *ngx_wavm_arity_i32x9[];
extern const wasm_valkind_t *ngx_wavm_arity_i32x10[];
extern const wasm_valkind_t *ngx_wavm_arity_i32x12[];
extern const wasm_valkind_t *ngx_wavm_arity_i32_i64[];


/* hfuncs */


#define NGX_WAVM_HFUNCS_MAX_TRAP_LEN   128

#define ngx_wavm_hfunc_null            { ngx_null_string, NULL, NULL, NULL }


struct ngx_wavm_hfunc_s {
    ngx_pool_t                        *pool;
    ngx_wavm_host_func_def_t          *def;
    wasm_functype_t                   *functype;
    ngx_uint_t                         idx;
};


ngx_wavm_hfunc_t *ngx_wavm_host_hfunc_create(ngx_pool_t *pool,
    ngx_wavm_host_def_t *host, ngx_str_t *name);
void ngx_wavm_host_hfunc_destroy(ngx_wavm_hfunc_t *hfunc);

wasm_trap_t * ngx_wavm_hfunc_trampoline(void *env,
#ifdef NGX_WASM_HAVE_WASMTIME
    wasmtime_caller_t *caller,
    const wasmtime_val_t *args, size_t nargs,
    wasmtime_val_t *rets, size_t nret);
#elif NGX_WASM_HAVE_WASMER
    const wasm_val_vec_t *args,
    wasm_val_vec_t *rets);
#elif NGX_WASM_HAVE_V8
    const wasm_val_t args[],
    wasm_val_t rets[]);
#endif


/* hfuncs utils */


#define NGX_WAVM_HOST_LIFT(instance, wasm_ptr, type)                         \
({                                                                           \
    unsigned   err_count = 0;                                                \
    void      *ptr = ngx_wavm_memory_lift(                                   \
        instance->memory, wasm_ptr,                                          \
        sizeof(type), __alignof__(type), &err_count);                        \
    if (err_count != 0) {                                                    \
        ngx_wavm_instance_trap_printf(                                       \
            instance,                                                        \
            "invalid data pointer passed to host function");                 \
        return NGX_WAVM_BAD_USAGE;                                           \
    }                                                                        \
    (type *) ptr;                                                            \
})


#define NGX_WAVM_HOST_LIFT_SLICE(instance, wasm_ptr, len)                    \
({                                                                           \
    unsigned   err_count = 0;                                                \
    void      *ptr = ngx_wavm_memory_lift(                                   \
        instance->memory, wasm_ptr,                                          \
        len, 1, &err_count);                                                 \
    if (err_count != 0) {                                                    \
        ngx_wavm_instance_trap_printf(                                       \
            instance,                                                        \
            "invalid slice pointer passed to host function");                \
        return NGX_WAVM_BAD_USAGE;                                           \
    }                                                                        \
    ptr;                                                                     \
})


#endif /* _NGX_WAVM_HOST_H_INCLUDED_ */
