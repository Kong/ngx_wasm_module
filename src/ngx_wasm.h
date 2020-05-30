/*
 * Copyright (C) Thibault Charbonnier
 */

#ifndef _NGX_WASM_H_INCLUDED_
#define _NGX_WASM_H_INCLUDED_


#if !(NGX_HAVE_VARIADIC_MACROS)
#   error "NYI: ngx_wasm_module requires C99 variadic macros"
#endif


#ifdef NGX_WASM_USE_ASSERT
#include <assert.h>
#   define ngx_wasm_assert(a)  assert(a)
#else
#   define ngx_wasm_assert(a)
#endif


#include <nginx.h>
#include <ngx_core.h>


#define NGX_WASM_MODULE        0x5741534d   /* "MSAW" */
#define NGX_WASM_CONF          0x00300000
#define NGX_WASM_DEFAULT_VM    "wasmtime"
#define NGX_WASM_NO_VM_ACTIONS { NULL, NULL, NULL, NULL, NULL }
#define NGX_LOG_DEBUG_WASM     NGX_LOG_DEBUG_CORE


#define NGX_WASM_ARG_I32(i)    { .kind = NGX_WASM_I32, .value.I32 = i }
#define NGX_WASM_ARG_I64(i)    { .kind = NGX_WASM_I64, .value.I64 = i }
#define NGX_WASM_ARG_F32(i)    { .kind = NGX_WASM_F32, .value.F32 = i }
#define NGX_WASM_ARG_F64(i)    { .kind = NGX_WASM_F64, .value.F64 = i }


typedef struct {
    ngx_str_t       path;
    ngx_pool_t     *pool;
    ngx_log_t      *log;
    void           *data;
    unsigned        wat:1;
} ngx_wasm_wmodule_t;


typedef struct {
    ngx_pool_t     *pool;
    ngx_log_t      *log;
    void           *data;
} ngx_wasm_winstance_t;


//typedef struct {

//} ngx_wasm_wfunc_t;


typedef enum {
    NGX_WASM_I32,
    NGX_WASM_I64,
    NGX_WASM_F32,
    NGX_WASM_F64,
} ngx_wasm_wval_kind;


typedef struct {
    ngx_wasm_wval_kind kind;

    union {
        int32_t        I32;
        int64_t        I64;
        float          F32;
        double         F64;
    } value;
} ngx_wasm_wval_t;


typedef struct {
    ngx_int_t               (*load_module)(ngx_wasm_wmodule_t *module,
                                           ngx_cycle_t *cycle);
    void                    (*unload_module)(ngx_wasm_wmodule_t *module);
    ngx_wasm_winstance_t   *(*new_instance)(ngx_wasm_wmodule_t *module);
    void                    (*free_instance)(ngx_wasm_winstance_t *instance);
    ngx_int_t               (*call_instance)(ngx_wasm_winstance_t *instance,
                                             const char *fname,
                                             const ngx_wasm_wval_t *args,
                                             size_t nargs,
                                             ngx_wasm_wval_t *rets,
                                             size_t nrets);
} ngx_wasm_vm_actions_t;


typedef struct {
    ngx_str_t                   *name;
    void                        *(*create_conf)(ngx_cycle_t *cycle);
    char                        *(*init_conf)(ngx_cycle_t *cycle, void *conf);
    ngx_int_t                    (*init)(ngx_cycle_t *cycle,
                                         ngx_wasm_vm_actions_t **vm_actions);
    ngx_wasm_vm_actions_t        vm_actions;
} ngx_wasm_module_t;


typedef struct {
    ngx_uint_t                   vm;
    u_char                      *vm_name;
} ngx_wasm_core_conf_t;


extern ngx_wasm_vm_actions_t     ngx_wasm_vm_actions;
extern ngx_module_t              ngx_wasm_module;


#endif /* _NGX_WASM_H_INCLUDED_ */


/* vi:set ft=c ts=4 sw=4 et fdm=marker: */
