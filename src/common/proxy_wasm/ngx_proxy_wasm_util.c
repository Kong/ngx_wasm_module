#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_proxy_wasm.h>
#include <ngx_event.h>
#include <ngx_wavm.h>


#define NGX_PROXY_WASM_PTR_SIZE  4


static ngx_str_t  ngx_proxy_wasm_errlist[] = {
    ngx_null_string,
    ngx_string("unknown ABI version"),
    ngx_string("incompatible ABI version"),
    ngx_string("incompatible host interface"),
    ngx_string("incompatible SDK interface"),
    ngx_string("instantiation failed"),
    ngx_string("instance trapped"),
    ngx_string("initialization failed"),
    ngx_string("unknown error")
};


static ngx_inline ngx_str_t *
ngx_proxy_wasm_filter_strerror(ngx_proxy_wasm_err_e err)
{
    ngx_str_t  *msg;

    msg = ((ngx_uint_t) err < NGX_PROXY_WASM_ERR_UNKNOWN)
              ? &ngx_proxy_wasm_errlist[err]
              : &ngx_proxy_wasm_errlist[NGX_PROXY_WASM_ERR_UNKNOWN];

    return msg;
}


/* utils */


ngx_uint_t
ngx_proxy_wasm_pairs_count(ngx_list_t *list)
{
    size_t            i, c = 0;
    ngx_list_part_t  *part;
    ngx_table_elt_t  *elt;

    part = &list->part;
    elt = part->elts;

    for (i = 0; /* void */; i++) {

        if (i >= part->nelts) {
            if (part->next == NULL) {
                break;
            }

            part = part->next;
            elt = part->elts;
            i = 0;
        }

        if (elt[i].hash == 0) {
            continue;
        }

        c++;
    }

    return c;
}


size_t
ngx_proxy_wasm_pairs_size(ngx_list_t *list, ngx_array_t *extras, ngx_uint_t max)
{
    size_t            i, n, size;
    ngx_list_part_t  *part;
    ngx_table_elt_t  *elt;

    part = &list->part;
    elt = part->elts;

    size = NGX_PROXY_WASM_PTR_SIZE; /* headers count */

    for (i = 0, n = 0; /* void */; i++) {

        if (i >= part->nelts) {
            if (part->next == NULL) {
                break;
            }

            part = part->next;
            elt = part->elts;
            i = 0;
        }

        if (elt[i].hash == 0) {
            continue;
        }

        size += NGX_PROXY_WASM_PTR_SIZE * 2;
        size += elt[i].key.len + 1;
        size += elt[i].value.len + 1;

#if 0
        dd("key: %.*s, value: %.*s, size: %lu",
           (int) elt[i].key.len, elt[i].key.data,
           (int) elt[i].value.len, elt[i].value.data, size);
#endif

        n++;

        if (max && n >= max) {
            break;
        }
    }

    if (extras) {
        elt = extras->elts;

        for (i = 0; i < extras->nelts; i++, n++) {
            size += NGX_PROXY_WASM_PTR_SIZE * 2;
            size += elt[i].key.len + 1;
            size += elt[i].value.len + 1;

#if 0
            dd("extra key: %.*s, extra value: %.*s, size: %lu",
               (int) elt[i].key.len, elt[i].key.data,
               (int) elt[i].value.len, elt[i].value.data, size);
#endif

            if (max && n >= max) {
                break;
            }
        }
    }

    return size;
}


void
ngx_proxy_wasm_pairs_marshal(ngx_list_t *list, ngx_array_t *extras, u_char *buf,
    ngx_uint_t max, ngx_uint_t *truncated)
{
    size_t            i, n = 0;
    uint32_t          count;
    ngx_table_elt_t  *elt;
    ngx_list_part_t  *part;

    count = ngx_proxy_wasm_pairs_count(list);

    if (extras) {
        count += extras->nelts;
    }

    if (max && count > max) {
        count = max;

        if (truncated) {
            *truncated = count;
        }
    }

    *((uint32_t *) buf) = count;
    buf += NGX_PROXY_WASM_PTR_SIZE;

    if (extras) {
        elt = extras->elts;

        for (i = 0; i < extras->nelts && n < count; i++, n++) {
            *((uint32_t *) buf) = elt[i].key.len;
            buf += NGX_PROXY_WASM_PTR_SIZE;
            *((uint32_t *) buf) = elt[i].value.len;
            buf += NGX_PROXY_WASM_PTR_SIZE;
        }
    }

    part = &list->part;
    elt = part->elts;

    for (i = 0; n < count; i++) {

        if (i >= part->nelts) {
            if (part->next == NULL) {
                break;
            }

            part = part->next;
            elt = part->elts;
            i = 0;
        }

       if (elt[i].hash == 0) {
           continue;
       }

        *((uint32_t *) buf) = elt[i].key.len;
        buf += NGX_PROXY_WASM_PTR_SIZE;
        *((uint32_t *) buf) = elt[i].value.len;
        buf += NGX_PROXY_WASM_PTR_SIZE;

        n++;
    }

    ngx_wasm_assert(n == count);

    n = 0;

    if (extras) {
        elt = extras->elts;

        for (i = 0; i < extras->nelts && n < count; i++, n++) {
            buf = ngx_cpymem(buf, elt[i].key.data, elt[i].key.len);
            *buf++ = '\0';
            buf = ngx_cpymem(buf, elt[i].value.data, elt[i].value.len);
            *buf++ = '\0';
        }
    }

    part = &list->part;
    elt = part->elts;

    for (i = 0; n < count; i++) {

        if (i >= part->nelts) {
            if (part->next == NULL) {
                break;
            }

            part = part->next;
            elt = part->elts;
            i = 0;
        }

        if (elt[i].hash == 0) {
           continue;
        }

        buf = ngx_cpymem(buf, elt[i].key.data, elt[i].key.len);
        *buf++ = '\0';
        buf = ngx_cpymem(buf, elt[i].value.data, elt[i].value.len);
        *buf++ = '\0';

        n++;
    }

    ngx_wasm_assert(n == count);
}


