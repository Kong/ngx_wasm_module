#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_wasm.h>
#include <ngx_wasm_shm_queue.h>


typedef void *(*ngx_wasm_shm_queue_alloc_pt)(ngx_uint_t n, void *alloc_ctx);


typedef struct {
    uint8_t     *buffer;
    uint8_t     *buffer_end;
    ngx_uint_t   push_ptr;
    ngx_uint_t   pop_ptr;
    ngx_uint_t   rising_occupancy;
} ngx_wasm_shm_queue_t;


static ngx_inline ngx_wasm_shm_queue_t *
ngx_wasm_shm_get_queue(ngx_wasm_shm_t *shm)
{
    ngx_wasm_assert(shm->type == NGX_WASM_SHM_TYPE_QUEUE);
    return shm->data;
}


static ngx_inline ngx_uint_t
queue_capacity(ngx_wasm_shm_queue_t *queue)
{
    return queue->buffer_end - queue->buffer;
}


static ngx_inline ngx_uint_t
queue_occupancy(ngx_wasm_shm_queue_t *queue)
{
    if (queue->push_ptr > queue->pop_ptr) {
        return queue->push_ptr - queue->pop_ptr;
    }

    if (queue->push_ptr < queue->pop_ptr) {
        return queue_capacity(queue) - (queue->pop_ptr - queue->push_ptr);
    }

    return queue->rising_occupancy ? queue_capacity(queue) : 0;
}


static ngx_inline void
inc_ptr(ngx_wasm_shm_queue_t *queue, ngx_uint_t *ptr, ngx_uint_t n)
{
    ngx_uint_t  new_ptr = *ptr + n;
    ngx_uint_t  cap = queue_capacity(queue);

    if (new_ptr >= cap) {
        new_ptr = new_ptr - cap;
        ngx_wasm_assert(new_ptr < cap);
    }

    *ptr = new_ptr;
}


static ngx_inline void
circular_write(ngx_log_t *log, ngx_wasm_shm_queue_t *queue,
               ngx_uint_t ptr, void *data, ngx_uint_t data_size)
{
    ngx_uint_t  cap = queue_capacity(queue);
    ngx_uint_t  forward_capacity = cap - ptr;

    ngx_wasm_assert(data_size <= cap - queue_occupancy(queue));
    ngx_wasm_assert(ptr < cap);

    dd("circular_write forward_capacity: %lu data_size: %lu", forward_capacity, data_size);

    if (data_size >= forward_capacity) {
        ngx_memcpy(queue->buffer + ptr, data, forward_capacity);
        ngx_memcpy(queue->buffer, (uint8_t *) data + forward_capacity, data_size - forward_capacity);

        ngx_wasm_log_error(NGX_LOG_DEBUG, log, 0,
                           "circular_write: wrapping around");

    } else {
        ngx_memcpy(queue->buffer + ptr, data, data_size);
    }
}


static ngx_inline void
circular_read(ngx_log_t *log, ngx_wasm_shm_queue_t *queue,
              ngx_uint_t ptr, void *data_out, ngx_uint_t data_size)
{
    ngx_uint_t  cap = queue_capacity(queue);
    ngx_uint_t  forward_capacity = cap - ptr;

    ngx_wasm_assert(data_size <= queue_occupancy(queue));
    ngx_wasm_assert(ptr < cap);

    if (data_size >= forward_capacity) {
        ngx_memcpy(data_out, queue->buffer + ptr, forward_capacity);
        ngx_memcpy((uint8_t *) data_out + forward_capacity, queue->buffer,
                   data_size - forward_capacity);

        ngx_wasm_log_error(NGX_LOG_DEBUG, log, 0,
                           "circular_read: wrapping around");

    } else {
        ngx_memcpy(data_out, queue->buffer + ptr, data_size);
    }
}


