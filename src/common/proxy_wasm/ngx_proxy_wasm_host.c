#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_wavm.h>
#include <ngx_event.h>
#include <ngx_proxy_wasm.h>
#include <ngx_proxy_wasm_maps.h>
#include <ngx_proxy_wasm_properties.h>
#include <ngx_wasm_shm_kv.h>
#include <ngx_wasm_shm_queue.h>
#ifdef NGX_WASM_HTTP
#include <ngx_http_proxy_wasm.h>
#endif


static ngx_chain_t *
ngx_proxy_wasm_get_buffer_helper(ngx_wavm_instance_t *instance,
    ngx_proxy_wasm_buffer_type_e buf_type, unsigned *none)
{
#ifdef NGX_WASM_HTTP
    ngx_chain_t              *cl;
    ngx_http_wasm_req_ctx_t  *rctx;
    ngx_http_request_t       *r;

    rctx = ngx_http_proxy_wasm_get_rctx(instance);
    r = rctx->r;
#endif

    switch (buf_type) {

#ifdef NGX_WASM_HTTP
    case NGX_PROXY_WASM_BUFFER_HTTP_REQUEST_BODY:
        if (r->request_body == NULL
            || r->request_body->bufs == NULL)
        {
            /* no body */
            *none = 1;
            return NULL;
        }

        if (r->request_body->temp_file) {
            ngx_wavm_log_error(NGX_LOG_ERR, instance->log, NULL,
                               "cannot read request body buffered "
                               "to file at \"%V\"",
                               &r->request_body->temp_file->file.name);
            *none = 1;
            return NULL;
        }

        return r->request_body->bufs;

    case NGX_PROXY_WASM_BUFFER_HTTP_RESPONSE_BODY:
        cl = rctx->resp_chunk;
        if (cl == NULL) {
            /* no body */
            *none = 1;
            return NULL;
        }

        return cl;

    case NGX_PROXY_WASM_BUFFER_HTTP_CALL_RESPONSE_BODY:
        {
            ngx_wasm_http_reader_ctx_t      *reader;
            ngx_http_proxy_wasm_dispatch_t  *call;
            ngx_proxy_wasm_exec_t           *pwexec;

            pwexec = ngx_proxy_wasm_instance2pwexec(instance);
            call = pwexec->call;
            if (call == NULL) {
                return NULL;
            }

            reader = &call->http_reader;

            if (!reader->body_len) {
                /* no body */
                *none = 1;
                return NULL;
            }

            ngx_wasm_assert(reader->body);

            return reader->body;
        }
#endif

    default:
        ngx_wavm_log_error(NGX_LOG_WASM_NYI, instance->log, NULL,
                           "NYI - get_buffer bad buf_type: %d", buf_type);
        return NULL;

    }
}


/* hfuncs */


static ngx_int_t
ngx_proxy_wasm_hfuncs_proxy_log(ngx_wavm_instance_t *instance,
    wasm_val_t args[], wasm_val_t rets[])
{
    uint32_t     log_level, msg_size;
    u_char      *msg_data;
    ngx_uint_t   level;

    log_level = args[0].of.i32;
    msg_size = args[2].of.i32;
    msg_data = NGX_WAVM_HOST_LIFT_SLICE(instance, args[1].of.i32, msg_size);

    switch (log_level) {

    case NGX_PROXY_WASM_LOG_TRACE:
    case NGX_PROXY_WASM_LOG_DEBUG:
        level = NGX_LOG_DEBUG;
        break;

    case NGX_PROXY_WASM_LOG_INFO:
        level = NGX_LOG_INFO;
        break;

    case NGX_PROXY_WASM_LOG_WARNING:
        level = NGX_LOG_WARN;
        break;

    case NGX_PROXY_WASM_LOG_ERROR:
        level = NGX_LOG_ERR;
        break;

    case NGX_PROXY_WASM_LOG_CRITICAL:
        level = NGX_LOG_CRIT;
        break;

    default:
        ngx_wavm_log_error(NGX_LOG_WASM_NYI, instance->log, NULL,
                           "NYI - proxy_log bad log_level: %d", log_level);

        return ngx_proxy_wasm_result_badarg(rets);

    }

    ngx_log_error(level, instance->log_ctx.orig_log, 0, "%*s",
                  msg_size, msg_data);

    return ngx_proxy_wasm_result_ok(rets);
}


static ngx_int_t
ngx_proxy_wasm_hfuncs_get_buffer(ngx_wavm_instance_t *instance,
    wasm_val_t args[], wasm_val_t rets[])
{
    size_t                         offset, max_len, len, chunk_len;
    unsigned                       none = 0;
    u_char                        *start = NULL;
    ngx_chain_t                   *cl = NULL;
    ngx_buf_t                     *buf;
    uint32_t                      *rlen;
    ngx_wavm_ptr_t                *rbuf, p;
    ngx_proxy_wasm_buffer_type_e   buf_type;
    ngx_proxy_wasm_exec_t         *pwexec;

    pwexec = ngx_proxy_wasm_instance2pwexec(instance);

    buf_type = args[0].of.i32;
    offset = args[1].of.i32;
    max_len = args[2].of.i32;
    rbuf = NGX_WAVM_HOST_LIFT(instance, args[3].of.i32, ngx_wavm_ptr_t);
    rlen = NGX_WAVM_HOST_LIFT(instance, args[4].of.i32, uint32_t);

    switch (buf_type) {
    case NGX_PROXY_WASM_BUFFER_PLUGIN_CONFIGURATION:
        start = pwexec->filter->config.data;
        len = pwexec->filter->config.len;
        break;

    case NGX_PROXY_WASM_BUFFER_VM_CONFIGURATION:
        start = pwexec->filter->module->config.data;
        len = pwexec->filter->module->config.len;
        break;

    default:
        cl = ngx_proxy_wasm_get_buffer_helper(instance, buf_type, &none);
        if (cl == NULL) {
            if (none) {
                return ngx_proxy_wasm_result_notfound(rets);
            }

            return ngx_proxy_wasm_result_badarg(rets);
        }

        len = ngx_wasm_chain_len(cl, NULL);
        break;
    }

    len = ngx_min(len, max_len);

    if (offset > offset + len) {
        /* overflow */
        return ngx_proxy_wasm_result_err(rets);
    }

    if (!len) {
        /* eof */
        return ngx_proxy_wasm_result_notfound(rets);
    }

    p = ngx_proxy_wasm_alloc(pwexec, len);
    if (p == 0) {
        return ngx_proxy_wasm_result_err(rets);
    }

    *rbuf = p;
    *rlen = (uint32_t) len;

    if (start == NULL) {
        ngx_wasm_assert(cl);

        if (cl->next == NULL) {
            /* single buffer */
            start = cl->buf->pos;
        }
    }

    if (start) {
        if (!ngx_wavm_memory_memcpy(instance->memory, p, start, len)) {
            return ngx_proxy_wasm_result_invalid_mem(rets);
        }

    } else {
        ngx_wasm_assert(cl);
        ngx_wasm_assert(cl->buf);
        ngx_wasm_assert(cl->next);
        len = 0;

        for (/* void */; cl; cl = cl->next) {
            buf = cl->buf;
            chunk_len = buf->last - buf->pos;
            len += chunk_len;

            if (len > max_len) {
                chunk_len -= len - max_len;
                len = 0; /* break after copy */
            }

            if (chunk_len) {
                p = ngx_wavm_memory_cpymem(instance->memory, p, buf->pos,
                                           chunk_len);
                if (p == 0) {
                    return ngx_proxy_wasm_result_invalid_mem(rets);
                }
            }

            if (buf->last_buf || buf->last_in_chain) {
                break;
            }
        }
    }

    return ngx_proxy_wasm_result_ok(rets);
}


