#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_event.h>
#include <ngx_wavm.h>
#include <ngx_proxy_wasm.h>
#ifdef NGX_WASM_HTTP
#include <ngx_http_wasm.h>
#include <ngx_wasm_subsystem.h>
#endif


#define NGX_PROXY_WASM_PTR_SIZE  4


static ngx_str_t  ngx_proxy_wasm_errlist[] = {
    ngx_null_string,
    ngx_string("unknown ABI version"),
    ngx_string("incompatible ABI version"),
    ngx_string("incompatible host interface"),
    ngx_string("incompatible SDK interface"),
    ngx_string("on_context_create internal failure"),
    ngx_string("on_vm_start failure"),
    ngx_string("on_configure failure"),
    ngx_string("dispatch failure"),
    ngx_string("instance trapped"),
    ngx_string("invalid return action"),
    ngx_string("unknown error")
};


static ngx_str_t  ngx_proxy_wasm_steplist[] = {
    ngx_null_string,
    ngx_string("on_request_headers"),
    ngx_string("on_request_body"),
    ngx_string("on_request_trailers"),
    ngx_string("on_response_headers"),
    ngx_string("on_response_body"),
    ngx_string("on_response_tailers"),
    ngx_string("on_log"),
    ngx_string("on_done"),
    ngx_string("on_tick"),
    ngx_string("on_dispatch_response")
};


static ngx_str_t  ngx_proxy_wasm_actionlist[] = {
    ngx_string("CONTINUE"),
    ngx_string("PAUSE"),
    ngx_string("END_STREAM"),
    ngx_string("DONE"),
    ngx_string("WAIT"),
    ngx_string("WAIT_FULL"),
    ngx_string("CLOSE"),
};


ngx_str_t *
ngx_proxy_wasm_step_name(ngx_proxy_wasm_step_e step)
{
    ngx_str_t  *name;

    ngx_wa_assert(step);
    ngx_wa_assert(step <= NGX_PROXY_WASM_STEP_DISPATCH_RESPONSE);

    name = &ngx_proxy_wasm_steplist[step];

    ngx_wa_assert(name);

    return name;
}


ngx_str_t *
ngx_proxy_wasm_action_name(ngx_proxy_wasm_action_e action)
{
    ngx_str_t  *name;

    ngx_wa_assert(action >= NGX_PROXY_WASM_ACTION_CONTINUE);
    ngx_wa_assert(action <= NGX_PROXY_WASM_ACTION_CLOSE);

    name = &ngx_proxy_wasm_actionlist[action];

    ngx_wa_assert(name);

    return name;
}


static ngx_inline ngx_str_t *
ngx_proxy_wasm_filter_strerror(ngx_proxy_wasm_err_e err)
{
    ngx_str_t  *msg;

    msg = ((ngx_uint_t) err < NGX_PROXY_WASM_ERR_UNKNOWN)
          ? &ngx_proxy_wasm_errlist[err]
          : &ngx_proxy_wasm_errlist[NGX_PROXY_WASM_ERR_UNKNOWN];

    return msg;
}


u_char *
ngx_proxy_wasm_log_error_handler(ngx_log_t *log, u_char *buf,
    size_t len)
{
    return buf;
}


void
ngx_proxy_wasm_log_error(ngx_uint_t level, ngx_log_t *log,
    ngx_proxy_wasm_err_e err, const char *fmt, ...)
{
    va_list                    args;
    ngx_str_t                 *pre = NULL;
    u_char                    *p, *last, buf[NGX_MAX_ERROR_STR];
    ngx_str_t                 *errmsg = NULL;
    ngx_proxy_wasm_exec_t     *pwexec;
    ngx_proxy_wasm_filter_t   *filter;
    ngx_proxy_wasm_log_ctx_t  *log_ctx;

    if (log->handler != ngx_proxy_wasm_log_error_handler) {
        goto skip_prefix;
    }

    log_ctx = (ngx_proxy_wasm_log_ctx_t *) log->data;
    pwexec = log_ctx->pwexec;
    filter = pwexec->filter;
    pre = &log_ctx->log_prefix;
    log = log_ctx->orig_log;

    if (pre->data == NULL) {
        p = &buf[0];

        pre->len = sizeof("[\"\" #") - 1;
        pre->len += filter->name->len;

        if (pwexec->root_id == NGX_PROXY_WASM_ROOT_CTX_ID) {
            pre->len += ngx_snprintf(p, NGX_SIZE_T_LEN, "%l", pwexec->root_id)
                        - buf;

        } else {
            pre->len += ngx_snprintf(p, NGX_SIZE_T_LEN, "%l", pwexec->id)
                        - buf;
        }

        pre->len += sizeof("] ") - 1;
        pre->data = ngx_pnalloc(pwexec->pool, pre->len);
        if (pre->data == NULL) {
            goto skip_prefix;
        }

        p = pre->data;
        p = ngx_snprintf(p, pre->len, "[\"%V\" #%l] ",
                         filter->name,
                         pwexec->root_id == NGX_PROXY_WASM_ROOT_CTX_ID
                         ? pwexec->root_id
                         : pwexec->id);

        ngx_wa_assert(pre->len == (size_t) (p - pre->data));
    }

skip_prefix:

    last = buf + NGX_MAX_ERROR_STR;
    p = &buf[0];

    if (pre) {
        p = ngx_slprintf(p, last, "%*s", pre->len, pre->data);
    }

    va_start(args, fmt);
    p = ngx_vslprintf(p, last, fmt, args);
    va_end(args);

    if (err) {
        errmsg = ngx_proxy_wasm_filter_strerror(err);
        p = ngx_slprintf(p, last, " (%V)", errmsg);
    }

    if (pre) {
        ngx_log_error_core(level, log, 0, "[proxy-wasm]%*s", p - buf, buf);

    } else {
        ngx_log_error_core(level, log, 0, "[proxy-wasm] %*s", p - buf, buf);
    }
}


