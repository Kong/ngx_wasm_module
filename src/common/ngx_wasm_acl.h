#ifndef _NGX_WASM_ACL_H_INCLUDED_
#define _NGX_WASM_ACL_H_INCLUDED_

#include <ngx_core.h>
#include <ngx_wasm.h>
#include <ngx_proxy_wasm.h>


typedef enum {
    NGX_WASM_ACL_ALLOW = 1,
    NGX_WASM_ACL_DENY  = 2,
} ngx_wasm_acl_type_e;


typedef struct {
    ngx_str_t              host;
    ngx_uint_t             wildcard;
    ngx_wasm_acl_type_e    type;
} ngx_wasm_acl_host_t;


struct ngx_wasm_acl_ctx_s {
    ngx_str_t              name;
    ngx_radix_tree_t      *addrs_v4;
    ngx_radix_tree_t      *addrs_v6;
    ngx_array_t            hosts_arr;       /* ngx_wasm_acl_host_t */
    ngx_hash_combined_t    hosts_hash;
};


struct ngx_wasm_acl_s {
    ngx_wasm_acl_ctx_t    *ctx;
    ngx_array_t            recorded_addrs;  /* ngx_cidr_t */
};


ngx_wasm_acl_t *ngx_wasm_acl_get(ngx_proxy_wasm_exec_t *pwexec);
ngx_wasm_acl_ctx_t *ngx_wasm_acl_find_ctx(ngx_cycle_t *cycle, ngx_str_t *name);
ngx_int_t ngx_wasm_acl_init(ngx_wasm_acl_t *acl, ngx_wasm_acl_ctx_t *ctx,
    ngx_pool_t *pool);
ngx_int_t ngx_wasm_acl_ctx_init(ngx_conf_t *cf, ngx_wasm_acl_ctx_t *ctx);
ngx_int_t ngx_wasm_acl_ctx_insert_cidr(ngx_wasm_acl_ctx_t *acl_ctx, ngx_cidr_t *cidr,
    ngx_wasm_acl_type_e type);
ngx_int_t ngx_wasm_acl_check_addr(ngx_wasm_acl_t *acl, struct sockaddr *addr);
ngx_int_t ngx_wasm_acl_check_host(ngx_wasm_acl_t *acl, ngx_str_t *host);
ngx_int_t ngx_wasm_acl_add_addr(ngx_wasm_acl_t *acl, struct sockaddr *addr);

#endif //_NGX_WASM_ACL_H_INCLUDED_