static ngx_int_t
ngx_proxy_wasm_hfuncs_set_buffer(ngx_wavm_instance_t *instance,
    wasm_val_t args[], wasm_val_t rets[])
{
    ngx_int_t                      rc = NGX_ERROR;
    ngx_proxy_wasm_buffer_type_e   buf_type;
#ifdef NGX_WASM_HTTP
    size_t                         buf_len;
    size_t                         offset, max;
    ngx_str_t                      s;
    ngx_wavm_ptr_t                *buf_data;
    ngx_http_wasm_req_ctx_t       *rctx;
    ngx_proxy_wasm_exec_t         *pwexec;

    rctx = ngx_http_proxy_wasm_get_rctx(instance);
    pwexec = ngx_proxy_wasm_instance2pwexec(instance);

    offset = args[1].of.i32;
    max = args[2].of.i32;

    buf_len = args[4].of.i32;
    buf_data = NGX_WAVM_HOST_LIFT_SLICE(instance, args[3].of.i32, buf_len);

    s.len = buf_len;
    s.data = (u_char *) buf_data;

    if (offset > 0 && max == 0 && s.len > 0) {
        max = s.len;
    }
#endif

    buf_type = args[0].of.i32;

    switch (buf_type) {

#ifdef NGX_WASM_HTTP
    case NGX_PROXY_WASM_BUFFER_HTTP_REQUEST_BODY:
        if (offset == 0 && max == 0 && buf_len > 0) {
            rc = ngx_http_wasm_prepend_req_body(rctx, &s);

        } else {
            rc = ngx_http_wasm_set_req_body(rctx, &s, offset, max);
        }

        if (rc == NGX_ABORT) {
            return ngx_proxy_wasm_result_trap(pwexec,
                                              "cannot set request body",
                                              rets, NGX_WAVM_BAD_USAGE);
        }

        break;

    case NGX_PROXY_WASM_BUFFER_HTTP_RESPONSE_BODY:
        if (offset == 0 && max == 0 && buf_len > 0) {
            rc = ngx_http_wasm_prepend_resp_body(rctx, &s);

        } else {
            rc = ngx_http_wasm_set_resp_body(rctx, &s, offset, max);
        }

        if (rc == NGX_ABORT) {
            return ngx_proxy_wasm_result_trap(pwexec,
                                              "cannot set response body",
                                              rets, NGX_WAVM_BAD_USAGE);
        }

        break;
#endif

    default:
        ngx_wavm_log_error(NGX_LOG_WASM_NYI, instance->log, NULL,
                           "NYI - set_buffer bad buf_type: %d", buf_type);
        return ngx_proxy_wasm_result_badarg(rets);

    }

    if (rc != NGX_OK) {
        return ngx_proxy_wasm_result_err(rets);
    }

    return ngx_proxy_wasm_result_ok(rets);
}


static ngx_int_t
ngx_proxy_wasm_hfuncs_get_header_map_pairs(ngx_wavm_instance_t *instance,
    wasm_val_t args[], wasm_val_t rets[])
{
    uint32_t                   *rlen;
    ngx_uint_t                  truncated = 0;
    ngx_list_t                 *list;
    ngx_array_t                 extras;
    ngx_wavm_ptr_t             *rbuf;
    ngx_proxy_wasm_exec_t      *pwexec;
    ngx_proxy_wasm_map_type_e   map_type;

    pwexec = ngx_proxy_wasm_instance2pwexec(instance);

    map_type = args[0].of.i32;
    rbuf = NGX_WAVM_HOST_LIFT(instance, args[1].of.i32, ngx_wavm_ptr_t);
    rlen = NGX_WAVM_HOST_LIFT(instance, args[2].of.i32, uint32_t);

    ngx_array_init(&extras, pwexec->pool, 8, sizeof(ngx_table_elt_t));

    list = ngx_proxy_wasm_maps_get_all(instance, map_type, &extras);
    if (list == NULL) {
        return ngx_proxy_wasm_result_badarg(rets);
    }

    if (!ngx_proxy_wasm_marshal(pwexec, list, &extras, rbuf, rlen,
                                &truncated))
    {
        return ngx_proxy_wasm_result_invalid_mem(rets);
    }

    if (truncated) {
        ngx_proxy_wasm_log_error(NGX_LOG_WARN, instance->log, 0,
                                 "marshalled map truncated to %ui elements",
                                 truncated);
    }

    return ngx_proxy_wasm_result_ok(rets);
}


static ngx_int_t
ngx_proxy_wasm_hfuncs_get_header_map_value(ngx_wavm_instance_t *instance,
    wasm_val_t args[], wasm_val_t rets[])
{
    uint32_t                   *rlen;
    ngx_str_t                  *value, key;
    ngx_wavm_ptr_t             *rbuf, p;
    ngx_proxy_wasm_exec_t      *pwexec;
    ngx_proxy_wasm_map_type_e   map_type;

    pwexec = ngx_proxy_wasm_instance2pwexec(instance);

    map_type = args[0].of.i32;
    key.len = args[2].of.i32;
    key.data = NGX_WAVM_HOST_LIFT_SLICE(instance, args[1].of.i32, key.len);
    rbuf = NGX_WAVM_HOST_LIFT(instance, args[3].of.i32, ngx_wavm_ptr_t);
    rlen = NGX_WAVM_HOST_LIFT(instance, args[4].of.i32, uint32_t);

    value = ngx_proxy_wasm_maps_get(instance, map_type, &key);
    if (value == NULL) {
        if (pwexec->filter->abi_version == NGX_PROXY_WASM_0_1_0) {
            rbuf = NULL;
            *rlen = 0;
            return ngx_proxy_wasm_result_ok(rets);
        }

        return ngx_proxy_wasm_result_notfound(rets);
    }

    p = ngx_proxy_wasm_alloc(pwexec, value->len);
    if (p == 0) {
        return ngx_proxy_wasm_result_err(rets);
    }

    if (!ngx_wavm_memory_memcpy(instance->memory, p, value->data, value->len)) {
        return ngx_proxy_wasm_result_invalid_mem(rets);
    }

    *rbuf = p;
    *rlen = (uint32_t) value->len;

    return ngx_proxy_wasm_result_ok(rets);
}


static ngx_int_t
ngx_proxy_wasm_hfuncs_set_header_map_pairs(ngx_wavm_instance_t *instance,
    wasm_val_t args[], wasm_val_t rets[])
{
    ngx_int_t                         rc = NGX_ERROR;
    ngx_proxy_wasm_map_type_e         map_type;
    ngx_array_t                       headers;
    ngx_proxy_wasm_marshalled_map_t   map;
    ngx_proxy_wasm_exec_t            *pwexec;
#ifdef NGX_WASM_HTTP
    ngx_http_wasm_req_ctx_t          *rctx;

    rctx =  ngx_http_proxy_wasm_get_rctx(instance);
#endif

    pwexec = ngx_proxy_wasm_instance2pwexec(instance);

    map_type = args[0].of.i32;
    map.len = args[2].of.i32;
    map.data = NGX_WAVM_HOST_LIFT_SLICE(instance, args[1].of.i32, map.len);

    if (ngx_proxy_wasm_pairs_unmarshal(pwexec, &headers, &map) != NGX_OK) {
        return ngx_proxy_wasm_result_err(rets);
    }

    switch (map_type) {

#ifdef NGX_WASM_HTTP
    case NGX_PROXY_WASM_MAP_HTTP_REQUEST_HEADERS:
        if (rctx->entered_header_filter) {
            ngx_wavm_log_error(NGX_LOG_ERR, instance->log, NULL,
                               "cannot set request headers: response produced");

            return ngx_proxy_wasm_result_ok(rets);
        }

        rc = ngx_http_wasm_clear_req_headers(
                 ngx_http_proxy_wasm_get_req(instance));
        break;

    case NGX_PROXY_WASM_MAP_HTTP_RESPONSE_HEADERS:
        if (rctx->r->header_sent) {
            ngx_wavm_log_error(NGX_LOG_ERR, instance->log, NULL,
                               "cannot set response headers: "
                               "headers already sent");

            return ngx_proxy_wasm_result_ok(rets);
        }

        rc = ngx_http_wasm_clear_resp_headers(
                 ngx_http_proxy_wasm_get_req(instance));
        break;
#endif

    default:
        ngx_wasm_log_error(NGX_LOG_WASM_NYI, instance->log, 0,
                           "NYI - set_map bad map_type: %d", map_type);
        return ngx_proxy_wasm_result_badarg(rets);
    }

    if (rc != NGX_OK) {
        return ngx_proxy_wasm_result_err(rets);
    }

    rc = ngx_proxy_wasm_maps_set_all(instance, map_type, &headers);
    if (rc == NGX_ERROR) {
        return ngx_proxy_wasm_result_err(rets);
    }

    /*
     * NGX_ABORT: read-only
     */

    ngx_wasm_assert(rc == NGX_OK || rc == NGX_ABORT);

    return ngx_proxy_wasm_result_ok(rets);
}


