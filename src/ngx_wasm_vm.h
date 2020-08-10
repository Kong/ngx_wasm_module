/*
 * Copyright (C) Thibault Charbonnier
 */

#ifndef _NGX_WASM_VM_H_INCLUDED_
#define _NGX_WASM_VM_H_INCLUDED_


#include <ngx_wasm.h>


ngx_wasm_vm_t *ngx_wasm_vm_new(char *vm_name, ngx_cycle_t *cycle,
    ngx_wasm_hfuncs_store_t *hf_store);

void ngx_wasm_vm_free(ngx_wasm_vm_t *vm);

ngx_int_t ngx_wasm_vm_add_module(ngx_wasm_vm_t *vm, u_char *mod_name,
    u_char *path);

ngx_int_t ngx_wasm_vm_has_module(ngx_wasm_vm_t *vm, u_char *mod_name);

ngx_int_t ngx_wasm_vm_init_runtime(ngx_wasm_vm_t *vm, ngx_str_t *vm_name,
    ngx_wasm_vm_actions_t *vm_actions);

ngx_int_t ngx_wasm_vm_load_module(ngx_wasm_vm_t *vm, ngx_str_t *mod_name);

ngx_int_t ngx_wasm_vm_load_modules(ngx_wasm_vm_t *vm);

ngx_wasm_instance_t *ngx_wasm_vm_instance_new(ngx_wasm_vm_t *vm,
    ngx_str_t *mod_name);

void ngx_wasm_vm_instance_bind_request(ngx_wasm_instance_t *instance,
    ngx_http_request_t *r);

ngx_int_t ngx_wasm_vm_instance_call(ngx_wasm_instance_t *instance,
    ngx_str_t *func_name);

void ngx_wasm_vm_instance_free(ngx_wasm_instance_t *instance);


#endif /* _NGX_WASM_VM_H_INCLUDED_ */