static ngx_inline void
check_queue_invariance(ngx_wasm_shm_queue_t *queue)
{
#if (NGX_DEBUG)
    ngx_uint_t  cap = queue_capacity(queue);

    ngx_wasm_assert(queue->push_ptr < cap);
    ngx_wasm_assert(queue->pop_ptr < cap);
#endif
}


ngx_int_t
ngx_wasm_shm_queue_init(ngx_wasm_shm_t *shm)
{
    ngx_wasm_shm_queue_t  *queue;
    ngx_uint_t             buffer_size;
    ngx_uint_t             reserved_size = ngx_pagesize;

    queue = ngx_slab_calloc(shm->shpool, sizeof(ngx_wasm_shm_queue_t));
    if (queue == NULL) {
        dd("failed to allocate queue structure");
        return NGX_ERROR;
    }

    buffer_size = shm->shpool->end - shm->shpool->start;
    ngx_wasm_assert(buffer_size > reserved_size);
    buffer_size -= reserved_size;
    queue->buffer = ngx_slab_calloc(shm->shpool, buffer_size);

    if (queue->buffer == NULL) {
        dd("failed to allocate queue buffer");
        return NGX_ERROR;
    }

    ngx_wasm_log_error(NGX_LOG_DEBUG, shm->log, 0,
                       "shm queue \"%V\": available buffer size is %l bytes",
                       &shm->name, buffer_size);

    queue->buffer_end = queue->buffer + buffer_size;
    check_queue_invariance(queue);

    shm->data = queue;
    return NGX_OK;
}


ngx_int_t
ngx_wasm_shm_queue_push_locked(ngx_wasm_shm_t *shm, ngx_str_t *data)
{
    ngx_wasm_shm_queue_t  *queue = ngx_wasm_shm_get_queue(shm);
    ngx_uint_t             entry_size = sizeof(uint32_t) + data->len;
    uint32_t               len = (uint32_t) data->len;

    /* Queue full? */

    if (queue_occupancy(queue) + entry_size > queue_capacity(queue)) {
        ngx_wasm_log_error(NGX_LOG_ERR, shm->log, 0,
                           "shm queue \"%V\": queue full, cannot push",
                           &shm->name);
        return NGX_ERROR;
    }

    dd("pre-push ptr=%lu len=%u", queue->push_ptr, len);

    circular_write(shm->log, queue, queue->push_ptr, &len, sizeof(uint32_t));
    inc_ptr(queue, &queue->push_ptr, sizeof(uint32_t));
    circular_write(shm->log, queue, queue->push_ptr, data->data, data->len);
    inc_ptr(queue, &queue->push_ptr, data->len);
    queue->rising_occupancy = 1;

    dd("post-push ptr=%lu", queue->push_ptr);

    check_queue_invariance(queue);
    return NGX_OK;
}


ngx_uint_t
ngx_wasm_shm_queue_pop_locked(ngx_wasm_shm_t *shm, ngx_str_t *data_out,
    ngx_wasm_shm_queue_alloc_pt alloc, void *alloc_ctx)
{
    ngx_wasm_shm_queue_t  *queue = ngx_wasm_shm_get_queue(shm);
    uint32_t               len;
    void                  *buf = NULL;

    /* Queue empty? */

    if (queue_occupancy(queue) < sizeof(uint32_t)) {
        return NGX_AGAIN;
    }

    circular_read(shm->log, queue, queue->pop_ptr, &len, sizeof(uint32_t));

    /* Defer `inc_ptr` after allocation to ensure atomicity */

    if (len != 0) {
        buf = alloc(len, alloc_ctx);
        if (buf == NULL) {
            return NGX_ERROR;
        }
    }

    inc_ptr(queue, &queue->pop_ptr, sizeof(uint32_t));
    circular_read(shm->log, queue, queue->pop_ptr, buf, len);
    inc_ptr(queue, &queue->pop_ptr, len);
    queue->rising_occupancy = 0;

    data_out->data = buf;
    data_out->len = len;

    check_queue_invariance(queue);
    return NGX_OK;
}