static ngx_int_t
ngx_proxy_wasm_hfuncs_add_header_map_value(ngx_wavm_instance_t *instance,
    wasm_val_t args[], wasm_val_t rets[])
{
    ngx_int_t                   rc;
    ngx_str_t                   key, value;
    ngx_proxy_wasm_map_type_e   map_type;
#ifdef NGX_WASM_HTTP
    ngx_http_wasm_req_ctx_t    *rctx;

    rctx = ngx_http_proxy_wasm_get_rctx(instance);
#endif

    map_type = args[0].of.i32;
    key.len = args[2].of.i32;
    key.data = NGX_WAVM_HOST_LIFT_SLICE(instance, args[1].of.i32, key.len);
    value.len = args[4].of.i32;
    value.data = NGX_WAVM_HOST_LIFT_SLICE(instance, args[3].of.i32, value.len);

#ifdef NGX_WASM_HTTP
    if (map_type == NGX_PROXY_WASM_MAP_HTTP_REQUEST_HEADERS
        && rctx->entered_header_filter)
    {
        ngx_wavm_log_error(NGX_LOG_ERR, instance->log, NULL,
                           "cannot add request header: response produced");

        return ngx_proxy_wasm_result_ok(rets);

    } else if (map_type == NGX_PROXY_WASM_MAP_HTTP_RESPONSE_HEADERS
               && rctx->r->header_sent)
    {
        ngx_wavm_log_error(NGX_LOG_ERR, instance->log, NULL,
                           "cannot add response header: "
                           "headers already sent");

        return ngx_proxy_wasm_result_ok(rets);
    }
#endif

    dd("adding '%.*s: %.*s' to map of type '%d'",
       (int) key.len, key.data, (int) value.len, value.data, map_type);

    rc = ngx_proxy_wasm_maps_set(instance, map_type, &key, &value,
                                 NGX_PROXY_WASM_MAP_ADD);
    if (rc == NGX_ERROR) {
        return ngx_proxy_wasm_result_err(rets);
    }

    ngx_wasm_assert(rc == NGX_OK || rc == NGX_ABORT);

    return ngx_proxy_wasm_result_ok(rets);
}


static ngx_int_t
ngx_proxy_wasm_hfuncs_replace_header_map_value(ngx_wavm_instance_t *instance,
    wasm_val_t args[], wasm_val_t rets[])
{
    ngx_int_t                   rc;
    ngx_str_t                   key, value;
    ngx_proxy_wasm_map_type_e   map_type;
#ifdef NGX_WASM_HTTP
    ngx_http_wasm_req_ctx_t    *rctx;

    rctx = ngx_http_proxy_wasm_get_rctx(instance);
#endif

    map_type = args[0].of.i32;
    key.len = args[2].of.i32;
    key.data = NGX_WAVM_HOST_LIFT_SLICE(instance, args[1].of.i32, key.len);
    value.len = args[4].of.i32;
    value.data = NGX_WAVM_HOST_LIFT_SLICE(instance, args[3].of.i32, value.len);

#ifdef NGX_WASM_HTTP
    if (map_type == NGX_PROXY_WASM_MAP_HTTP_REQUEST_HEADERS
        && rctx->entered_header_filter)
    {
        ngx_wavm_log_error(NGX_LOG_ERR, instance->log, NULL,
                           "cannot set request header: response produced");

        return ngx_proxy_wasm_result_ok(rets);

    } else if (map_type == NGX_PROXY_WASM_MAP_HTTP_RESPONSE_HEADERS
               && rctx->r->header_sent)
    {
        ngx_wavm_log_error(NGX_LOG_ERR, instance->log, NULL,
                           "cannot set response header: "
                           "headers already sent");

        return ngx_proxy_wasm_result_ok(rets);
    }
#endif

    dd("setting '%.*s: %.*s' into map of type '%d'",
       (int) key.len, key.data, (int) value.len, value.data, map_type);

    rc = ngx_proxy_wasm_maps_set(instance, map_type, &key, &value,
                                 NGX_PROXY_WASM_MAP_SET);
    if (rc == NGX_ERROR) {
        return ngx_proxy_wasm_result_err(rets);
    }

    ngx_wasm_assert(rc == NGX_OK || rc == NGX_ABORT);

    return ngx_proxy_wasm_result_ok(rets);
}


static ngx_int_t
ngx_proxy_wasm_hfuncs_remove_header_map_value(ngx_wavm_instance_t *instance,
    wasm_val_t args[], wasm_val_t rets[])
{
    size_t                      klen;
    ngx_int_t                   rc;
    ngx_wavm_ptr_t             *key;
    ngx_proxy_wasm_map_type_e   map_type;
    ngx_str_t                   skey;

    map_type = args[0].of.i32;
    klen = args[2].of.i32;
    key = NGX_WAVM_HOST_LIFT_SLICE(instance, args[1].of.i32, klen);

    dd("removing '%.*s' from map of type '%d'",
       (int) klen, (u_char *) key, map_type);

    skey.data = (u_char *) key;
    skey.len = klen;

    rc = ngx_proxy_wasm_maps_set(instance, map_type, &skey, NULL,
                                 NGX_PROXY_WASM_MAP_REMOVE);
    if (rc == NGX_ERROR) {
        return ngx_proxy_wasm_result_err(rets);
    }

    ngx_wasm_assert(rc == NGX_OK || rc == NGX_ABORT);

    return ngx_proxy_wasm_result_ok(rets);
}


static ngx_int_t
ngx_proxy_wasm_hfuncs_set_tick_period(ngx_wavm_instance_t *instance,
    wasm_val_t args[], wasm_val_t rets[])
{
    uint32_t                period = args[0].of.i32;
    ngx_event_t            *ev;
    ngx_proxy_wasm_exec_t  *pwexec = ngx_proxy_wasm_instance2pwexec(instance);

    ngx_wasm_assert(pwexec->root_id == NGX_PROXY_WASM_ROOT_CTX_ID);

    if (pwexec->root_id != NGX_PROXY_WASM_ROOT_CTX_ID) {
        /* ignore */
        return ngx_proxy_wasm_result_ok(rets);
    }

    if (ngx_exiting) {
        return ngx_proxy_wasm_result_trap(pwexec, "process exiting", rets,
                                          NGX_WAVM_OK);
    }

    if (pwexec->tick_period) {
        return ngx_proxy_wasm_result_trap(pwexec, "tick_period already set",
                                          rets, NGX_WAVM_OK);
    }

    pwexec->tick_period = period;

    ev = ngx_calloc(sizeof(ngx_event_t), instance->log);
    if (ev == NULL) {
        goto nomem;
    }

    ev->handler = ngx_proxy_wasm_filter_tick_handler;
    ev->data = pwexec;
    ev->log = pwexec->log;

    ngx_add_timer(ev, pwexec->tick_period);

    return ngx_proxy_wasm_result_ok(rets);

nomem:

    return ngx_proxy_wasm_result_trap(pwexec, "no memory",
                                      rets, NGX_WAVM_ERROR);
}


static ngx_int_t
ngx_proxy_wasm_hfuncs_get_current_time(ngx_wavm_instance_t *instance,
    wasm_val_t args[], wasm_val_t rets[])
{
    void  *rtime;

    /* WASM might not align 64-bit integers to 8-byte boundaries. So we
     * need to buffer & copy here. */
    rtime = NGX_WAVM_HOST_LIFT_SLICE(instance, args[0].of.i32,
                                     sizeof(uint64_t));

    ngx_wasm_wall_time(rtime);

    return ngx_proxy_wasm_result_ok(rets);
}


