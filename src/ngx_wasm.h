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


#define NGX_WASM_MODULE            0x5741534d   /* "MSAW" */
#define NGX_WASM_CONF              0x00300000
#define NGX_WASM_NO_VM_ACTIONS     { NULL, NULL, NULL, NULL, NULL, NULL }

#define NGX_WASM_WMODULE_ISWAT     (1 << 0)
#define NGX_WASM_WMODULE_HASBYTES  (1 << 1)
#define NGX_WASM_WMODULE_LOADED    (1 << 2)

#define NGX_LOG_DEBUG_WASM         NGX_LOG_DEBUG_ALL

#define NGX_WASM_ARG_I32(i)        { .kind = NGX_WASM_I32, .value.I32 = i }
#define NGX_WASM_ARG_I64(i)        { .kind = NGX_WASM_I64, .value.I64 = i }
#define NGX_WASM_ARG_F32(i)        { .kind = NGX_WASM_F32, .value.F32 = i }
#define NGX_WASM_ARG_F64(i)        { .kind = NGX_WASM_F64, .value.F64 = i }


typedef ngx_str_t ngx_wasm_vm_bytes_t;
typedef void* ngx_wasm_vm_module_t;
typedef void* ngx_wasm_vm_instance_t;
typedef void* ngx_wasm_vm_error_t;
typedef void* ngx_wasm_vm_trap_t;


typedef struct {
    ngx_wasm_vm_trap_t     trap;
    ngx_wasm_vm_error_t    vm_error;
} ngx_wasm_werror_t;


typedef struct {
    ngx_str_t               name;
    ngx_str_t               path;
    ngx_wasm_vm_bytes_t     bytes;
    ngx_uint_t              flags;
    ngx_pool_t             *pool;
    ngx_wasm_vm_module_t    vm_module;
} ngx_wasm_wmodule_t;


typedef struct {
    ngx_wasm_wmodule_t     *module;
    ngx_pool_t             *pool;
    ngx_log_t              *log;
    ngx_wasm_vm_instance_t  vm_instance;
} ngx_wasm_winstance_t;


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
    ngx_wasm_vm_module_t (*new_module)(ngx_cycle_t *cycle, ngx_pool_t *pool,
        ngx_wasm_vm_bytes_t *bytes, ngx_uint_t flags,
        ngx_wasm_vm_error_t *vm_error);

    void (*free_module)(ngx_wasm_vm_module_t vm_module);

    ngx_wasm_vm_instance_t (*new_instance)(ngx_wasm_vm_module_t vm_module,
        ngx_wasm_vm_error_t *vm_error, ngx_wasm_vm_trap_t *vm_trap);

    void (*free_instance)(ngx_wasm_vm_instance_t vm_instance);

    ngx_int_t (*call_instance)(ngx_wasm_vm_instance_t vm_instance,
        const u_char *fname, const ngx_wasm_wval_t *args, size_t nargs,
        ngx_wasm_wval_t *rets, size_t nrets, ngx_wasm_vm_error_t *vm_error,
        ngx_wasm_vm_trap_t *vm_trap);

    u_char *(*log_error_handler)(ngx_wasm_vm_error_t vm_error,
        ngx_wasm_vm_trap_t vm_trap, u_char *buf, size_t len);

} ngx_wasm_vm_actions_t;


typedef struct {
    ngx_str_t                   *name;
    void                        *(*create_conf)(ngx_cycle_t *cycle);
    char                        *(*init_conf)(ngx_cycle_t *cycle, void *conf);
    ngx_int_t                    (*init)(ngx_cycle_t *cycle,
                                         ngx_wasm_vm_actions_t **vm_actions);
    ngx_wasm_vm_actions_t        vm_actions;
} ngx_wasm_module_t;


extern ngx_module_t              ngx_wasm_module;


ngx_wasm_wmodule_t *ngx_wasm_get_module(const u_char *name, ngx_cycle_t *cycle);
ngx_wasm_wmodule_t *ngx_wasm_new_module(const u_char *name, const u_char *path,
    ngx_log_t *log);
void ngx_wasm_free_module(ngx_wasm_wmodule_t *wmodule);
ngx_int_t ngx_wasm_load_module(ngx_wasm_wmodule_t *wmodule,
    ngx_cycle_t *cycle, ngx_wasm_werror_t *werror);
ngx_wasm_winstance_t *ngx_wasm_new_instance(ngx_wasm_wmodule_t *wmodule,
    ngx_log_t *log, ngx_wasm_werror_t *werror);
void ngx_wasm_free_instance(ngx_wasm_winstance_t *winstance);
ngx_int_t ngx_wasm_call_instance(ngx_wasm_winstance_t *winstance,
    const u_char *fname, const ngx_wasm_wval_t *args, size_t nargs,
    ngx_wasm_wval_t *rets, size_t nrets, ngx_wasm_werror_t *werror);
void ngx_wasm_log_error(ngx_uint_t level, ngx_log_t *log, ngx_err_t err,
    ngx_wasm_werror_t *werror,
#if (NGX_HAVE_VARIADIC_MACROS)
    const char *fmt, ...);

#else
    const char *fmt, va_list args);
#endif


#endif /* _NGX_WASM_H_INCLUDED_ */


/* vi:set ft=c ts=4 sw=4 et fdm=marker: */
