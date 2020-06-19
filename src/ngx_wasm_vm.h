/*
 * Copyright (C) Thibault Charbonnier
 */

#ifndef _NGX_WASM_VM_H_INCLUDED_
#define _NGX_WASM_VM_H_INCLUDED_


#include <ngx_wasm.h>


ngx_wasm_vm_pt ngx_wasm_vm_new(const char *name, ngx_log_t *log);

void ngx_wasm_vm_free(ngx_wasm_vm_pt vm);

ngx_int_t ngx_wasm_vm_add_module(ngx_wasm_vm_pt vm, u_char *mod_name,
    u_char *path);

ngx_int_t ngx_wasm_vm_has_module(ngx_wasm_vm_pt vm, u_char *mod_name);

ngx_int_t ngx_wasm_vm_init_runtime(ngx_wasm_vm_pt vm, ngx_str_t *vm_name,
    ngx_wasm_vm_actions_t *vm_actions);

ngx_int_t ngx_wasm_vm_load_module(ngx_wasm_vm_pt vm, u_char *mod_name);

ngx_int_t ngx_wasm_vm_load_modules(ngx_wasm_vm_pt vm);

ngx_int_t ngx_wasm_vm_call_by_name(ngx_wasm_vm_pt vm, ngx_str_t *mod_name,
    ngx_str_t *func_name);


#endif /* _NGX_WASM_VM_H_INCLUDED_ */


/* vi:set ft=c ts=4 sw=4 et fdm=marker: */