#ifdef NGX_WASM_HTTP
static ngx_int_t
ngx_proxy_wasm_hfuncs_send_local_response(ngx_wavm_instance_t *instance,
    wasm_val_t args[], wasm_val_t rets[])
{
    int32_t                           status, reason_len, body_len;
#if (NGX_DEBUG)
    int32_t                           grpc_status;
#endif
    u_char                           *reason, *body;
    ngx_int_t                         rc;
    ngx_array_t                       headers;
    ngx_proxy_wasm_marshalled_map_t   map;
    ngx_proxy_wasm_exec_t            *pwexec;
    ngx_http_wasm_req_ctx_t          *rctx;

    rctx = ngx_http_proxy_wasm_get_rctx(instance);
    pwexec = ngx_proxy_wasm_instance2pwexec(instance);

    status = args[0].of.i32;
    reason_len = args[2].of.i32;
    reason = NGX_WAVM_HOST_LIFT_SLICE(instance, args[1].of.i32, reason_len);
    body_len = args[4].of.i32;
    body = NGX_WAVM_HOST_LIFT_SLICE(instance, args[3].of.i32, body_len);
    map.len = args[6].of.i32;
    map.data = NGX_WAVM_HOST_LIFT_SLICE(instance, args[5].of.i32, map.len);

#if (NGX_DEBUG)
    grpc_status = args[7].of.i32;

    ngx_wasm_assert(grpc_status == -1);
#endif

    if (ngx_proxy_wasm_pairs_unmarshal(pwexec, &headers, &map) != NGX_OK) {
        return ngx_proxy_wasm_result_err(rets);
    }

    rc = ngx_http_wasm_stash_local_response(rctx, status, reason, reason_len,
                                            &headers, body, body_len);

    dd("stash local response rc: %ld", rc);

    switch (rc) {

    case NGX_OK:
        pwexec->parent->action = NGX_PROXY_WASM_ACTION_DONE;
        break;

    case NGX_ERROR:
        return ngx_proxy_wasm_result_err(rets);

    case NGX_DECLINED:
        return ngx_proxy_wasm_result_badarg(rets);

    case NGX_BUSY:
        return ngx_proxy_wasm_result_trap(pwexec,
                                          "local response already stashed",
                                          rets, NGX_WAVM_BAD_USAGE);

    case NGX_ABORT:
        return ngx_proxy_wasm_result_trap(pwexec,
                                          "response already sent", rets,
                                          NGX_WAVM_BAD_USAGE);

    default:
        /* unreachable */
        ngx_wasm_assert(0);
        return NGX_WAVM_NYI;

    }

    return ngx_proxy_wasm_result_ok(rets);
}


static ngx_int_t
ngx_proxy_wasm_hfuncs_dispatch_http_call(ngx_wavm_instance_t *instance,
    wasm_val_t args[], wasm_val_t rets[])
{
    uint32_t                         *callout_id, timeout;
    ngx_str_t                         host, body;
    ngx_proxy_wasm_marshalled_map_t   headers, trailers;
    ngx_proxy_wasm_exec_t            *pwexec;
    ngx_http_wasm_req_ctx_t          *rctx;
    ngx_http_proxy_wasm_dispatch_t   *call = NULL;

    pwexec = ngx_proxy_wasm_instance2pwexec(instance);
    rctx = ngx_http_proxy_wasm_get_rctx(instance);

    host.len = args[1].of.i32;
    host.data = NGX_WAVM_HOST_LIFT_SLICE(instance, args[0].of.i32, host.len);

    headers.len = args[3].of.i32;
    headers.data = NGX_WAVM_HOST_LIFT_SLICE(instance, args[2].of.i32,
                                            headers.len);

    body.len = args[5].of.i32;
    body.data = NGX_WAVM_HOST_LIFT_SLICE(instance, args[4].of.i32, body.len);

    trailers.len = args[7].of.i32;
    trailers.data = NGX_WAVM_HOST_LIFT_SLICE(instance, args[6].of.i32,
                                             trailers.len);

    timeout = args[8].of.i32;
    callout_id = NGX_WAVM_HOST_LIFT(instance, args[9].of.i32, uint32_t);

    call = ngx_http_proxy_wasm_dispatch(pwexec, rctx, &host,
                                        &headers, &trailers,
                                        &body, timeout);
    if (call == NULL) {
        return ngx_proxy_wasm_result_err(rets);
    }

    *callout_id = call->id;

    return ngx_proxy_wasm_result_ok(rets);
}


#else
static ngx_int_t
ngx_proxy_wasm_hfuncs_dispatch_http_call(ngx_wavm_instance_t *instance,
    wasm_val_t args[], wasm_val_t rets[])
{
    ngx_proxy_wasm_exec_t  *pwexec;

    pwexec = ngx_proxy_wasm_instance2pwexec(instance);

    return ngx_proxy_wasm_result_trap(pwexec,
                                      "NYI - not supported without "
                                      "ngx_http_core_module",
                                      rets, NGX_WAVM_ERROR);
}
#endif


static ngx_int_t
ngx_proxy_wasm_hfuncs_set_effective_context(ngx_wavm_instance_t *instance,
    wasm_val_t args[], wasm_val_t rets[])
{
#if (DDEBUG)
    uint32_t   context_id;

    context_id = args[0].of.i32;

    dd("effective context_id: %d", context_id);
#endif

    return ngx_proxy_wasm_result_ok(rets);
}


static ngx_int_t
ngx_proxy_wasm_hfuncs_get_configuration(ngx_wavm_instance_t *instance,
    wasm_val_t args[], wasm_val_t rets[])
{
    uint32_t               *rlen;
    ngx_wavm_ptr_t         *rbuf, p;
    ngx_proxy_wasm_exec_t  *pwexec;

    pwexec = ngx_proxy_wasm_instance2pwexec(instance);

    rbuf = NGX_WAVM_HOST_LIFT(instance, args[0].of.i32, ngx_wavm_ptr_t);
    rlen = NGX_WAVM_HOST_LIFT(instance, args[1].of.i32, uint32_t);

    if (pwexec->filter->config.len) {
        p = ngx_proxy_wasm_alloc(pwexec, pwexec->filter->config.len);
        if (p == 0) {
            return ngx_proxy_wasm_result_err(rets);
        }

        if (!ngx_wavm_memory_memcpy(instance->memory, p,
                                    pwexec->filter->config.data,
                                    pwexec->filter->config.len))
        {
            return ngx_proxy_wasm_result_invalid_mem(rets);
        }

        *rbuf = p;
        *rlen = (uint32_t) pwexec->filter->config.len;
    }

    return ngx_proxy_wasm_result_ok(rets);
}


static ngx_int_t
ngx_proxy_wasm_hfuncs_get_property(ngx_wavm_instance_t *instance,
    wasm_val_t args[], wasm_val_t rets[])
{
    ngx_int_t               rc;
    ngx_str_t               path, value;
    int32_t                *ret_size;
    ngx_proxy_wasm_exec_t  *pwexec;
    ngx_wavm_ptr_t         *ret_data, p = 0;

    path.len = args[1].of.i32;
    path.data = NGX_WAVM_HOST_LIFT_SLICE(instance, args[0].of.i32, path.len);

    ret_data = NGX_WAVM_HOST_LIFT(instance, args[2].of.i32, ngx_wavm_ptr_t);
    ret_size = NGX_WAVM_HOST_LIFT(instance, args[3].of.i32, int32_t);

    pwexec = ngx_proxy_wasm_instance2pwexec(instance);

    rc = ngx_proxy_wasm_properties_get(pwexec->parent, &path, &value);
    if (rc == NGX_DECLINED) {
        return ngx_proxy_wasm_result_notfound(rets);
    }

    ngx_wasm_assert(rc == NGX_OK);

    p = ngx_proxy_wasm_alloc(pwexec, value.len);
    if (p == 0) {
        return ngx_proxy_wasm_result_err(rets);
    }

    if (!ngx_wavm_memory_memcpy(instance->memory, p,
                                value.data, value.len))
    {
        return ngx_proxy_wasm_result_invalid_mem(rets);
    }

    *ret_data = p;
    *ret_size = value.len;

    return ngx_proxy_wasm_result_ok(rets);
}


