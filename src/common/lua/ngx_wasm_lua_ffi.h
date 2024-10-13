#ifndef _NGX_WASM_LUA_FFI_H_INCLUDED_
#define _NGX_WASM_LUA_FFI_H_INCLUDED_


#include <ngx_wavm.h>
#include <ngx_wasm_ops.h>
#include <ngx_proxy_wasm.h>
#include <ngx_proxy_wasm_properties.h>
#ifdef NGX_WASM_HTTP
#include <ngx_http_proxy_wasm.h>
#endif


#define NGX_WASM_LUA_FFI_MAX_ERRLEN  256


ngx_wavm_t *ngx_wasm_ffi_main_vm();


#ifdef NGX_WASM_HTTP
typedef struct {
    ngx_str_t       *name;
    ngx_str_t       *config;
} ngx_wasm_ffi_filter_t;


typedef void (*ngx_wa_ffi_shm_setup_zones_handler_pt)(ngx_wa_shm_t *shm);


ngx_int_t ngx_http_wasm_ffi_plan_new(ngx_wavm_t *vm,
    ngx_wasm_ffi_filter_t *filters, size_t n_filters,
    ngx_wasm_ops_plan_t **out, u_char *err, size_t *errlen);
void ngx_http_wasm_ffi_plan_free(ngx_wasm_ops_plan_t *plan);
ngx_int_t ngx_http_wasm_ffi_plan_load(ngx_wasm_ops_plan_t *plan);
ngx_int_t ngx_http_wasm_ffi_plan_attach(ngx_http_request_t *r,
    ngx_wasm_ops_plan_t *plan, ngx_uint_t isolation);
ngx_int_t ngx_http_wasm_ffi_set_property(ngx_http_request_t *r,
    ngx_str_t *key, ngx_str_t *value, u_char *err, size_t *errlen);
ngx_int_t ngx_http_wasm_ffi_get_property(ngx_http_request_t *r,
    ngx_str_t *key, ngx_str_t *value, u_char *err, size_t *errlen);
ngx_int_t ngx_http_wasm_ffi_set_host_property(ngx_http_request_t *r,
    ngx_str_t *key, ngx_str_t *value, unsigned is_const, unsigned retrieve);
ngx_int_t ngx_http_wasm_ffi_set_host_properties_handlers(ngx_http_request_t *r,
    ngx_proxy_wasm_properties_ffi_handler_pt getter,
    ngx_proxy_wasm_properties_ffi_handler_pt setter);
#endif


ngx_int_t ngx_wa_ffi_shm_setup_zones(
    ngx_wa_ffi_shm_setup_zones_handler_pt handler);
ngx_int_t ngx_wa_ffi_shm_iterate_keys(ngx_wa_shm_t *shm, ngx_uint_t page_size,
    ngx_uint_t *clast_idx, ngx_uint_t *cur_idx, ngx_str_t **keys);

ngx_uint_t ngx_wa_ffi_shm_kv_nelts(ngx_wa_shm_t *shm);
ngx_int_t ngx_wa_ffi_shm_kv_get(ngx_wa_shm_t *shm, ngx_str_t *k,
    ngx_str_t **v, uint32_t *cas);
ngx_int_t ngx_wa_ffi_shm_kv_set(ngx_wa_shm_t *shm, ngx_str_t *k,
    ngx_str_t *v, uint32_t cas, unsigned *written);

ngx_int_t ngx_wa_ffi_shm_metric_define(ngx_str_t *name,
    ngx_wa_metric_type_e type, uint32_t *bins, uint16_t n_bins,
    uint32_t *metric_id);
ngx_int_t ngx_wa_ffi_shm_metric_increment(uint32_t metric_id, ngx_uint_t value);
ngx_int_t ngx_wa_ffi_shm_metric_record(uint32_t metric_id, ngx_uint_t value);
ngx_int_t ngx_wa_ffi_shm_metric_get(uint32_t metric_id, ngx_str_t *name,
    u_char *m_buf, size_t mbs, u_char *h_buf, size_t hbs);


void
ngx_wa_ffi_shm_lock(ngx_wa_shm_t *shm)
{
    ngx_wa_shm_lock(shm);
}


void
ngx_wa_ffi_shm_unlock(ngx_wa_shm_t *shm)
{
    ngx_wa_shm_unlock(shm);
}


ngx_int_t
ngx_wa_ffi_shm_metrics_one_slot_size()
{
    return NGX_WA_METRICS_ONE_SLOT_SIZE;
}


ngx_int_t
ngx_wa_ffi_shm_metrics_histogram_max_size()
{
    return NGX_WA_METRICS_HISTOGRAM_MAX_SIZE;
}


ngx_int_t
ngx_wa_ffi_shm_metrics_histogram_max_bins()
{
    return NGX_WA_METRICS_HISTOGRAM_BINS_MAX;
}


#endif /* _NGX_WASM_LUA_FFI_H_INCLUDED_ */
