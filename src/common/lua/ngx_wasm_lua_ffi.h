#ifndef _NGX_WASM_LUA_FFI_H_INCLUDED_
#define _NGX_WASM_LUA_FFI_H_INCLUDED_


#include <ngx_wavm.h>
#include <ngx_wasm_ops.h>
#include <ngx_proxy_wasm.h>
#include <ngx_proxy_wasm_properties.h>
#ifdef NGX_WASM_HTTP
#include <ngx_http_proxy_wasm.h>
#endif


ngx_wavm_t *ngx_wasm_ffi_main_vm();


#ifdef NGX_WASM_HTTP
typedef struct {
    ngx_str_t       *name;
    ngx_str_t       *config;
} ngx_wasm_ffi_filter_t;


typedef struct {
    ngx_http_request_t  *r;
    lua_State           *L;
    unsigned             is_getter;
} ngx_wasm_ffi_host_property_ctx_t;


ngx_int_t ngx_http_wasm_ffi_plan_new(ngx_wavm_t *vm,
    ngx_wasm_ffi_filter_t *filters, size_t n_filters,
    ngx_wasm_ops_plan_t **out, u_char *err, size_t *errlen);
void ngx_http_wasm_ffi_plan_free(ngx_wasm_ops_plan_t *plan);
ngx_int_t ngx_http_wasm_ffi_plan_load(ngx_wasm_ops_plan_t *plan);
ngx_int_t ngx_http_wasm_ffi_plan_attach(ngx_http_request_t *r,
    ngx_wasm_ops_plan_t *plan, ngx_uint_t isolation);
ngx_int_t ngx_http_wasm_ffi_start(ngx_http_request_t *r);
ngx_int_t ngx_http_wasm_ffi_set_property(ngx_http_request_t *r,
    ngx_str_t *key, ngx_str_t *value);
ngx_int_t ngx_http_wasm_ffi_get_property(ngx_http_request_t *r,
    ngx_str_t *key, ngx_str_t *value);
ngx_int_t ngx_http_wasm_ffi_set_host_property(ngx_http_request_t *r,
    ngx_str_t *key, ngx_str_t *value, unsigned is_const, unsigned retrieve);
ngx_int_t ngx_http_wasm_ffi_set_property_setter(ngx_http_request_t *r,
    ngx_wasm_host_prop_fn_t fn);
ngx_int_t ngx_http_wasm_ffi_set_property_getter(ngx_http_request_t *r,
    ngx_wasm_host_prop_fn_t fn);
#endif


#endif /* _NGX_WASM_LUA_FFI_H_INCLUDED_ */