static ngx_int_t
ngx_proxy_wasm_hfuncs_set_property(ngx_wavm_instance_t *instance,
    wasm_val_t args[], wasm_val_t rets[])
{
    ngx_proxy_wasm_exec_t  *pwexec;
    ngx_str_t               path, value;
    ngx_int_t               rc;

    path.len = args[1].of.i32;
    path.data = NGX_WAVM_HOST_LIFT_SLICE(instance, args[0].of.i32, path.len);
    value.len = args[3].of.i32;
    value.data = NGX_WAVM_HOST_LIFT_SLICE(instance, args[2].of.i32, value.len);

    pwexec = ngx_proxy_wasm_instance2pwexec(instance);

    rc = ngx_proxy_wasm_properties_set(pwexec->parent, &path, &value);

    switch (rc) {
    case NGX_DECLINED:
        return ngx_proxy_wasm_result_notfound(rets);
    case NGX_ERROR:
        return ngx_proxy_wasm_result_err(rets);
    default:
        ngx_wasm_assert(rc == NGX_OK);
        return ngx_proxy_wasm_result_ok(rets);
    }
}


static ngx_int_t
ngx_proxy_wasm_hfuncs_resume_http_request(ngx_wavm_instance_t *instance,
    wasm_val_t args[], wasm_val_t rets[])
{
    ngx_proxy_wasm_exec_t    *pwexec;
    ngx_proxy_wasm_ctx_t     *pwctx;
#ifdef NGX_WASM_HTTP
    ngx_http_wasm_req_ctx_t  *rctx;
#endif

    dd("enter");

    pwexec = ngx_proxy_wasm_instance2pwexec(instance);
    pwctx = pwexec->parent;

    pwctx->action = NGX_PROXY_WASM_ACTION_CONTINUE;

#ifdef NGX_WASM_HTTP
    rctx = ngx_http_proxy_wasm_get_rctx(instance);

    /* force resume */
    rctx->state = NGX_HTTP_WASM_REQ_STATE_CONTINUE;
#endif

    return ngx_proxy_wasm_result_ok(rets);
}


static ngx_int_t
ngx_proxy_wasm_hfuncs_continue(ngx_wavm_instance_t *instance,
    wasm_val_t args[], wasm_val_t rets[])
{
    ngx_uint_t   type;

    type = args[0].of.i32;

    switch (type) {
    case NGX_PROXY_WASM_STREAM_TYPE_HTTP_REQUEST:
    case NGX_PROXY_WASM_STREAM_TYPE_HTTP_RESPONSE:
        return ngx_proxy_wasm_hfuncs_resume_http_request(instance, args, rets);
    default:
        ngx_wavm_log_error(NGX_LOG_WASM_NYI, instance->log, NULL,
                           "NYI - continue stream type: %ld", type);
        break;
    }

    return ngx_proxy_wasm_result_err(rets);
}


static ngx_int_t
ngx_proxy_wasm_hfuncs_get_shared_data(ngx_wavm_instance_t *instance,
    wasm_val_t args[], wasm_val_t rets[])
{
    ngx_int_t               rc;
    ngx_str_t               key;
    ngx_str_t              *value;
    uint32_t               *value_data, *value_size, *cas;
    uint32_t                wbuf_ptr;
    ngx_wasm_shm_kv_key_t   resolved;
    ngx_proxy_wasm_exec_t  *pwexec = ngx_proxy_wasm_instance2pwexec(instance);

    key.len = args[1].of.i32;
    key.data = NGX_WAVM_HOST_LIFT_SLICE(instance, args[0].of.i32, key.len);
    value_data = NGX_WAVM_HOST_LIFT(instance, args[2].of.i32, uint32_t);
    value_size = NGX_WAVM_HOST_LIFT(instance, args[3].of.i32, uint32_t);
    cas = NGX_WAVM_HOST_LIFT(instance, args[4].of.i32, uint32_t);

    dd("getting \"%.*s\"", (int) key.len, key.data);

    /* resolve key namespace */

    rc = ngx_wasm_shm_kv_resolve_key(&key, &resolved);
    if (rc == NGX_ABORT) {
        return ngx_proxy_wasm_result_trap(pwexec, "attempt to get "
                                          "key/value from a queue", rets,
                                          NGX_WAVM_BAD_USAGE);
    }

    if (rc == NGX_DECLINED) {
        /* TODO: format with key */
        return ngx_proxy_wasm_result_trap(pwexec, "failed getting value "
                                          "from shm (could not resolve "
                                          "namespace)", rets,
                                          NGX_WAVM_BAD_USAGE);
    }

    ngx_wasm_assert(rc == NGX_OK);

    /* get */

    ngx_wasm_shm_lock(resolved.shm);

    rc = ngx_wasm_shm_kv_get_locked(resolved.shm, &key, &value, cas);

    ngx_wasm_shm_unlock(resolved.shm);

    if (rc == NGX_DECLINED) {
        return ngx_proxy_wasm_result_notfound(rets);
    }

    ngx_wasm_assert(rc == NGX_OK);

    /* return value */

    wbuf_ptr = ngx_proxy_wasm_alloc(pwexec, value->len);
    if (wbuf_ptr == 0) {
        /* TODO: format with key */
        return ngx_proxy_wasm_result_trap(pwexec, "failed getting value "
                                          "from shm (no memory)", rets,
                                          NGX_WAVM_ERROR);
    }

    ngx_memcpy(NGX_WAVM_HOST_LIFT_SLICE(instance, wbuf_ptr, value->len),
               value->data, value->len);

    *value_data = wbuf_ptr;
    *value_size = value->len;

    return ngx_proxy_wasm_result_ok(rets);
}


static ngx_int_t
ngx_proxy_wasm_hfuncs_set_shared_data(ngx_wavm_instance_t *instance,
    wasm_val_t args[], wasm_val_t rets[])
{
    uint32_t                cas;
    ngx_int_t               rc, written;
    ngx_str_t               key, value;
    ngx_wasm_shm_kv_key_t   resolved;
    ngx_proxy_wasm_exec_t  *pwexec = ngx_proxy_wasm_instance2pwexec(instance);

    key.len = args[1].of.i32;
    key.data = NGX_WAVM_HOST_LIFT_SLICE(instance, args[0].of.i32, key.len);
    value.len = args[3].of.i32;
    value.data = NGX_WAVM_HOST_LIFT_SLICE(instance, args[2].of.i32, value.len);
    cas = args[4].of.i32;

    dd("setting \"%.*s\" -> \"%.*s\"",
       (int) key.len, key.data,
       (int) value.len, value.data);

    /* resolve key namespace */

    rc = ngx_wasm_shm_kv_resolve_key(&key, &resolved);
    if (rc == NGX_ABORT) {
        /* TODO: format with key */
        return ngx_proxy_wasm_result_trap(pwexec, "attempt to set "
                                          "key/value in a queue", rets,
                                          NGX_WAVM_BAD_USAGE);
    }

    if (rc == NGX_DECLINED) {
        /* TODO: format with key */
        return ngx_proxy_wasm_result_trap(pwexec, "failed setting value "
                                          "to shm (could not resolve "
                                          "namespace)", rets,
                                          NGX_WAVM_BAD_USAGE);
    }

    ngx_wasm_assert(rc == NGX_OK);

    /* set */

    ngx_wasm_shm_lock(resolved.shm);

    /*
     * If the filter passes a NULL value pointer, treat it as a delete request.
     * It is still possible to distinguish between:
     * - Setting an empty value (ptr != NULL, len == 0)
     * - Deleting a k/v pair (ptr == NULL, len == 0)
     */
    rc = ngx_wasm_shm_kv_set_locked(resolved.shm,
                                    &key, value.data ? &value : NULL,
                                    cas, &written);

    ngx_wasm_shm_unlock(resolved.shm);

    if (rc == NGX_ERROR) {
        /* TODO: format with key */
        return ngx_proxy_wasm_result_trap(pwexec, "failed setting value "
                                          "to shm (could not write to slab)",
                                          rets,
                                          NGX_WAVM_ERROR);
    }

    ngx_wasm_assert(rc == NGX_OK);

    if (!written) {
        return ngx_proxy_wasm_result_cas_mismatch(rets);
    }

    return ngx_proxy_wasm_result_ok(rets);
}


