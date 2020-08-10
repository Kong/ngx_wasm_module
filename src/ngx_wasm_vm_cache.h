/*
 * Copyright (C) Thibault Charbonnier
 */

#ifndef _NGX_WASM_VM_CACHE_H_INCLUDED_
#define _NGX_WASM_VM_CACHE_H_INCLUDED_


#include <ngx_wasm.h>
#include <ngx_wasm_vm.h>


typedef struct ngx_wasm_vm_cache_s {
    ngx_rbtree_t        rbtree;
    ngx_rbtree_node_t   sentinel;
    ngx_pool_t         *pool;
    ngx_wasm_vm_t      *vm;
} ngx_wasm_vm_cache_t;


void ngx_wasm_vm_cache_init(ngx_wasm_vm_cache_t *cache);

ngx_wasm_instance_t *ngx_wasm_vm_cache_get_instance(ngx_wasm_vm_cache_t *cache,
    ngx_str_t *mod_name);

void ngx_wasm_vm_cache_cleanup(ngx_wasm_vm_cache_t *cache);


#endif /* _NGX_WASM_VM_CACHE_H_INCLUDED_ */
