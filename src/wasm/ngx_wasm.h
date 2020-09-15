/*
 * Copyright (C) Thibault Charbonnier
 */

#ifndef _NGX_WASM_H_INCLUDED_
#define _NGX_WASM_H_INCLUDED_


#include <ngx_core.h>


#if (NGX_DEBUG)
#include <assert.h>
#   define ngx_wasm_assert(a)        assert(a)
#else
#   define ngx_wasm_assert(a)
#endif

#define NGX_LOG_DEBUG_WASM           NGX_LOG_DEBUG_ALL

#define NGX_WASM_MODULE              0x5741534d   /* "WASM" */
#define NGX_WASM_CONF                0x00300000

#define NGX_WASM_MODULE_ISWAT        (1 << 0)
#define NGX_WASM_MODULE_LOADED       (1 << 1)

#define NGX_WASM_ARGS_MAX            8
#define NGX_WASM_RETS_MAX            1

#define NGX_WASM_DEFAULT_RUNTIME     "wasmtime"

#define NGX_WASM_OK                  NGX_OK
#define NGX_WASM_ERROR               NGX_ERROR
#define NGX_WASM_BAD_CTX             NGX_DECLINED
#define NGX_WASM_SENT_LAST           NGX_DONE

#define NGX_WASM_ARGS_NONE           { 0, 0, 0, 0, 0, 0, 0, 0 }
#define NGX_WASM_RETS_NONE           { 0 }

#define NGX_WASM_ARGS_I32_I32                                                \
    { NGX_WASM_I32, NGX_WASM_I32, 0, 0, 0, 0, 0, 0 }

#define NGX_WASM_ARGS_I32_I32_I32                                            \
    { NGX_WASM_I32, NGX_WASM_I32, NGX_WASM_I32, 0, 0, 0, 0, 0 }

#define NGX_WASM_ARGS_I64_I32                                                \
    { NGX_WASM_I64, NGX_WASM_I32, 0, 0, 0, 0, 0, 0 }

#define NGX_WASM_RETS_I32                                                    \
    { NGX_WASM_I32 }

#define ngx_wasm_hfunc_padding   0, 0, 0, NULL

#define ngx_wasm_hfunc_null                                                  \
    { ngx_null_string, NULL, NGX_WASM_ARGS_NONE, NGX_WASM_RETS_NONE,         \
      ngx_wasm_hfunc_padding}


/* wasm vm */


typedef struct ngx_wasm_vm_s  ngx_wasm_vm_t;
typedef struct ngx_wasm_vm_instance_s  ngx_wasm_vm_instance_t;