static ngx_int_t
ngx_proxy_wasm_hfuncs_register_shared_queue(ngx_wavm_instance_t *instance,
    wasm_val_t args[], wasm_val_t rets[])
{
    uint32_t               *token;
    ngx_int_t               zone_idx;
    ngx_str_t               queue_name;
    ngx_wasm_shm_t         *shm;
    ngx_shm_zone_t         *zone;
    ngx_cycle_t            *cycle = (ngx_cycle_t *) ngx_cycle;
    ngx_proxy_wasm_exec_t  *pwexec = ngx_proxy_wasm_instance2pwexec(instance);

    queue_name.len = args[1].of.i32;
    queue_name.data = NGX_WAVM_HOST_LIFT_SLICE(instance, args[0].of.i32,
                                               queue_name.len);
    token = NGX_WAVM_HOST_LIFT(instance, args[2].of.i32, uint32_t);

    zone_idx = ngx_wasm_shm_lookup_index(&queue_name);
    if (zone_idx == NGX_WASM_SHM_INDEX_NOTFOUND) {
        /* TODO: format with queue name */
        return ngx_proxy_wasm_result_trap(pwexec, "could not find queue", rets,
                                          NGX_WAVM_BAD_USAGE);
    }

    zone = ((ngx_wasm_shm_mapping_t *)
            ngx_wasm_core_shms(cycle)->elts)[zone_idx].zone;

    shm = zone->data;
    if (shm->type != NGX_WASM_SHM_TYPE_QUEUE) {
        /* TODO: format with shm name */
        return ngx_proxy_wasm_result_trap(pwexec, "attempt to use "
                                          "a key/value shm store as a queue",
                                          rets,
                                          NGX_WAVM_BAD_USAGE);
    }

    *token = (uint32_t) zone_idx;

    return ngx_proxy_wasm_result_ok(rets);
}


static ngx_int_t
ngx_proxy_wasm_hfuncs_enqueue_shared_queue(ngx_wavm_instance_t *instance,
    wasm_val_t args[], wasm_val_t rets[])
{
    ngx_int_t               rc;
    ngx_uint_t              token;
    ngx_str_t               data;
    ngx_shm_zone_t         *zone;
    ngx_wasm_shm_t         *shm;
    ngx_proxy_wasm_exec_t  *pwexec = ngx_proxy_wasm_instance2pwexec(instance);

    token = args[0].of.i32;
    data.len = args[2].of.i32;
    data.data = NGX_WAVM_HOST_LIFT_SLICE(instance, args[1].of.i32, data.len);

    /* resolve queue */

    rc = ngx_wasm_shm_queue_resolve(instance->log, token, &zone);
    if (rc == NGX_DECLINED) {
        /* TODO: format with token */
        return ngx_proxy_wasm_result_trap(pwexec, "could not find queue", rets,
                                          NGX_WAVM_BAD_USAGE);
    }

    if (rc == NGX_ABORT) {
        return ngx_proxy_wasm_result_trap(pwexec, "attempt to use "
                                          "a key/value shm store as a queue",
                                          rets,
                                          NGX_WAVM_BAD_USAGE);
    }

    ngx_wasm_assert(rc == NGX_OK);

    shm = zone->data;

    /* push */

    ngx_wasm_shm_lock(shm);
    rc = ngx_wasm_shm_queue_push_locked(shm, &data);
    ngx_wasm_shm_unlock(shm);

    if (rc == NGX_ABORT) {
        /* TODO: format with queue name */
        return ngx_proxy_wasm_result_trap(pwexec, "could not enqueue "
                                          "(queue is full)", rets,
                                          NGX_WAVM_ERROR);
    }

    ngx_wasm_assert(rc == NGX_OK);

    return ngx_proxy_wasm_result_ok(rets);
}


static void *
shared_queue_alloc(ngx_uint_t n, void *ctx)
{
    uint32_t                wasm_ptr;
    ngx_proxy_wasm_exec_t  *pwexec;
    ngx_wavm_instance_t    *instance = ctx;

    pwexec = ngx_proxy_wasm_instance2pwexec(instance);

    wasm_ptr = ngx_proxy_wasm_alloc(pwexec, n);
    if (wasm_ptr == 0) {
        return NULL;
    }

    return ngx_wavm_memory_lift(instance->memory, wasm_ptr, n, 1, NULL);
}


static ngx_int_t
ngx_proxy_wasm_hfuncs_dequeue_shared_queue(ngx_wavm_instance_t *instance,
    wasm_val_t args[], wasm_val_t rets[])
{
    ngx_int_t               rc;
    ngx_uint_t              token;
    ngx_str_t               data;
    ngx_shm_zone_t         *zone;
    ngx_wasm_shm_t         *shm;
    uint32_t               *wasm_data_ptr;
    uint32_t               *wasm_data_size;
    ngx_proxy_wasm_exec_t  *pwexec = ngx_proxy_wasm_instance2pwexec(instance);

    token = args[0].of.i32;
    wasm_data_ptr = NGX_WAVM_HOST_LIFT(instance, args[1].of.i32, uint32_t);
    wasm_data_size = NGX_WAVM_HOST_LIFT(instance, args[2].of.i32, uint32_t);

    /* resolve queue */

    rc = ngx_wasm_shm_queue_resolve(instance->log, token, &zone);
    if (rc == NGX_DECLINED) {
        /* TODO: format with token */
        return ngx_proxy_wasm_result_trap(pwexec, "could not find queue", rets,
                                          NGX_WAVM_BAD_USAGE);
    }

    if (rc == NGX_ABORT) {
        return ngx_proxy_wasm_result_trap(pwexec, "attempt to use "
                                          "a key/value shm store as a queue",
                                          rets,
                                          NGX_WAVM_BAD_USAGE);
    }

    ngx_wasm_assert(rc == NGX_OK);

    shm = zone->data;

    /* pop */

    ngx_wasm_shm_lock(shm);
    rc = ngx_wasm_shm_queue_pop_locked(shm, &data,
                                       shared_queue_alloc, instance);
    ngx_wasm_shm_unlock(shm);

    if (rc == NGX_ERROR) {
        return ngx_proxy_wasm_result_err(rets);
    }

    if (rc == NGX_AGAIN) {
        return ngx_proxy_wasm_result_empty(rets);
    }

    ngx_wasm_assert(rc == NGX_OK);

    /* return value */

    if (data.data) {
        *wasm_data_ptr = (uint32_t) ((char *) data.data -
                                     ngx_wavm_memory_base(instance->memory));

    } else {
        *wasm_data_ptr = 0;
    }

    *wasm_data_size = data.len;

    return ngx_proxy_wasm_result_ok(rets);
}


static ngx_int_t
ngx_proxy_wasm_hfuncs_nop(ngx_wavm_instance_t *instance,
    wasm_val_t args[], wasm_val_t rets[])
{
    return NGX_WAVM_NYI;
}


