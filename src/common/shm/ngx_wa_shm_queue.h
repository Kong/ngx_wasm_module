#ifndef _NGX_WA_SHM_QUEUE_H_INCLUDED_
#define _NGX_WA_SHM_QUEUE_H_INCLUDED_


#include <ngx_wa_shm.h>


typedef void *(*ngx_wa_shm_queue_alloc_pt)(size_t size, void *alloc_ctx);


ngx_int_t ngx_wa_shm_queue_init(ngx_wa_shm_t *shm);
ngx_int_t ngx_wa_shm_queue_push_locked(ngx_wa_shm_t *shm, ngx_str_t *data);
ngx_int_t ngx_wa_shm_queue_pop_locked(ngx_wa_shm_t *shm,
    ngx_str_t *data_out, ngx_wa_shm_queue_alloc_pt alloc, void *alloc_ctx);
ngx_int_t ngx_wa_shm_queue_resolve(ngx_log_t *log, uint32_t token,
    ngx_shm_zone_t **out);


#endif /* _NGX_WA_SHM_QUEUE_H_INCLUDED_ */