typedef enum {
    NGX_WASM_I32 = 1,
    NGX_WASM_I64,
    NGX_WASM_F32,
    NGX_WASM_F64
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


/* host context */


typedef enum {
    NGX_WASM_SUBSYS_ANY,
    NGX_WASM_SUBSYS_HTTP
} ngx_wasm_subsys_kind;

typedef struct {
    ngx_pool_t              *pool;
    ngx_log_t               *log;
    char                    *mem_off;
    void                    *data;
} ngx_wasm_hctx_t;


/* host functions */


typedef ngx_int_t (*ngx_wasm_hfunc_pt)(ngx_wasm_hctx_t *hctx,
    const ngx_wasm_val_t args[], ngx_wasm_val_t rets[]);

typedef struct ngx_wasm_hfunc_s  ngx_wasm_hfunc_t;
typedef struct ngx_wasm_hfuncs_s  ngx_wasm_hfuncs_t;
typedef struct ngx_wasm_hfuncs_decls_s  ngx_wasm_hfuncs_decls_t;

ngx_wasm_hfunc_t *ngx_wasm_hfuncs_lookup(ngx_wasm_hfuncs_t *hfuncs,
    u_char *mod_name, size_t mod_len, u_char *func_name, size_t func_len);


/* wasm runtime */


typedef void *ngx_wrt_engine_pt;
typedef void *ngx_wrt_module_pt;
typedef void *ngx_wrt_instance_pt;
typedef void *ngx_wrt_functype_pt;
typedef void *ngx_wrt_error_pt;
typedef void *ngx_wrt_trap_pt;

typedef ngx_wrt_functype_pt (*ngx_wrt_hfunctype_new_pt)(ngx_wasm_hfunc_t *hfunc);
typedef void (*ngx_wrt_hfunctype_free_pt)(ngx_wrt_functype_pt func);

typedef ngx_wrt_engine_pt (*ngx_wrt_engine_new_pt)(ngx_pool_t *pool);

typedef ngx_wrt_module_pt (*ngx_wrt_module_new_pt)(ngx_wrt_engine_pt engine,
    ngx_wasm_hfuncs_t *hfuncs, ngx_str_t *mod_name, ngx_str_t *bytes,
    ngx_uint_t flags, ngx_wrt_error_pt *err);

typedef ngx_wrt_instance_pt (*ngx_wrt_instance_new_pt)(
    ngx_wrt_module_pt module, ngx_wasm_hctx_t *hctx,
    ngx_wrt_error_pt *err, ngx_wrt_trap_pt *trap);

typedef ngx_int_t (*ngx_wrt_instance_call_pt)(
    ngx_wrt_instance_pt instance, ngx_str_t *func_name,
    ngx_wrt_error_pt *err, ngx_wrt_trap_pt *trap);

typedef void (*ngx_wrt_instance_free_pt)(ngx_wrt_instance_pt instance);
typedef void (*ngx_wrt_module_free_pt)(ngx_wrt_module_pt module);
typedef void (*ngx_wrt_engine_free_pt)(ngx_wrt_engine_pt engine);

typedef u_char *(*ngx_wrt_trap_log_handler_pt)(ngx_wrt_error_pt err,
    ngx_wrt_trap_pt trap, u_char *buf, size_t len);

typedef struct {
    ngx_str_t                     name;

    ngx_wrt_engine_new_pt         engine_new;
    ngx_wrt_engine_free_pt        engine_free;

    ngx_wrt_module_new_pt         module_new;
    ngx_wrt_module_free_pt        module_free;

    ngx_wrt_instance_new_pt       instance_new;
    ngx_wrt_instance_call_pt      instance_call;
    ngx_wrt_instance_free_pt      instance_free;

    ngx_wrt_hfunctype_new_pt      hfunctype_new;
    ngx_wrt_hfunctype_free_pt     hfunctype_free;

    ngx_wrt_trap_log_handler_pt   trap_log_handler;
} ngx_wrt_t;


/* definitions */


struct ngx_wasm_hfunc_s {
    ngx_str_t                      name;
    ngx_wasm_hfunc_pt              ptr;
    ngx_wasm_val_kind              args[NGX_WASM_ARGS_MAX];
    ngx_wasm_val_kind              rets[NGX_WASM_RETS_MAX];
    ngx_uint_t                     nargs;
    ngx_uint_t                     nrets;
    ngx_wasm_subsys_kind           subsys;
    ngx_wrt_functype_pt            wrt_functype;
};


struct ngx_wasm_hfuncs_decls_s {
    ngx_wasm_subsys_kind           subsys;
    ngx_wasm_hfunc_t               hfuncs[];
};


/* core wasm module ABI */


extern ngx_module_t  ngx_wasm_module;

typedef struct {
    ngx_wrt_t                     *runtime;
    ngx_wasm_hfuncs_decls_t       *hdecls;
    void                          *(*create_conf)(ngx_cycle_t *cycle);
    char                          *(*init_conf)(ngx_cycle_t *cycle, void *conf);
    ngx_int_t                      (*init)(ngx_cycle_t *cycle);
} ngx_wasm_module_t;

ngx_wasm_vm_t *ngx_wasm_core_get_vm(ngx_cycle_t *cycle);


#endif /* _NGX_WASM_H_INCLUDED_ */
