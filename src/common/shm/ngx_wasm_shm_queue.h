#ifndef _NGX_WASM_SHM_QUEUE_H_INCLUDED_
#define _NGX_WASM_SHM_QUEUE_H_INCLUDED_


#include <ngx_wasm_shm.h>


typedef void *(*ngx_wasm_shm_queue_alloc_pt)(size_t size, void *alloc_ctx);


ngx_int_t ngx_wasm_shm_queue_init(ngx_wasm_shm_t *shm);
ngx_int_t ngx_wasm_shm_queue_push_locked(ngx_wasm_shm_t *shm, ngx_str_t *data);
ngx_int_t ngx_wasm_shm_queue_pop_locked(ngx_wasm_shm_t *shm,
    ngx_str_t *data_out, ngx_wasm_shm_queue_alloc_pt alloc, void *alloc_ctx);
ngx_int_t ngx_wasm_shm_queue_resolve(ngx_log_t *log, uint32_t token,
    ngx_shm_zone_t **out);


#endif /* _NGX_WASM_SHM_QUEUE_H_INCLUDED_ */
