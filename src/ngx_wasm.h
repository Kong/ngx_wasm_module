/*
 * Copyright (C) Thibault Charbonnier
 */

#ifndef _NGX_WASM_H_INCLUDED_
#define _NGX_WASM_H_INCLUDED_

#ifdef NGX_WASM_USE_ASSERT
#include <assert.h>
#   define ngx_wasm_assert(a)  assert(a)
#else
#   define ngx_wasm_assert(a)
#endif

#include <nginx.h>
#include <ngx_core.h>


#define NGX_LOG_DEBUG_WASM           NGX_LOG_DEBUG_ALL

#define NGX_WASM_DEFAULT_VM          "wasmtime"

#define NGX_WASM_VM_MODULE_ISWAT     (1 << 0)
#define NGX_WASM_VM_MODULE_LOADED    (1 << 1)

#define NGX_WASM_HOSTFUNC_MAX_ARGS   8
#define NGX_WASM_HOSTFUNC_MAX_RETS   1

//#define NGX_WASM_ARG_I32(i)        { .kind = NGX_WASM_I32, .value.I32 = i }
//#define NGX_WASM_ARG_I64(i)        { .kind = NGX_WASM_I64, .value.I64 = i }
//#define NGX_WASM_ARG_F32(i)        { .kind = NGX_WASM_F32, .value.F32 = i }
//#define NGX_WASM_ARG_F64(i)        { .kind = NGX_WASM_F64, .value.F64 = i }


/* vm */


typedef struct ngx_wasm_vm_s       *ngx_wasm_vm_pt;
typedef struct ngx_wasm_instance_s *ngx_wasm_instance_pt;


typedef enum {
    NGX_WASM_I32 = 1,
    NGX_WASM_I64,
    NGX_WASM_F32,
    NGX_WASM_F64,
} ngx_wasm_vm_val_kind;


typedef struct {
    ngx_wasm_vm_val_kind     kind;

    union {
        int32_t     I32;
        int64_t     I64;
        float       F32;
        double      F64;
    } value;
} ngx_wasm_vm_val_t;


/* runtime */


typedef void *ngx_wasm_vm_engine_pt;
typedef void *ngx_wasm_vm_module_pt;
typedef void *ngx_wasm_vm_instance_pt;
typedef void *ngx_wasm_vm_error_pt;
typedef void *ngx_wasm_vm_trap_pt;


typedef ngx_wasm_vm_trap_pt (*ngx_wasm_hostfunc_pt)(const void *caller,
    const ngx_wasm_vm_val_t args[], ngx_wasm_vm_val_t rets[]);


typedef struct {
    ngx_str_t               name;
    ngx_wasm_hostfunc_pt    ptr;
    ngx_wasm_vm_val_kind    args[NGX_WASM_HOSTFUNC_MAX_ARGS];
    ngx_wasm_vm_val_kind    rets[NGX_WASM_HOSTFUNC_MAX_RETS];
} ngx_wasm_hostfunc_t;


typedef struct {
    ngx_wasm_vm_engine_pt   (*new_engine)(ngx_pool_t *pool,
                                          ngx_wasm_vm_error_pt *vm_error);

    void                    (*free_engine)(ngx_wasm_vm_engine_pt vm_engine);

    ngx_wasm_vm_module_pt   (*new_module)(ngx_wasm_vm_engine_pt vm_engine,
                                          ngx_str_t *name, ngx_str_t *bytes,
                                          ngx_uint_t flags,
                                          ngx_wasm_vm_error_pt *vm_error);

    void                    (*free_module)(ngx_wasm_vm_module_pt vm_module);

    ngx_wasm_vm_instance_pt (*new_instance)(ngx_wasm_vm_engine_pt vm_engine,
                                            ngx_wasm_vm_module_pt vm_module,
                                            ngx_wasm_vm_error_pt *vm_error,
                                            ngx_wasm_vm_trap_pt *vm_trap);

    void                    (*free_instance)(ngx_wasm_vm_instance_pt
                                             vm_instance);

    u_char                 *(*log_error_handler)(ngx_wasm_vm_error_pt vm_error,
                                                 ngx_wasm_vm_trap_pt vm_trap,
                                                 u_char *buf, size_t len);
} ngx_wasm_vm_actions_t;


#endif /* _NGX_WASM_H_INCLUDED_ */


/* vi:set ft=c ts=4 sw=4 et fdm=marker: */
