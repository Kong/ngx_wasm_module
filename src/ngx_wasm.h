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

#include <nginx.h>
#include <ngx_core.h>
#include <ngx_http.h>


#define NGX_LOG_DEBUG_WASM           NGX_LOG_DEBUG_ALL

#define NGX_WASM_VM_DEFAULT          "wasmtime"
#define NGX_WASM_VM_MODULE_ISWAT     (1 << 0)
#define NGX_WASM_VM_MODULE_LOADED    (1 << 1)
#define NGX_WASM_VM_NO_ACTIONS                                               \
    { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL }

#define NGX_WASM_ARGS_MAX            8
#define NGX_WASM_RETS_MAX            1
#define NGX_WASM_ARGS_NONE           { 0, 0, 0, 0, 0, 0, 0, 0 }
#define NGX_WASM_RETS_NONE           { 0 }
#define NGX_WASM_ARGS_I32_I32_I32                                            \
    { NGX_WASM_I32, NGX_WASM_I32, NGX_WASM_I32, 0, 0, 0, 0, 0 }
#define NGX_WASM_RETS_I32                                                    \
    { NGX_WASM_I32 }
#define ngx_wasm_hfunc_null                                                  \
    { ngx_null_string, NULL, NGX_WASM_ARGS_NONE, NGX_WASM_RETS_NONE }


/* vm */


typedef struct ngx_wasm_vm_s  ngx_wasm_vm_t;
typedef struct ngx_wasm_instance_s  ngx_wasm_instance_t;


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


typedef struct {
    ngx_log_t            *log;
    char                 *memory_offset;

    union {
        ngx_http_request_t    *r;
    } data;
} ngx_wasm_hctx_t;


/* host functions */


typedef ngx_int_t (*ngx_wasm_hfunc_pt)(ngx_wasm_hctx_t *hctx,
    const ngx_wasm_val_t args[], ngx_wasm_val_t rets[]);


typedef struct ngx_wasm_hfunc_decl_s {
    ngx_str_t                name;
    ngx_wasm_hfunc_pt        ptr;
    ngx_wasm_val_kind        args[NGX_WASM_ARGS_MAX];
    ngx_wasm_val_kind        rets[NGX_WASM_RETS_MAX];
} ngx_wasm_hfunc_decl_t;


void ngx_wasm_core_hfuncs_add(ngx_cycle_t *cycle,
    const ngx_wasm_hfunc_decl_t decls[]);


/* runtime */


typedef struct ngx_wasm_hfunc_s  ngx_wasm_hfunc_t;
typedef struct ngx_wasm_hfuncs_store_s  ngx_wasm_hfuncs_store_t;
typedef struct ngx_wasm_hfuncs_resolver_s  ngx_wasm_hfuncs_resolver_t;


typedef void *ngx_wasm_vm_engine_pt;
typedef void *ngx_wasm_vm_module_pt;
typedef void *ngx_wasm_vm_instance_pt;
typedef void *ngx_wasm_vm_error_pt;
typedef void *ngx_wasm_vm_trap_pt;


typedef void *(*ngx_wasm_hfunc_new_pt)(ngx_wasm_hfunc_t *hfunc);
typedef void (*ngx_wasm_hfunc_free_pt)(void *vm_data);


typedef struct {
    ngx_wasm_vm_engine_pt     (*engine_new)(ngx_pool_t *pool,
                                  ngx_wasm_hfuncs_resolver_t *hf_resolver);

    void                      (*engine_free)(ngx_wasm_vm_engine_pt vm_engine);

    ngx_wasm_vm_module_pt     (*module_new)(ngx_wasm_vm_engine_pt vm_engine,
                                            ngx_str_t *mod_name,
                                            ngx_str_t *bytes, ngx_uint_t flags,
                                            ngx_wasm_vm_error_pt *error);

    void                      (*module_free)(ngx_wasm_vm_module_pt vm_module);

    ngx_wasm_vm_instance_pt   (*instance_new)(ngx_wasm_vm_module_pt vm_module,
                                              ngx_wasm_hctx_t *hctx,
                                              ngx_wasm_vm_error_pt *error,
                                              ngx_wasm_vm_trap_pt *trap);

    ngx_int_t                 (*instance_call)(ngx_wasm_vm_instance_pt
                                               vm_instance,
                                               ngx_str_t *func_name,
                                               ngx_wasm_vm_error_pt *error,
                                               ngx_wasm_vm_trap_pt *trap);

    void                      (*instance_free)(ngx_wasm_vm_instance_pt
                                               vm_instance);

    u_char                   *(*log_error_handler)(ngx_wasm_vm_error_pt error,
                                                   ngx_wasm_vm_trap_pt trap,
                                                   u_char *buf, size_t len);

    ngx_wasm_hfunc_new_pt     hfunc_new;
    ngx_wasm_hfunc_free_pt    hfunc_free;
} ngx_wasm_vm_actions_t;


#endif /* _NGX_WASM_H_INCLUDED_ */
