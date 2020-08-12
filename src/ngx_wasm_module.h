/*
 * Copyright (C) Thibault Charbonnier
 */

#ifndef _NGX_WASM_MODULE_H_INCLUDED_
#define _NGX_WASM_MODULE_H_INCLUDED_


#include <ngx_core.h>
#include <ngx_wasm.h>


/* wasm runtime */


typedef ngx_wrt_engine_pt (*ngx_wrt_engine_new_pt)(ngx_pool_t *pool,
    ngx_wasm_hfuncs_resolver_t *hf_resolver);

typedef ngx_wrt_module_pt (*ngx_wrt_module_new_pt)(
    ngx_wrt_engine_pt engine, ngx_str_t *mod_name, ngx_str_t *bytes,
    ngx_uint_t flags, ngx_wrt_error_pt *err);

typedef ngx_wrt_instance_pt (*ngx_wrt_instance_new_pt)(
    ngx_wrt_module_pt module, ngx_wasm_hctx_t *hctx,
    ngx_wrt_error_pt *err, ngx_wrt_trap_pt *trap);

typedef ngx_int_t (*ngx_wrt_instance_call_pt)(
    ngx_wrt_instance_pt instance, ngx_str_t *func_name,
    ngx_wrt_error_pt *err, ngx_wrt_trap_pt *trap);

typedef ngx_wrt_functype_pt (*ngx_wrt_hfunctype_new_pt)(ngx_wasm_hfunc_t *hfunc);


typedef void (*ngx_wrt_hfunctype_free_pt)(ngx_wrt_functype_pt func);
typedef void (*ngx_wrt_instance_free_pt)(ngx_wrt_instance_pt instance);
typedef void (*ngx_wrt_module_free_pt)(ngx_wrt_module_pt module);
typedef void (*ngx_wrt_engine_free_pt)(ngx_wrt_engine_pt engine);


typedef u_char *(*ngx_wrt_trap_log_handler_pt)(ngx_wrt_error_pt err,
    ngx_wrt_trap_pt trap, u_char *buf, size_t len);


struct ngx_wrt_s {
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
};


/* wasm module */


#define NGX_WASM_MODULE              0x5741534d   /* "WASM" */
#define NGX_WASM_CONF                0x00300000

#define NGX_WASM_MODULE_ISWAT        (1 << 0)
#define NGX_WASM_MODULE_LOADED       (1 << 1)


typedef struct {
    ngx_wrt_t     *runtime;
    void          *(*create_conf)(ngx_cycle_t *cycle);
    char          *(*init_conf)(ngx_cycle_t *cycle, void *conf);
    ngx_int_t      (*init)(ngx_cycle_t *cycle);
} ngx_wasm_module_t;


/* core wasm module ABI */


ngx_wasm_vm_t *ngx_wasm_core_get_vm(ngx_cycle_t *cycle);

void ngx_wasm_core_hfuncs_add(ngx_cycle_t *cycle,
    const ngx_wasm_hfunc_decl_t decls[]);


#endif /* _NGX_WASM_MODULE_H_INCLUDED_ */
