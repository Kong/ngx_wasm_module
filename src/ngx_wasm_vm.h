/*
 * Copyright (C) Thibault Charbonnier
 */

#ifndef _NGX_WASM_VM_H_INCLUDED_
#define _NGX_WASM_VM_H_INCLUDED_


#include <ngx_wasm.h>


ngx_wasm_vm_pt ngx_wasm_vm_new(const char *name, ngx_log_t *log);

void ngx_wasm_vm_free(ngx_wasm_vm_pt vm);

ngx_int_t ngx_wasm_vm_add_module(ngx_wasm_vm_pt vm, u_char *name, u_char *path);

ngx_int_t ngx_wasm_vm_has_module(ngx_wasm_vm_pt vm, u_char *name);

ngx_int_t ngx_wasm_vm_init(ngx_wasm_vm_pt vm,
    ngx_wasm_vm_actions_t *vm_actions);

ngx_int_t ngx_wasm_vm_load_module(ngx_wasm_vm_pt vm, u_char *name);

ngx_int_t ngx_wasm_vm_load_modules(ngx_wasm_vm_pt vm);

ngx_wasm_instance_pt ngx_wasm_vm_new_instance(ngx_wasm_vm_pt vm,
    u_char *module_name);

void ngx_wasm_vm_free_instance(ngx_wasm_instance_pt instance);


#endif /* _NGX_WASM_VM_H_INCLUDED_ */


/* vi:set ft=c ts=4 sw=4 et fdm=marker: */
