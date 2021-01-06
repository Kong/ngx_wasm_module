#ifndef _NGX_WASM_VM_CACHE_H_INCLUDED_
#define _NGX_WASM_VM_CACHE_H_INCLUDED_


#include <ngx_core.h>
#include <ngx_wasm.h>


typedef struct {
    ngx_rbtree_t          rbtree;
    ngx_rbtree_node_t     sentinel;
    ngx_pool_t           *pool;
    ngx_wasm_vm_t        *vm;
} ngx_wasm_vm_cache_t;


#define ngx_wasm_vm_cache_init(cache)                                        \
    (cache)->rbtree.root = NULL

ngx_wasm_vm_instance_t *ngx_wasm_vm_cache_get_instance(
    ngx_wasm_vm_cache_t *cache, ngx_wasm_vm_t *vm,
    ngx_str_t *mod_name);

void ngx_wasm_vm_cache_free(ngx_wasm_vm_cache_t *cache);


#endif /* _NGX_WASM_VM_CACHE_H_INCLUDED_ */
