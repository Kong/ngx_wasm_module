/*
 * Copyright (C) Thibault Charbonnier
 */

#ifndef _NGX_WASM_H_INCLUDED_
#define _NGX_WASM_H_INCLUDED_


#if (NGX_DEBUG)
#include <assert.h>
#   define ngx_wasm_assert(a)  assert(a)
#else
#   define ngx_wasm_assert(a)
#endif


#include <ngx_core.h>


#define NGX_LOG_DEBUG_WASM           NGX_LOG_DEBUG_ALL

#define NGX_WASM_ARGS_MAX            8
#define NGX_WASM_RETS_MAX            1

#define NGX_WRT_DEFAULT              "wasmtime"


/* wasm vm */


typedef struct ngx_wasm_vm_s  ngx_wasm_vm_t;
typedef struct ngx_wasm_vm_module_s  ngx_wasm_vm_module_t;
typedef struct ngx_wasm_vm_instance_s  ngx_wasm_vm_instance_t;


typedef enum {
    NGX_WASM_I32 = 1,
    NGX_WASM_I64,
    NGX_WASM_F32,
    NGX_WASM_F64,
} ngx_wasm_val_kind;


typedef struct {
    ngx_wasm_val_kind     kind;

    union {
        int32_t     I32;
        int64_t     I64;
        float       F32;
        double      F64;
    } value;
} ngx_wasm_val_t;


/* wasm runtime */


typedef struct ngx_wrt_s  ngx_wrt_t;

typedef void *ngx_wrt_engine_pt;
typedef void *ngx_wrt_module_pt;
typedef void *ngx_wrt_instance_pt;
typedef void *ngx_wrt_functype_pt;
typedef void *ngx_wrt_error_pt;
typedef void *ngx_wrt_trap_pt;


/* host */


typedef struct ngx_wasm_hfuncs_resolver_s  ngx_wasm_hfuncs_resolver_t;
typedef struct ngx_wasm_hctx_s  ngx_wasm_hctx_t;

typedef ngx_int_t (*ngx_wasm_dfunc_pt)(ngx_wasm_hctx_t *hctx,
    const ngx_wasm_val_t args[], ngx_wasm_val_t rets[]);


typedef struct {
    ngx_str_t               *name;
    ngx_wasm_dfunc_pt        ptr;
    ngx_wasm_val_kind        args[NGX_WASM_ARGS_MAX];
    ngx_wasm_val_kind        rets[NGX_WASM_RETS_MAX];
    ngx_uint_t               nargs;
    ngx_uint_t               nrets;
    ngx_wrt_functype_pt      wrt_functype;
} ngx_wasm_hfunc_t;


typedef struct {
    ngx_str_t                name;
    ngx_wasm_dfunc_pt        ptr;
    ngx_wasm_val_kind        args[NGX_WASM_ARGS_MAX];
    ngx_wasm_val_kind        rets[NGX_WASM_RETS_MAX];
} ngx_wasm_hfunc_decl_t;


#endif /* _NGX_WASM_H_INCLUDED_ */