static ngx_wavm_host_func_def_t  ngx_proxy_wasm_hfuncs[] = {

    /* integration */

    { ngx_string("proxy_abi_version_0_1_0"),
      &ngx_proxy_wasm_hfuncs_nop,
      NULL,
      NULL },

    { ngx_string("proxy_log"),
      &ngx_proxy_wasm_hfuncs_proxy_log,
      ngx_wavm_arity_i32x3,
      ngx_wavm_arity_i32 },

    /* context */

    { ngx_string("proxy_set_effective_context"),
      &ngx_proxy_wasm_hfuncs_set_effective_context,
      ngx_wavm_arity_i32,
      ngx_wavm_arity_i32 },

    { ngx_string("proxy_context_finalize"),
      &ngx_proxy_wasm_hfuncs_nop,
      NULL,
      ngx_wavm_arity_i32 },
    /* legacy: 0.1.0 - 0.2.1 */
    { ngx_string("proxy_done"),
      &ngx_proxy_wasm_hfuncs_nop,
      NULL,
      ngx_wavm_arity_i32 },

    /* stream */

    /* rust-sdk < 0.2.0 */
    { ngx_string("proxy_resume_stream"),
      &ngx_proxy_wasm_hfuncs_nop,
      ngx_wavm_arity_i32,
      ngx_wavm_arity_i32 },

    /* rust-sdk >= 0.2.0 */
    { ngx_string("proxy_continue_stream"),
      &ngx_proxy_wasm_hfuncs_continue,
      ngx_wavm_arity_i32,
      ngx_wavm_arity_i32 },

    { ngx_string("proxy_close_stream"),
      &ngx_proxy_wasm_hfuncs_nop,
      ngx_wavm_arity_i32,
      ngx_wavm_arity_i32 },

    /* http */

#ifdef NGX_WASM_HTTP
    { ngx_string("proxy_send_http_response"),
      &ngx_proxy_wasm_hfuncs_send_local_response,
      ngx_wavm_arity_i32x8,
      ngx_wavm_arity_i32 },
    /* legacy: 0.1.0 - 0.2.1 */
    { ngx_string("proxy_send_local_response"),
      &ngx_proxy_wasm_hfuncs_send_local_response,
      ngx_wavm_arity_i32x8,
      ngx_wavm_arity_i32 },

    { ngx_string("proxy_resume_http_stream"),
      &ngx_proxy_wasm_hfuncs_nop,
      ngx_wavm_arity_i32,
      ngx_wavm_arity_i32 },
    /* legacy: 0.1.0 - 0.2.1 */
    { ngx_string("proxy_continue_request"),
      &ngx_proxy_wasm_hfuncs_resume_http_request,
      NULL,
      ngx_wavm_arity_i32 },
    /* legacy: 0.1.0 - 0.2.1 */
    { ngx_string("proxy_continue_response"),
      &ngx_proxy_wasm_hfuncs_nop,
      NULL,
      ngx_wavm_arity_i32 },
#endif

    /* buffers */

    { ngx_string("proxy_get_buffer"),
      &ngx_proxy_wasm_hfuncs_get_buffer,
      ngx_wavm_arity_i32x5,
      ngx_wavm_arity_i32 },
    { ngx_string("proxy_get_buffer_bytes"),
      &ngx_proxy_wasm_hfuncs_get_buffer,
      ngx_wavm_arity_i32x5,
      ngx_wavm_arity_i32 },

    { ngx_string("proxy_set_buffer"),
      &ngx_proxy_wasm_hfuncs_set_buffer,
      ngx_wavm_arity_i32x5,
      ngx_wavm_arity_i32 },
    { ngx_string("proxy_set_buffer_bytes"),
      &ngx_proxy_wasm_hfuncs_set_buffer,
      ngx_wavm_arity_i32x5,
      ngx_wavm_arity_i32 },

    /* maps */

    { ngx_string("proxy_get_map_values"),
      &ngx_proxy_wasm_hfuncs_nop,
      ngx_wavm_arity_i32x5,
      ngx_wavm_arity_i32 },
    { ngx_string("proxy_get_header_map_pairs"),
      &ngx_proxy_wasm_hfuncs_get_header_map_pairs,
      ngx_wavm_arity_i32x3,
      ngx_wavm_arity_i32 },
    { ngx_string("proxy_get_header_map_value"),
      &ngx_proxy_wasm_hfuncs_get_header_map_value,
      ngx_wavm_arity_i32x5,
      ngx_wavm_arity_i32 },

    { ngx_string("proxy_set_map_values"),
      &ngx_proxy_wasm_hfuncs_nop,
      ngx_wavm_arity_i32x5,
      ngx_wavm_arity_i32 },
    { ngx_string("proxy_set_header_map_pairs"),
      &ngx_proxy_wasm_hfuncs_set_header_map_pairs,
      ngx_wavm_arity_i32x3,
      ngx_wavm_arity_i32 },
    { ngx_string("proxy_add_header_map_value"),
      &ngx_proxy_wasm_hfuncs_add_header_map_value,
      ngx_wavm_arity_i32x5,
      ngx_wavm_arity_i32 },
    { ngx_string("proxy_replace_header_map_value"),
      &ngx_proxy_wasm_hfuncs_replace_header_map_value,
      ngx_wavm_arity_i32x5,
      ngx_wavm_arity_i32 },
    { ngx_string("proxy_remove_header_map_value"),
      &ngx_proxy_wasm_hfuncs_remove_header_map_value,
      ngx_wavm_arity_i32x3,
      ngx_wavm_arity_i32 },

    /* shared k/v store */

    { ngx_string("proxy_open_shared_kvstore"),
      &ngx_proxy_wasm_hfuncs_nop,
      ngx_wavm_arity_i32x4,
      ngx_wavm_arity_i32 },

    { ngx_string("proxy_get_shared_kvstore_key_values"),
      &ngx_proxy_wasm_hfuncs_nop,
      ngx_wavm_arity_i32x6,
      ngx_wavm_arity_i32 },
    { ngx_string("proxy_get_shared_data"),
      &ngx_proxy_wasm_hfuncs_get_shared_data,
      ngx_wavm_arity_i32x5,
      ngx_wavm_arity_i32 },

    { ngx_string("proxy_set_shared_kvstore_key_values"),
      &ngx_proxy_wasm_hfuncs_nop,
      ngx_wavm_arity_i32x6,
      ngx_wavm_arity_i32 },
    { ngx_string("proxy_set_shared_data"),
      &ngx_proxy_wasm_hfuncs_set_shared_data,
      ngx_wavm_arity_i32x5,
      ngx_wavm_arity_i32 },

    { ngx_string("proxy_add_shared_kvstore_key_values"),
      &ngx_proxy_wasm_hfuncs_nop,
      ngx_wavm_arity_i32x6,
      ngx_wavm_arity_i32 },

    { ngx_string("proxy_remove_shared_kvstore_key"),
      &ngx_proxy_wasm_hfuncs_nop,
      ngx_wavm_arity_i32x6,
      ngx_wavm_arity_i32 },

    { ngx_string("proxy_delete_shared_kvstore"),
      &ngx_proxy_wasm_hfuncs_nop,
      ngx_wavm_arity_i32x6,
      ngx_wavm_arity_i32 },

    /* shared queue */

    { ngx_string("proxy_open_shared_queue"),
      &ngx_proxy_wasm_hfuncs_nop,
      ngx_wavm_arity_i32x4,
      ngx_wavm_arity_i32 },
    { ngx_string("proxy_register_shared_queue"),
      &ngx_proxy_wasm_hfuncs_register_shared_queue,
      ngx_wavm_arity_i32x3,
      ngx_wavm_arity_i32 },

    { ngx_string("proxy_dequeue_shared_queue_item"),
      &ngx_proxy_wasm_hfuncs_nop,
      ngx_wavm_arity_i32x3,
      ngx_wavm_arity_i32 },
    { ngx_string("proxy_dequeue_shared_queue"),
      &ngx_proxy_wasm_hfuncs_dequeue_shared_queue,
      ngx_wavm_arity_i32x3,
      ngx_wavm_arity_i32 },

    { ngx_string("proxy_enqueue_shared_queue_item"),
      &ngx_proxy_wasm_hfuncs_nop,
      ngx_wavm_arity_i32x3,
      ngx_wavm_arity_i32 },
    { ngx_string("proxy_enqueue_shared_queue"),
      &ngx_proxy_wasm_hfuncs_enqueue_shared_queue,
      ngx_wavm_arity_i32x3,
      ngx_wavm_arity_i32 },

    { ngx_string("proxy_delete_shared_queue"),
      &ngx_proxy_wasm_hfuncs_nop,
      ngx_wavm_arity_i32x4,
      ngx_wavm_arity_i32 },

    /* timers */

    { ngx_string("proxy_create_timer"),
      &ngx_proxy_wasm_hfuncs_nop,
      ngx_wavm_arity_i32x3,
      ngx_wavm_arity_i32 },

    { ngx_string("proxy_delete_timer"),
      &ngx_proxy_wasm_hfuncs_nop,
      ngx_wavm_arity_i32,
      ngx_wavm_arity_i32 },

    /* stats/metrics */

    { ngx_string("proxy_create_metric"),
      &ngx_proxy_wasm_hfuncs_nop,
      ngx_wavm_arity_i32x4,
      ngx_wavm_arity_i32 },
    { ngx_string("proxy_define_metric"),
      &ngx_proxy_wasm_hfuncs_nop,
      ngx_wavm_arity_i32x4,
      ngx_wavm_arity_i32 },

    { ngx_string("proxy_get_metric_value"),
      &ngx_proxy_wasm_hfuncs_nop,
      ngx_wavm_arity_i32x2,
      ngx_wavm_arity_i32 },
    { ngx_string("proxy_get_metric"),
      &ngx_proxy_wasm_hfuncs_nop,
      ngx_wavm_arity_i32x2,
      ngx_wavm_arity_i32 },

    { ngx_string("proxy_set_metric_value"),
      &ngx_proxy_wasm_hfuncs_nop,
      ngx_wavm_arity_i32_i64,
      ngx_wavm_arity_i32 },
    { ngx_string("proxy_record_metric"),
      &ngx_proxy_wasm_hfuncs_nop,
      ngx_wavm_arity_i32_i64,
      ngx_wavm_arity_i32 },

    { ngx_string("proxy_increment_metric_value"),
      &ngx_proxy_wasm_hfuncs_nop,
      ngx_wavm_arity_i32_i64,
      ngx_wavm_arity_i32 },
    { ngx_string("proxy_increment_metric"),
      &ngx_proxy_wasm_hfuncs_nop,
      ngx_wavm_arity_i32_i64,
      ngx_wavm_arity_i32 },

    { ngx_string("proxy_delete_metric"),
      &ngx_proxy_wasm_hfuncs_nop,
      ngx_wavm_arity_i32,
      ngx_wavm_arity_i32 },
    { ngx_string("proxy_remove_metric"),
      &ngx_proxy_wasm_hfuncs_nop,
      ngx_wavm_arity_i32,
      ngx_wavm_arity_i32 },

    /* http callouts */

    { ngx_string("proxy_dispatch_http_call"),
      &ngx_proxy_wasm_hfuncs_dispatch_http_call,
      ngx_wavm_arity_i32x10,
      ngx_wavm_arity_i32 },
    /* legacy: 0.1.0 - 0.2.1 */
    { ngx_string("proxy_http_call"),
      &ngx_proxy_wasm_hfuncs_dispatch_http_call,
      ngx_wavm_arity_i32x10,
      ngx_wavm_arity_i32 },

    /* grpc callouts */

    { ngx_string("proxy_dispatch_grpc_call"),
      &ngx_proxy_wasm_hfuncs_nop,
      ngx_wavm_arity_i32x12,
      ngx_wavm_arity_i32 },
    /* legacy: 0.2.0 - 0.2.1 */
    { ngx_string("proxy_grpc_call"),
      &ngx_proxy_wasm_hfuncs_nop,
      ngx_wavm_arity_i32x12,
      ngx_wavm_arity_i32 },

    { ngx_string("proxy_open_grpc_stream"),
      &ngx_proxy_wasm_hfuncs_nop,
      ngx_wavm_arity_i32x9,
      ngx_wavm_arity_i32 },
    /* legacy: 0.2.0 - 0.2.1 */
    { ngx_string("proxy_grpc_stream"),
      &ngx_proxy_wasm_hfuncs_nop,
      ngx_wavm_arity_i32x9,
      ngx_wavm_arity_i32 },

    { ngx_string("proxy_send_grpc_stream_message"),
      &ngx_proxy_wasm_hfuncs_nop,
      ngx_wavm_arity_i32x3,
      ngx_wavm_arity_i32 },
    /* legacy: 0.2.0 - 0.2.1 */
    { ngx_string("proxy_grpc_send"),
      &ngx_proxy_wasm_hfuncs_nop,
      ngx_wavm_arity_i32x4,
      ngx_wavm_arity_i32 },

    { ngx_string("proxy_cancel_grpc_call"),
      &ngx_proxy_wasm_hfuncs_nop,
      ngx_wavm_arity_i32,
      ngx_wavm_arity_i32 },
    /* legacy: 0.2.0 - 0.2.1 */
    { ngx_string("proxy_grpc_cancel"),
      &ngx_proxy_wasm_hfuncs_nop,
      ngx_wavm_arity_i32,
      ngx_wavm_arity_i32 },

    { ngx_string("proxy_close_grpc_call"),
      &ngx_proxy_wasm_hfuncs_nop,
      ngx_wavm_arity_i32,
      ngx_wavm_arity_i32 },
    /* legacy: 0.2.0 - 0.2.1 */
    { ngx_string("proxy_grpc_close"),
      &ngx_proxy_wasm_hfuncs_nop,
      ngx_wavm_arity_i32,
      ngx_wavm_arity_i32 },

    /* legacy */
    { ngx_string("proxy_get_status"),
      &ngx_proxy_wasm_hfuncs_nop,
      ngx_wavm_arity_i32x3,
      ngx_wavm_arity_i32 },

    /* custom extension points */

    /* rust-sdk < 0.2.0 */
    { ngx_string("proxy_call_custom_function"),
      &ngx_proxy_wasm_hfuncs_nop,
      ngx_wavm_arity_i32x5,
      ngx_wavm_arity_i32 },

    /* rust-sdk >= 0.2.0 */
    { ngx_string("proxy_call_foreign_function"),
      &ngx_proxy_wasm_hfuncs_nop,
      ngx_wavm_arity_i32x6,
      ngx_wavm_arity_i32 },

    /* legacy: 0.2.1 */

    { ngx_string("proxy_get_log_level"),
      &ngx_proxy_wasm_hfuncs_nop,
      ngx_wavm_arity_i32x2,
      ngx_wavm_arity_i32 },

    /* legacy: 0.1.0 - 0.2.1 */

    { ngx_string("proxy_set_tick_period_milliseconds"),
      &ngx_proxy_wasm_hfuncs_set_tick_period,
      ngx_wavm_arity_i32,
      ngx_wavm_arity_i32 },

    { ngx_string("proxy_get_current_time_nanoseconds"),
      &ngx_proxy_wasm_hfuncs_get_current_time,
      ngx_wavm_arity_i32,
      ngx_wavm_arity_i32 },

    { ngx_string("proxy_get_configuration"),
      &ngx_proxy_wasm_hfuncs_get_configuration,
      ngx_wavm_arity_i32x2,
      ngx_wavm_arity_i32 },

    { ngx_string("proxy_resolve_shared_queue"),
      &ngx_proxy_wasm_hfuncs_nop,
      ngx_wavm_arity_i32x5,
      ngx_wavm_arity_i32 },

    { ngx_string("proxy_clear_route_cache"),
      &ngx_proxy_wasm_hfuncs_nop,
      NULL,
      ngx_wavm_arity_i32 },

    { ngx_string("proxy_get_property"),
      &ngx_proxy_wasm_hfuncs_get_property,
      ngx_wavm_arity_i32x4,
      ngx_wavm_arity_i32 },

    { ngx_string("proxy_set_property"),
      &ngx_proxy_wasm_hfuncs_set_property,
      ngx_wavm_arity_i32x4,
      ngx_wavm_arity_i32 },

    ngx_wavm_hfunc_null
};


ngx_wavm_host_def_t  ngx_proxy_wasm_host = {
    ngx_string("ngx_proxy_wasm"),
    ngx_proxy_wasm_hfuncs,
};