void
ngx_proxy_wasm_filter_tick_handler(ngx_event_t *ev)
{
    ngx_log_t                *log = ev->log;
    ngx_proxy_wasm_exec_t    *rexec = ev->data;
    ngx_proxy_wasm_filter_t  *filter = rexec->filter;
#ifdef NGX_WASM_HTTP
    ngx_proxy_wasm_ctx_t     *pwctx = rexec->parent;
#endif

    dd("enter");

    ngx_wa_assert(rexec->root_id == NGX_PROXY_WASM_ROOT_CTX_ID);

    ngx_free(ev);

    rexec->ev = NULL;

    if (ngx_exiting || !filter->proxy_on_timer_ready) {
        return;
    }

#ifdef NGX_WASM_HTTP
    pwctx->phase = ngx_wasm_phase_lookup(&ngx_http_wasm_subsystem,
                                         NGX_WASM_BACKGROUND_PHASE);
#endif

    (void) ngx_proxy_wasm_run_step(rexec, NGX_PROXY_WASM_STEP_TICK);

    if (!ngx_exiting) {
        rexec->ev = ngx_calloc(sizeof(ngx_event_t), log);
        if (rexec->ev == NULL) {
            goto nomem;
        }

        rexec->ev->handler = ngx_proxy_wasm_filter_tick_handler;
        rexec->ev->data = rexec;
        rexec->ev->log = log;

        dd("scheduling next tick in %ld", rexec->tick_period);

        ngx_add_timer(rexec->ev, rexec->tick_period);
    }

    return;

nomem:

    ngx_wasm_log_error(NGX_LOG_CRIT, log, 0,
                       "tick_handler: no memory");
}


static ngx_uint_t
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


static size_t
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


static void
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

    ngx_wa_assert(n == count);

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

    ngx_wa_assert(n == count);
}


ngx_int_t
ngx_proxy_wasm_pairs_unmarshal(ngx_proxy_wasm_exec_t *pwexec,
    ngx_array_t *dst, ngx_proxy_wasm_marshalled_map_t *map)
{
    size_t            i;
    uint32_t          count = 0;
    u_char           *buf;
    ngx_table_elt_t  *elt;

    buf = map->data;

    if (map->len) {
        count = *((uint32_t *) buf);
        buf += NGX_PROXY_WASM_PTR_SIZE;
    }

    if (ngx_array_init(dst, pwexec->pool, count, sizeof(ngx_table_elt_t))
        != NGX_OK)
    {
        return NGX_ERROR;
    }

    for (i = 0; i < count; i++) {
        elt = ngx_array_push(dst);
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

    for (i = 0; i < dst->nelts; i++) {
        elt = &((ngx_table_elt_t *) dst->elts)[i];

        elt->key.data = ngx_pnalloc(pwexec->pool, elt->key.len + 1);
        if (elt->key.data == NULL) {
            goto failed;
        }

        ngx_memcpy(elt->key.data, buf, elt->key.len + 1);
        buf += elt->key.len + 1;

        elt->value.data = ngx_pnalloc(pwexec->pool, elt->value.len + 1);
        if (elt->value.data == NULL) {
            goto failed;
        }

        ngx_memcpy(elt->value.data, buf, elt->value.len + 1);
        buf += elt->value.len + 1;

#if 0
        dd("key: %.*s, value: %.*s",
           (int) elt->key.len, elt->key.data,
           (int) elt->value.len, elt->value.data);
#endif
    }

    return NGX_OK;

failed:

    ngx_array_destroy(dst);

    return NGX_ERROR;
}


unsigned
ngx_proxy_wasm_marshal(ngx_proxy_wasm_exec_t *pwexec, ngx_list_t *list,
    ngx_array_t *shims, ngx_wavm_ptr_t *out, uint32_t *out_size,
    ngx_uint_t *truncated)
{
    size_t                size;
    ngx_wavm_ptr_t        p;
    ngx_wavm_instance_t  *instance = ngx_proxy_wasm_pwexec2instance(pwexec);

    size = ngx_proxy_wasm_pairs_size(list, shims, pwexec->filter->max_pairs);

    p = ngx_proxy_wasm_alloc(pwexec, size);
    if (!p) {
        return 0;
    }

    if (p + size > ngx_wavm_memory_data_size(instance->memory)) {
        return 0;
    }

    ngx_proxy_wasm_pairs_marshal(list, shims,
                                 NGX_WAVM_HOST_LIFT_SLICE(instance, p, size),
                                 pwexec->filter->max_pairs,
                                 truncated);

    *out = p;
    *out_size = (uint32_t) size;

    return 1;
}
