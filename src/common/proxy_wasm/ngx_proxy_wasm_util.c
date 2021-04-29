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
    ngx_string("incompatible sdk interface"),
    ngx_string("unknown error")
};


static u_char *
ngx_proxy_wasm_strerror(ngx_proxy_wasm_err_t err, u_char *buf, size_t size)
{
    ngx_str_t  *msg;

    msg = ((ngx_uint_t) err < NGX_PROXY_WASM_ERR_UNKNOWN)
              ? &ngx_proxy_wasm_errlist[err]
              : &ngx_proxy_wasm_errlist[NGX_PROXY_WASM_ERR_UNKNOWN];

    size = ngx_min(size, msg->len);

    return ngx_cpymem(buf, msg->data, size);
}


/* utils */


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
ngx_proxy_wasm_pairs_size(ngx_list_t *list, ngx_uint_t max)
{
    size_t            i, n, size;
    ngx_table_elt_t  *elt;
    ngx_list_part_t  *part;

    part = &list->part;
    elt = part->elts;

    size = NGX_PROXY_WASM_PTR_SIZE; /* headers count */

    for (i = 0, n = 0; /* void */; i++, n++) {

        if (max && n >= max) {
            break;
        }

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
ngx_proxy_wasm_pairs_marshal(ngx_list_t *list, u_char *buf, ngx_uint_t max,
    ngx_uint_t *truncated)
{
    size_t            i, n;
    uint32_t          count;
    ngx_table_elt_t  *elt;
    ngx_list_part_t  *part;

    part = &list->part;
    elt = part->elts;

    count = ngx_proxy_wasm_pairs_count(list);

    if (max && count > max) {
        count = max;

        if (truncated) {
            *truncated = count;
        }
    }

    *((uint32_t *) buf) = count;
    buf += NGX_PROXY_WASM_PTR_SIZE;

    for (i = 0, n = 0; /* void */; i++, n++) {

        if (max && n >= max) {
            break;
        }

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

    part = &list->part;
    elt = part->elts;

    for (i = 0, n = 0; /* void */; i++, n++) {

        if (max && n >= max) {
            break;
        }

        if (i >= part->nelts) {
            if (part->next == NULL) {
                break;
            }

            part = part->next;
            elt = part->elts;
            i = 0;
        }

        buf = ngx_cpymem(buf, elt[i].key.data, elt[i].key.len);
        *buf++ = '\0';
        buf = ngx_cpymem(buf, elt[i].value.data, elt[i].value.len);
        *buf++ = '\0';
    }
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


ngx_str_t *
ngx_proxy_wasm_get_map_value(ngx_list_t *map, u_char *key, size_t key_len)
{
    size_t            i;
    ngx_table_elt_t  *elt;
    ngx_list_part_t  *part;

    part = &map->part;
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

#if 0
        dd("key: %.*s, value: %.*s",
           (int) elt[i].key.len, elt[i].key.data,
           (int) elt[i].value.len, elt[i].value.data);
#endif

        if (key_len == elt[i].key.len
            && ngx_strncasecmp(elt[i].key.data, key, key_len) == 0)
        {
            return &elt[i].value;
        }
    }

    return NULL;
}


#if 0
ngx_int_t
ngx_proxy_wasm_add_map_value(ngx_pool_t *pool, ngx_list_t *map, u_char *key,
    size_t key_len, u_char *value, size_t val_len)
{
    ngx_table_elt_t  *h;

    h = ngx_list_push(map);
    if (h == NULL) {
        return NGX_ERROR;
    }

    h->hash = ngx_hash_key(key, key_len);
    h->key.len = key_len;
    h->key.data = ngx_pnalloc(pool, h->key.len);
    if (h->key.data == NULL) {
        return NGX_ERROR;
    }

    ngx_memcpy(h->key.data, key, key_len);

    h->value.len = val_len;
    h->value.data = ngx_pnalloc(pool, h->value.len);
    if (h->value.data == NULL) {
        return NGX_ERROR;
    }

    ngx_memcpy(h->value.data, value, val_len);

    h->lowcase_key = ngx_pnalloc(pool, h->key.len);
    if (h->lowcase_key == NULL) {
        return NGX_ERROR;
    }

    ngx_strlow(h->lowcase_key, h->key.data, h->key.len);

    return NGX_OK;
}
#endif


void
ngx_proxy_wasm_tick_handler(ngx_event_t *ev)
{
    ngx_int_t          rc;
    ngx_proxy_wasm_t  *pwm = ev->data;
    wasm_val_vec_t     args;

    ngx_free(ev);

    if (ngx_exiting) {
        return;
    }

    if (pwm->proxy_on_timer_ready) {
        ngx_wavm_ctx_update(pwm->instance->ctx, NULL, NULL);

        wasm_val_vec_new_uninitialized(&args, 1);
        ngx_wasm_vec_set_i32(&args, 0, pwm->rctxid);

        rc = ngx_wavm_instance_call_funcref_vec(pwm->instance,
                                                pwm->proxy_on_timer_ready,
                                                NULL, &args);
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

    ngx_wasm_log_error(NGX_LOG_CRIT, pwm->instance->log, 0,
                       "tick_handler: no memory");
}


void
ngx_proxy_wasm_log_error(ngx_uint_t level, ngx_log_t *log,
    ngx_proxy_wasm_err_t err, const char *fmt, ...)
{
    va_list   args;
    u_char   *p, *last, buf[NGX_MAX_ERROR_STR];

    last = buf + NGX_MAX_ERROR_STR;
    p = &buf[0];

    if (err) {
        p = ngx_proxy_wasm_strerror(err, p, last - p);
    }

    if (fmt) {
        if (err && p + 2 <= last) {
            *p++ = ':';
            *p++ = ' ';
        }

        va_start(args, fmt);
        p = ngx_vslprintf(p, last, fmt, args);
        va_end(args);
    }

    ngx_wasm_log_error(level, log, 0, "%*s", p - buf, buf);
}
