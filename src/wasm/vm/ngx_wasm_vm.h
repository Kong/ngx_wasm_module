#ifndef _NGX_WASM_VM_H_INCLUDED_
#define _NGX_WASM_VM_H_INCLUDED_


#include <ngx_core.h>
#include <ngx_wasm.h>


#define ngx_wasm_vm_abi_version         0000001


ngx_wasm_vm_t *ngx_wasm_vm_new(ngx_cycle_t *cycle, ngx_str_t *vm_name);

void ngx_wasm_vm_free(ngx_wasm_vm_t *vm);

ngx_int_t ngx_wasm_vm_add_module(ngx_wasm_vm_t *vm, ngx_str_t *mod_name,
    ngx_str_t *path);

ngx_uint_t ngx_wasm_vm_has_module(ngx_wasm_vm_t *vm, ngx_str_t *mod_name);

ngx_int_t ngx_wasm_vm_init(ngx_wasm_vm_t *vm, ngx_wrt_t *runtime,
    ngx_wasm_hfuncs_t *hfuncs);

ngx_int_t ngx_wasm_vm_load_module(ngx_wasm_vm_t *vm, ngx_str_t *mod_name);

ngx_wasm_vm_instance_t *ngx_wasm_vm_instance_new(ngx_wasm_vm_t *vm,
    ngx_str_t *mod_name);

void ngx_wasm_vm_instance_set_hctx(ngx_wasm_vm_instance_t *instance,
    ngx_wasm_hctx_t *hctx);

ngx_int_t ngx_wasm_vm_instance_call(ngx_wasm_vm_instance_t *instance,
    ngx_str_t *func_name);

void ngx_wasm_vm_instance_free(ngx_wasm_vm_instance_t *instance);


#endif /* _NGX_WASM_VM_H_INCLUDED_ */
