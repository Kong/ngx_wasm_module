#ifndef _NGX_WASM_VM_HOST_H_INCLUDED_
#define _NGX_WASM_VM_HOST_H_INCLUDED_


#include <ngx_core.h>
#include <ngx_wasm_core.h>


typedef struct {
    ngx_str_node_t                 sn;
    ngx_wasm_hfunc_t              *hfunc;
} ngx_wasm_hfuncs_fnode_t;


typedef struct {
    ngx_str_t                      name;
    ngx_rbtree_t                   ftree;
    ngx_rbtree_node_t              sentinel;
    ngx_str_node_t                 sn;
} ngx_wasm_hfuncs_mnode_t;


struct ngx_wasm_hfuncs_s {
    ngx_cycle_t                   *cycle;
    ngx_pool_t                    *pool;
    ngx_rbtree_t                   mtree;
    ngx_rbtree_node_t              sentinel;
    ngx_wrt_hfunctype_new_pt       hfunctype_new;
    ngx_wrt_hfunctype_free_pt      hfunctype_free;
};


ngx_wasm_hfuncs_t *ngx_wasm_hfuncs_new(ngx_cycle_t *cycle);

void ngx_wasm_hfuncs_add(ngx_wasm_hfuncs_t *hfuncs, u_char *mname,
    ngx_wasm_hdefs_t *hdefs);

void ngx_wasm_hfuncs_init(ngx_wasm_hfuncs_t *hfuncs, ngx_wrt_t *runtime);

ngx_wasm_hfunc_t *ngx_wasm_hfuncs_lookup(ngx_wasm_hfuncs_t *hfuncs,
    u_char *mname, size_t mlen, u_char *fname, size_t flen);

void ngx_wasm_hfuncs_free(ngx_wasm_hfuncs_t *hfuncs);

void ngx_wasm_hctx_trapmsg(ngx_wasm_hctx_t *hctx,
#if (NGX_HAVE_VARIADIC_MACROS)
    const char *fmt, ...);

#else
    const char *fmt, va_list args);
#endif


#endif /* _NGX_WASM_VM_HOST_H_INCLUDED_ */
