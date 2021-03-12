#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_proxy_wasm.h>
#include <ngx_event.h>
#include <ngx_wavm.h>


#define NGX_PROXY_WASM_PTR_SIZE  4


uint32_t
ngx_proxy_wasm_alloc(ngx_proxy_wasm_module_t *pwm, size_t size)
{
   uint32_t              p;
   ngx_int_t             rc;
   ngx_wavm_instance_t  *instance;
   wasm_val_vec_t        args, *rets;

   instance = pwm->instance;

   wasm_val_vec_new_uninitialized(&args, 1);
   ngx_wasm_vec_set_i32(&args, 0, (uint32_t) size);

   rc = ngx_wavm_instance_call_funcref(instance, pwm->proxy_on_memory_allocate,
                                       &args, &rets);
   if (rc != NGX_OK) {
       ngx_wasm_log_error(NGX_LOG_EMERG, instance->log, 0,
                          "proxy_wasm_alloc(%uz) failed", size);
       wasm_val_vec_delete(&args);
       return 0;
   }

   instance->mem_offset = (u_char *) wasm_memory_data(instance->memory);

   p = rets->data[0].of.i64;

   ngx_log_debug2(NGX_LOG_DEBUG_WASM, instance->log, 0,
                  "proxy_wasm_alloc: %p:%uz", p, size);

   wasm_val_vec_delete(&args);

   return p;
}


ngx_uint_t
ngx_proxy_wasm_pairs_count(ngx_list_t *list)
{
    ngx_uint_t        c;
    ngx_list_part_t  *part;

    part = &list->part;
    c = part->nelts;

    while (part->next != NULL) {
        part = part->next;
        c += part->nelts;
    }

    return c;
}


size_t
ngx_proxy_wasm_pairs_size(ngx_list_t *list)
{
    size_t            i, size;
    ngx_table_elt_t  *elt;
    ngx_list_part_t  *part;

    part = &list->part;
    elt = part->elts;

    size = NGX_PROXY_WASM_PTR_SIZE; /* headers count */

    for (i = 0; /* void */; i++) {

        if (i >= part->nelts) {
            if (part->next == NULL) {
                break;
            }

            part = part->next;
            elt = part->elts;
            i = 0;
        }

        size += NGX_PROXY_WASM_PTR_SIZE * 2;
        size += elt[i].key.len + 1;
        size += elt[i].value.len + 1;
#if 0
        dd("key: %.*s, value: %.*s",
           (int) elt[i].key.len, elt[i].key.data,
           (int) elt[i].value.len, elt[i].value.data);
#endif
    }

    return size;
}


void
ngx_proxy_wasm_marshal_pairs(ngx_list_t *list, u_char *buf)
{
    size_t            i;
    uint32_t          count;
    ngx_table_elt_t  *elt;
    ngx_list_part_t  *part;

    part = &list->part;
    elt = part->elts;

    count = ngx_proxy_wasm_pairs_count(list);
    *((uint32_t *) buf) = count;
    buf += NGX_PROXY_WASM_PTR_SIZE;

    for (i = 0; /* void */; i++) {

        if (i >= part->nelts) {
            if (part->next == NULL) {
                break;
            }

            part = part->next;
            elt = part->elts;
            i = 0;
        }

        *((uint32_t *) buf) = elt[i].key.len;
        buf += NGX_PROXY_WASM_PTR_SIZE;
        *((uint32_t *) buf) = elt[i].value.len;
        buf += NGX_PROXY_WASM_PTR_SIZE;
    }

    for (i = 0; /* void */; i++) {

        if (i >= part->nelts) {
            if (part->next == NULL) {
                break;
            }

            part = part->next;
            elt = part->elts;
            i = 0;
        }

        buf = ngx_cpymem(buf, elt[i].key.data, elt[i].key.len);
        *buf++ = 0;
        buf = ngx_cpymem(buf, elt[i].value.data, elt[i].value.len);
        *buf++ = 0;
    }
}


void
ngx_proxy_wasm_tick_handler(ngx_event_t *ev)
{
    ngx_int_t                 rc;
    ngx_proxy_wasm_module_t  *pwm = ev->data;
    wasm_val_vec_t            args;

    ngx_free(ev);

    if (pwm->proxy_on_tick) {
        ngx_wavm_ctx_update(pwm->instance->ctx, NULL, NULL);

        wasm_val_vec_new_uninitialized(&args, 1);
        ngx_wasm_vec_set_i32(&args, 0, pwm->ctxid);

        rc = ngx_wavm_instance_call_funcref(pwm->instance, pwm->proxy_on_tick,
                                            &args, NULL);
        wasm_val_vec_delete(&args);

        if (!ngx_exiting) {
            ev = ngx_calloc(sizeof(ngx_event_t), pwm->instance->log);
            if (ev == NULL) {
                goto nomem;
            }

            ev->handler = ngx_proxy_wasm_tick_handler;
            ev->data = pwm;
            ev->log = pwm->log;

            ngx_add_timer(ev, pwm->tick_period);
        }

        if (rc != NGX_OK) {
            return;
        }
    }

    return;

nomem:

    ngx_wasm_log_error(NGX_LOG_EMERG, pwm->instance->log, 0,
                       "tick_handler: no memory");
}
