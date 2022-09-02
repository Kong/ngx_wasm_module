#ifndef _NGX_WASM_SHM_QUEUE_H_INCLUDED_
#define _NGX_WASM_SHM_QUEUE_H_INCLUDED_


#include <ngx_wasm_shm.h>


ngx_int_t ngx_wasm_shm_queue_init(ngx_wasm_shm_t *shm);
ngx_int_t ngx_wasm_shm_queue_push_locked(ngx_wasm_shm_t *shm, ngx_str_t *data);
ngx_uint_t ngx_wasm_shm_queue_pop_locked(ngx_wasm_shm_t *shm,
    ngx_str_t *data_out, void *(*alloc)(ngx_uint_t n, void *alloc_ctx),
    void *alloc_ctx);

#endif