ngx_array_t *
ngx_proxy_wasm_pairs_unmarshal(ngx_pool_t *pool, u_char *buf, size_t len)
{
    size_t            i;
    uint32_t          count;
    ngx_table_elt_t  *elt;
    ngx_array_t      *a;

    count = *((uint32_t *) buf);
    buf += NGX_PROXY_WASM_PTR_SIZE;

    a = ngx_array_create(pool, count, sizeof(ngx_table_elt_t));
    if (a == NULL) {
        return NULL;
    }

    for (i = 0; i < count; i++) {
        elt = ngx_array_push(a);
        if (elt == NULL) {
            goto failed;
        }

        elt->hash = 0;
        elt->lowcase_key = NULL;

        elt->key.len = *((uint32_t *) buf);
        buf += NGX_PROXY_WASM_PTR_SIZE;
        elt->value.len = *((uint32_t *) buf);
        buf += NGX_PROXY_WASM_PTR_SIZE;
    }

    for (i = 0; i < a->nelts; i++) {
        elt = &((ngx_table_elt_t *) a->elts)[i];

        elt->key.data = ngx_pnalloc(pool, elt->key.len + 1);
        if (elt->key.data == NULL) {
            goto failed;
        }

        ngx_memcpy(elt->key.data, buf, elt->key.len + 1);
        buf += elt->key.len + 1;

        elt->value.data = ngx_pnalloc(pool, elt->value.len + 1);
        if (elt->value.data == NULL) {
            goto failed;
        }

        ngx_memcpy(elt->value.data, buf, elt->value.len + 1);
        buf += elt->value.len + 1;

#if 0
        dd("key: %.*s, value: %.*s",
           (int) elt[i].key.len, elt[i].key.data,
           (int) elt[i].value.len, elt[i].value.data);
#endif

    }

    return a;

failed:

    for (i = 0; i < a->nelts; i++) {
        elt = ((ngx_table_elt_t **) a->elts)[i];
        if (elt) {
            ngx_pfree(pool, elt);
        }
    }

    ngx_array_destroy(a);

    return NULL;
}


void
ngx_proxy_wasm_filter_tick_handler(ngx_event_t *ev)
{
    ngx_int_t                       rc;
    wasm_val_vec_t                  args;
    ngx_proxy_wasm_filter_ctx_t    *fctx = ev->data;
    ngx_proxy_wasm_instance_ctx_t  *ictx;
    ngx_wavm_instance_t            *instance;

    instance = ngx_proxy_wasm_fctx2instance(fctx);
    ictx = (ngx_proxy_wasm_instance_ctx_t *) instance->data;

    ngx_free(ev);

    ngx_wasm_assert(fctx->root_id == NGX_PROXY_WASM_ROOT_CTX_ID);

    if (ngx_exiting) {
        return;
    }

    if (fctx->filter->proxy_on_timer_ready) {
        ngx_wavm_instance_set_data(instance, ictx, ev->log);

        wasm_val_vec_new_uninitialized(&args, 1);
        ngx_wasm_vec_set_i32(&args, 0, fctx->id);

        rc = ngx_wavm_instance_call_funcref_vec(instance,
                                            fctx->filter->proxy_on_timer_ready,
                                            NULL, &args);
        wasm_val_vec_delete(&args);

        if (!ngx_exiting) {
            ev = ngx_calloc(sizeof(ngx_event_t), ev->log);
            if (ev == NULL) {
                goto nomem;
            }

            ev->handler = ngx_proxy_wasm_filter_tick_handler;
            ev->data = fctx;
            ev->log = fctx->log;

            ngx_add_timer(ev, fctx->filter->tick_period);
        }

        if (rc != NGX_OK) {
            return;
        }
    }

    return;

nomem:

    ngx_wasm_log_error(NGX_LOG_CRIT, fctx->log, 0,
                       "tick_handler: no memory");
}


void
ngx_proxy_wasm_log_error(ngx_uint_t level, ngx_log_t *log,
    ngx_proxy_wasm_err_e err, const char *fmt, ...)
{
    va_list     args;
    u_char     *p, *last, buf[NGX_MAX_ERROR_STR];
    ngx_str_t  *errmsg = NULL;

    last = buf + NGX_MAX_ERROR_STR;
    p = &buf[0];

    if (err) {
        errmsg = ngx_proxy_wasm_filter_strerror(err);
    }

    if (fmt) {
        va_start(args, fmt);
        p = ngx_vslprintf(p, last, fmt, args);
        va_end(args);

        if (err) {
            p = ngx_slprintf(p, last, " (%V)", errmsg);
        }

        ngx_log_error(level, log, 0, "%*s", p - buf, buf);
        return;

    } else if (err) {
        ngx_log_error(level, log, 0, "%V", errmsg);
        return;
    }

    ngx_wasm_assert(0);
}
