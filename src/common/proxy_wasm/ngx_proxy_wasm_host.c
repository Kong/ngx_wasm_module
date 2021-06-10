#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#ifdef NGX_WASM_HTTP
#include <ngx_http_proxy_wasm.h>
#endif
#include <ngx_proxy_wasm.h>
#include <ngx_event.h>
#include <ngx_wavm.h>


#define ngx_proxy_wasm_get_pwm(instance)                                     \
    ((ngx_proxy_wasm_t *)                                                    \
     ((u_char *) (instance)->ctx - offsetof(ngx_proxy_wasm_t, wvctx)))


typedef enum {
    NGX_PROXY_WASM_MAP_SET,
    NGX_PROXY_WASM_MAP_ADD,
    NGX_PROXY_WASM_MAP_REMOVE,
} ngx_proxy_wasm_map_op_e;


static ngx_str_t *
ngx_proxy_wasm_get_map_special_key(ngx_wavm_instance_t *instance, u_char *key,
    size_t key_len)
{
#ifdef NGX_WASM_HTTP
    ngx_http_wasm_req_ctx_t    *rctx = instance->ctx->data;
    ngx_http_request_t         *r = rctx->r;
#endif

    switch (key_len) {

#ifdef NGX_WASM_HTTP
    case 5:
        if (ngx_strncmp(key, ":path", key_len) == 0) {
            return &r->uri;
        }

        break;

    case 7:
        if (ngx_strncmp(key, ":method", key_len) == 0) {
            return &r->method_name;
        }

        break;
#endif

    }

    return NULL;
}


static ngx_chain_t *
ngx_proxy_wasm_get_buffer_helper(ngx_wavm_instance_t *instance,
    ngx_proxy_wasm_buffer_type_t buf_type, unsigned *none)
{
#ifdef NGX_WASM_HTTP
    ngx_chain_t              *cl;
    ngx_http_wasm_req_ctx_t  *rctx = instance->ctx->data;
    ngx_http_request_t       *r = rctx->r;
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
            ngx_wasm_log_error(NGX_LOG_ERR, instance->log, 0,
                              "cannot read request body buffered "
                              "to file at \"%V\"",
                              &r->request_body->temp_file->file.name);

            *none = 1;
            return NULL;
        }

        return r->request_body->bufs;

    case NGX_PROXY_WASM_BUFFER_HTTP_RESPONSE_BODY:
        cl = rctx->resp_body_out;
        if (cl == NULL) {
            /* no body */
            *none = 1;
            return NULL;
        }

        return cl;
#endif

    default:
        ngx_wasm_log_error(NGX_LOG_WASM_NYI, instance->log, 0,
                           "NYI - get_buffer: %d", buf_type);
        return NULL;

    }
}


static ngx_list_t *
ngx_proxy_wasm_get_map_helper(ngx_wavm_instance_t *instance,
    ngx_proxy_wasm_map_type_t map_type)
{
#ifdef NGX_WASM_HTTP
    ngx_http_wasm_req_ctx_t  *rctx = instance->ctx->data;
    ngx_http_request_t       *r = rctx->r;
#endif

    /* TODO: add :path, :method... */

    switch (map_type) {

#ifdef NGX_WASM_HTTP
    case NGX_PROXY_WASM_MAP_HTTP_REQUEST_HEADERS:
        return &r->headers_in.headers;

    case NGX_PROXY_WASM_MAP_HTTP_RESPONSE_HEADERS:
        return &r->headers_out.headers;
#endif

    default:
        ngx_wasm_log_error(NGX_LOG_WASM_NYI, instance->log, 0,
                           "NYI - get_map: %d", map_type);
        return NULL;

    }
}


static ngx_int_t
ngx_proxy_wasm_map_set_helper(ngx_wavm_instance_t *instance,
    ngx_proxy_wasm_map_type_t map_type, ngx_str_t *key, ngx_str_t *value,
    ngx_proxy_wasm_map_op_e map_op)
{
    ngx_int_t                 rc = NGX_ERROR;
#ifdef NGX_WASM_HTTP
    ngx_str_t                *str;
    ngx_http_wasm_req_ctx_t  *rctx = instance->ctx->data;
    ngx_http_request_t       *r = rctx->r;
    ngx_uint_t                mode;

    switch (map_op){

    case NGX_PROXY_WASM_MAP_SET:
        str = ngx_proxy_wasm_get_map_special_key(instance, key->data, key->len);
        if (str) {
            /* TODO: set :path, :method... */
            return NGX_OK;
        }

        mode = NGX_HTTP_WASM_HEADERS_SET;
        break;

    case NGX_PROXY_WASM_MAP_ADD:
        mode = NGX_HTTP_WASM_HEADERS_APPEND;
        break;

    case NGX_PROXY_WASM_MAP_REMOVE:
        mode = NGX_HTTP_WASM_HEADERS_REMOVE;
        break;

    }
#endif

    switch (map_type) {

#ifdef NGX_WASM_HTTP
    case NGX_PROXY_WASM_MAP_HTTP_REQUEST_HEADERS:
        rc = ngx_http_wasm_set_req_header(r, key, value, mode);
        break;

    case NGX_PROXY_WASM_MAP_HTTP_RESPONSE_HEADERS:
        rc = ngx_http_wasm_set_resp_header(r, key, value, mode);
        break;
#endif

    default:
        ngx_wasm_assert(0);
        break;

    }

    return rc;
}


static ngx_int_t
ngx_proxy_wasm_map_set_value_helper(ngx_wavm_instance_t *instance,
    ngx_proxy_wasm_map_type_t map_type, u_char *key, size_t klen,
    u_char *value, size_t vlen, ngx_proxy_wasm_map_op_e map_op)
{
    u_char     *p;
    ngx_str_t   skey, svalue;

    skey.len = klen;
    skey.data = ngx_pnalloc(instance->ctx->pool, skey.len + 1);
    if (skey.data == NULL) {
        return NGX_ERROR;
    }

    p = ngx_cpymem(skey.data, key, skey.len);
    *p = '\0';

    svalue.len = vlen;
    svalue.data = ngx_pnalloc(instance->ctx->pool, svalue.len + 1);
    if (svalue.data == NULL) {
        return NGX_ERROR;
    }

    p = ngx_cpymem(svalue.data, value, svalue.len);
    *p = '\0';

    return ngx_proxy_wasm_map_set_helper(instance, map_type,
                                         &skey, &svalue,
                                         map_op);
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
    msg_data = ngx_wavm_memory_lift(instance->memory, args[1].of.i32);
    msg_size = args[2].of.i32;

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
        ngx_wasm_log_error(NGX_LOG_WASM_NYI, instance->log, 0,
                           "NYI: unknown log level \"%d\"", log_level);

        return ngx_proxy_wasm_result_badarg(rets);

    }

    ngx_wasm_log_error(level, instance->log, 0, "%*s",
                       msg_size, msg_data);

    return ngx_proxy_wasm_result_ok(rets);
}


static ngx_int_t
ngx_proxy_wasm_hfuncs_get_buffer(ngx_wavm_instance_t *instance,
    wasm_val_t args[], wasm_val_t rets[])
{
    size_t                        *rlen, offset, max_len, len, chunk_len;
    unsigned                       none = 0;
    ngx_chain_t                   *cl;
    ngx_buf_t                     *buf;
    ngx_wavm_ptr_t                *rbuf, p;
    ngx_proxy_wasm_buffer_type_t   buf_type;
    ngx_proxy_wasm_t              *pwm = ngx_proxy_wasm_get_pwm(instance);

    buf_type = args[0].of.i32;
    offset = args[1].of.i32;
    max_len = args[2].of.i32;
    rbuf = ngx_wavm_memory_lift(instance->memory, args[3].of.i32);
    rlen = ngx_wavm_memory_lift(instance->memory, args[4].of.i32);

    if (offset > offset + max_len) {
        /* overflow */
        return ngx_proxy_wasm_result_badarg(rets);
    }

    cl = ngx_proxy_wasm_get_buffer_helper(instance, buf_type, &none);
    if (cl == NULL) {
        if (none) {
            /* no body or buffered to file */
            return ngx_proxy_wasm_result_notfound(rets);
        }

        return ngx_proxy_wasm_result_badarg(rets);
    }

    len = ngx_wasm_chain_len(cl);
    len = ngx_min(len, max_len);

    if (!len) {
        /* eof */
        return ngx_proxy_wasm_result_notfound(rets);
    }

    p = ngx_proxy_wasm_alloc(pwm, len);
    if (p == 0) {
        return ngx_proxy_wasm_result_err(rets);
    }

    *rbuf = p;
    *rlen = len;

    if (cl->next == NULL) {
        /* single buffer */
        buf = cl->buf;

        if (!ngx_wavm_memory_memcpy(instance->memory, p, buf->pos, len)) {
            return ngx_proxy_wasm_result_invalid_mem(rets);
        }

    } else {
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
    size_t                         offset, max, buf_len;
    ngx_int_t                      rc = NGX_ERROR;
    ngx_str_t                      s;
    ngx_wavm_ptr_t                *buf_data;
    ngx_proxy_wasm_buffer_type_t   buf_type;
    ngx_http_wasm_req_ctx_t       *rctx = instance->ctx->data;
    ngx_http_request_t            *r = rctx->r;

    buf_type = args[0].of.i32;
    offset = args[1].of.i32;
    max = args[2].of.i32;
    buf_data = ngx_wavm_memory_lift(instance->memory, args[3].of.i32);
    buf_len = args[4].of.i32;

    s.len = buf_len;
    s.data = (u_char *) buf_data;

    switch (buf_type) {

#ifdef NGX_WASM_HTTP
    case NGX_PROXY_WASM_BUFFER_HTTP_REQUEST_BODY:
        rc = ngx_http_wasm_set_req_body(r, &s, offset, max);
        break;

    case NGX_PROXY_WASM_BUFFER_HTTP_RESPONSE_BODY:
        rc = ngx_http_wasm_set_resp_body(r, &s);
        break;
#endif

    default:
        ngx_wasm_log_error(NGX_LOG_WASM_NYI, instance->log, 0,
                           "NYI - set_buffer: %d", buf_type);
        break;

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
    size_t                     *rlen;
    ngx_wavm_ptr_t             *rbuf;
    ngx_proxy_wasm_map_type_t   map_type;
    ngx_proxy_wasm_t           *pwm = ngx_proxy_wasm_get_pwm(instance);
    ngx_list_t                 *list;
    ngx_uint_t                  truncated = 0;
    ngx_array_t                *shim_pairs = NULL;

    map_type = args[0].of.i32;
    rbuf = ngx_wavm_memory_lift(instance->memory, args[1].of.i32);
    rlen = ngx_wavm_memory_lift(instance->memory, args[2].of.i32);

    list = ngx_proxy_wasm_get_map_helper(instance, map_type);
    if (list == NULL) {
        return ngx_proxy_wasm_result_badarg(rets);
    }

#ifdef NGX_WASM_HTTP
    if (map_type == NGX_PROXY_WASM_MAP_HTTP_RESPONSE_HEADERS) {
        /* inject shim response headers (produced by ngx_http_header_filter) */
        shim_pairs = ngx_http_wasm_get_shim_headers(
                         ngx_http_proxy_wasm_rctx(pwm));
    }
#endif

    if (!ngx_proxy_wasm_marshal(pwm, list, shim_pairs, rbuf, rlen,
                                &truncated))
    {
        return ngx_proxy_wasm_result_invalid_mem(rets);
    }

    if (truncated) {
        ngx_proxy_wasm_log_error(NGX_LOG_WARN, pwm->log, 0,
                                 "marshalled map truncated to %ui elements",
                                 truncated);
    }

    return ngx_proxy_wasm_result_ok(rets);
}


static ngx_int_t
ngx_proxy_wasm_hfuncs_get_header_map_value(ngx_wavm_instance_t *instance,
    wasm_val_t args[], wasm_val_t rets[])
{
    ngx_proxy_wasm_map_type_t   map_type;
    uint32_t                    key_len;
    ngx_wavm_ptr_t              p;
    uintptr_t                  *rbuf;
    size_t                     *rlen;
    u_char                     *key;
    ngx_list_t                 *list;
    ngx_str_t                  *value;
    ngx_proxy_wasm_t           *pwm;

    pwm = ngx_proxy_wasm_get_pwm(instance);

    map_type = args[0].of.i32;
    key = ngx_wavm_memory_lift(instance->memory, args[1].of.i32);
    key_len = args[2].of.i32;
    rbuf = ngx_wavm_memory_lift(instance->memory, args[3].of.i32);
    rlen = ngx_wavm_memory_lift(instance->memory, args[4].of.i32);

    /* :path, :method... */

    value = ngx_proxy_wasm_get_map_special_key(instance, key, key_len);
    if (value == NULL) {
        /* header lookup */

        list = ngx_proxy_wasm_get_map_helper(instance, map_type);
        if (list == NULL) {
            return ngx_proxy_wasm_result_badarg(rets);
        }

        value = ngx_wasm_get_list_elem(list, (u_char *) key, key_len);

#ifdef NGX_WASM_HTTP
        if (value == NULL
            && map_type == NGX_PROXY_WASM_MAP_HTTP_RESPONSE_HEADERS)
        {
            /* shim header lookup */

            value = ngx_http_wasm_get_shim_header(ngx_http_proxy_wasm_rctx(pwm),
                                                  (u_char *) key,
                                                  key_len);
        }
#endif

        if (value == NULL) {
            if (pwm->abi_version == NGX_PROXY_WASM_0_1_0) {
                rbuf = NULL;
                *rlen = 0;
                return ngx_proxy_wasm_result_ok(rets);
            }

            return ngx_proxy_wasm_result_notfound(rets);
        }
    }

    ngx_wasm_assert(value);

    p = ngx_proxy_wasm_alloc(pwm, value->len);
    if (p == 0) {
        return ngx_proxy_wasm_result_err(rets);
    }

    if (!ngx_wavm_memory_memcpy(instance->memory, p, value->data, value->len)) {
        return ngx_proxy_wasm_result_invalid_mem(rets);
    }

    *rbuf = p;
    *rlen = value->len;

    return ngx_proxy_wasm_result_ok(rets);
}


static ngx_int_t
ngx_proxy_wasm_hfuncs_set_header_map_pairs(ngx_wavm_instance_t *instance,
    wasm_val_t args[], wasm_val_t rets[])
{
    size_t                      map_size, i;
    ngx_int_t                   rc = NGX_ERROR;
    ngx_wavm_ptr_t             *map_data;
    ngx_proxy_wasm_map_type_t   map_type;
    ngx_array_t                *headers;
    ngx_table_elt_t            *elt;

    map_type = args[0].of.i32;
    map_data = ngx_wavm_memory_lift(instance->memory, args[1].of.i32);
    map_size = args[2].of.i32;

    headers = ngx_proxy_wasm_pairs_unmarshal(instance->ctx->pool,
                                             (u_char *) map_data, map_size);
    if (headers == NULL) {
        return ngx_proxy_wasm_result_err(rets);
    }

    switch (map_type) {

#ifdef NGX_WASM_HTTP
    case NGX_PROXY_WASM_MAP_HTTP_REQUEST_HEADERS:
        rc = ngx_http_wasm_clear_req_headers(
                 ngx_http_proxy_wasm_request(
                     ngx_proxy_wasm_get_pwm(instance)));
        break;

    case NGX_PROXY_WASM_MAP_HTTP_RESPONSE_HEADERS:
        rc = ngx_http_wasm_clear_resp_headers(
                 ngx_http_proxy_wasm_request(
                     ngx_proxy_wasm_get_pwm(instance)));
        break;
#endif

    default:
        ngx_wasm_assert(0);
        break;

    }

    if (rc != NGX_OK) {
        return ngx_proxy_wasm_result_err(rets);
    }

    for (i = 0; i < headers->nelts; i++) {
        elt = &((ngx_table_elt_t *) headers->elts)[i];

        if (ngx_proxy_wasm_map_set_helper(instance, map_type,
                                          &elt->key, &elt->value,
                                          NGX_PROXY_WASM_MAP_SET)
            != NGX_OK)
        {
            return ngx_proxy_wasm_result_err(rets);
        }
    }

    return ngx_proxy_wasm_result_ok(rets);
}


static ngx_int_t
ngx_proxy_wasm_hfuncs_add_header_map_value(ngx_wavm_instance_t *instance,
    wasm_val_t args[], wasm_val_t rets[])
{
    size_t                      klen, vlen;
    ngx_wavm_ptr_t             *key, *value;
    ngx_proxy_wasm_map_type_t   map_type;

    map_type = args[0].of.i32;
    key = ngx_wavm_memory_lift(instance->memory, args[1].of.i32);
    klen = args[2].of.i32;
    value = ngx_wavm_memory_lift(instance->memory, args[3].of.i32);
    vlen = args[4].of.i32;

    dd("adding '%.*s: %.*s' to map of type '%d'",
       (int) klen, (u_char *) key, (int) vlen, (u_char *) value, map_type);

    if (ngx_proxy_wasm_map_set_value_helper(instance, map_type,
                                            (u_char *) key, klen,
                                            (u_char *) value, vlen,
                                            NGX_PROXY_WASM_MAP_ADD)
        != NGX_OK)
    {
        return ngx_proxy_wasm_result_err(rets);
    }

    return ngx_proxy_wasm_result_ok(rets);
}


static ngx_int_t
ngx_proxy_wasm_hfuncs_replace_header_map_value(ngx_wavm_instance_t *instance,
    wasm_val_t args[], wasm_val_t rets[])
{
    size_t                      klen, vlen;
    ngx_wavm_ptr_t             *key, *value;
    ngx_proxy_wasm_map_type_t   map_type;

    map_type = args[0].of.i32;
    key = ngx_wavm_memory_lift(instance->memory, args[1].of.i32);
    klen = args[2].of.i32;
    value = ngx_wavm_memory_lift(instance->memory, args[3].of.i32);
    vlen = args[4].of.i32;

    dd("setting '%.*s: %.*s' into map of type '%d'",
       (int) klen, (u_char *) key, (int) vlen, (u_char *) value, map_type);

    if (ngx_proxy_wasm_map_set_value_helper(instance, map_type,
                                            (u_char *) key, klen,
                                            (u_char *) value, vlen,
                                            NGX_PROXY_WASM_MAP_SET)
        != NGX_OK)
    {
        return ngx_proxy_wasm_result_err(rets);
    }

    return ngx_proxy_wasm_result_ok(rets);
}


static ngx_int_t
ngx_proxy_wasm_hfuncs_remove_header_map_value(ngx_wavm_instance_t *instance,
    wasm_val_t args[], wasm_val_t rets[])
{
    size_t                      klen;
    ngx_wavm_ptr_t             *key;
    ngx_proxy_wasm_map_type_t   map_type;
    ngx_str_t                   skey;

    map_type = args[0].of.i32;
    key = ngx_wavm_memory_lift(instance->memory, args[1].of.i32);
    klen = args[2].of.i32;

    dd("removing '%.*s' from map of type '%d'",
       (int) klen, (u_char *) key, map_type);

    skey.data = (u_char *) key;
    skey.len = klen;

    if (ngx_proxy_wasm_map_set_helper(instance, map_type,
                                      &skey, NULL,
                                      NGX_PROXY_WASM_MAP_REMOVE)
        != NGX_OK)
    {
        return ngx_proxy_wasm_result_err(rets);
    }

    return ngx_proxy_wasm_result_ok(rets);
}


static ngx_int_t
ngx_proxy_wasm_hfuncs_set_tick_period(ngx_wavm_instance_t *instance,
    wasm_val_t args[], wasm_val_t rets[])
{
    ngx_event_t       *ev;
    ngx_proxy_wasm_t  *pwm;
    uint32_t           period = args[0].of.i32;

    pwm = instance->ctx->data;

    if (ngx_exiting) {
        ngx_wavm_instance_trap_printf(instance, "process exiting");
        return NGX_WAVM_ERROR;
    }

    if (pwm->tick_period) {
        ngx_wavm_instance_trap_printf(instance, "tick_period already set");
        return NGX_WAVM_ERROR;
    }

    pwm->tick_period = period;

    ev = ngx_calloc(sizeof(ngx_event_t), instance->log);
    if (ev == NULL) {
        goto nomem;
    }

    ev->handler = ngx_proxy_wasm_tick_handler;
    ev->data = pwm;
    ev->log = pwm->log;

    ngx_add_timer(ev, pwm->tick_period);

    return ngx_proxy_wasm_result_ok(rets);

nomem:

    ngx_wavm_instance_trap_printf(instance, "no memory");
    return NGX_WAVM_ERROR;
}


static ngx_int_t
ngx_proxy_wasm_hfuncs_get_current_time(ngx_wavm_instance_t *instance,
    wasm_val_t args[], wasm_val_t rets[])
{
    uint64_t    *rtime;
    ngx_time_t  *tp;

    rtime = ngx_wavm_memory_lift(instance->memory, args[0].of.i32);

    ngx_time_update();

    tp = ngx_timeofday();

    *rtime = (tp->sec * 1000 + tp->msec) * 1e6;

    return ngx_proxy_wasm_result_ok(rets);
}


#ifdef NGX_WASM_HTTP
static ngx_int_t
ngx_proxy_wasm_hfuncs_send_local_response(ngx_wavm_instance_t *instance,
    wasm_val_t args[], wasm_val_t rets[])
{
    int32_t                   status, reason_len, body_len, headers_len
#if (NGX_DEBUG)
                              , grpc_status
#endif
                              ;
    u_char                   *reason, *body, *marsh_headers;
    ngx_int_t                 rc;
    ngx_array_t              *headers = NULL;
    ngx_http_wasm_req_ctx_t  *rctx = instance->ctx->data;
    ngx_http_request_t       *r = rctx->r;
    ngx_proxy_wasm_t         *pwm;

    status = args[0].of.i32;
    reason = ngx_wavm_memory_lift(instance->memory, args[1].of.i32);
    reason_len = args[2].of.i32;
    body = ngx_wavm_memory_lift(instance->memory, args[3].of.i32);
    body_len = args[4].of.i32;
    marsh_headers = ngx_wavm_memory_lift(instance->memory, args[5].of.i32);
    headers_len = args[6].of.i32;
#if (NGX_DEBUG)
    grpc_status = args[7].of.i32;

    ngx_wasm_assert(grpc_status == -1);
#endif

    if (headers_len) {
        headers = ngx_proxy_wasm_pairs_unmarshal(r->pool, marsh_headers,
                                                 headers_len);
        if (headers == NULL) {
            return ngx_proxy_wasm_result_err(rets);
        }
    }

    rc = ngx_http_wasm_stash_local_response(rctx, status, reason, reason_len,
                                            headers, body, body_len);
    switch (rc) {

    case NGX_OK:
        break;

    case NGX_ERROR:
        return ngx_proxy_wasm_result_err(rets);

    case NGX_DECLINED:
        return ngx_proxy_wasm_result_badarg(rets);

    case NGX_BUSY:
        pwm = ngx_proxy_wasm_get_pwm(instance);
        return ngx_proxy_wasm_result_trap(pwm, "local response already stashed",
                                          rets);

    case NGX_ABORT:
        pwm = ngx_proxy_wasm_get_pwm(instance);
        return ngx_proxy_wasm_result_trap(pwm, "response already sent", rets);

    default:
        /* unreachable */
        ngx_wasm_assert(0);
        return NGX_WAVM_NYI;

    }

    return ngx_proxy_wasm_result_ok(rets);
}
#endif


static ngx_int_t
ngx_proxy_wasm_hfuncs_get_configuration(ngx_wavm_instance_t *instance,
    wasm_val_t args[], wasm_val_t rets[])
{
    size_t                     *rlen;
    ngx_wavm_ptr_t             *rbuf, p;
    ngx_proxy_wasm_t           *pwm;

    pwm = ngx_proxy_wasm_get_pwm(instance);

    rbuf = ngx_wavm_memory_lift(instance->memory, args[0].of.i32);
    rlen = ngx_wavm_memory_lift(instance->memory, args[1].of.i32);

    if (pwm->filter_config.len) {
        p = ngx_proxy_wasm_alloc(pwm, pwm->filter_config.len);
        if (p == 0) {
            return ngx_proxy_wasm_result_err(rets);
        }

        if (!ngx_wavm_memory_memcpy(instance->memory, p,
                                    pwm->filter_config.data,
                                    pwm->filter_config.len))
        {
            return ngx_proxy_wasm_result_invalid_mem(rets);
        }

        *rbuf = p;
        *rlen = pwm->filter_config.len;
    }

    return ngx_proxy_wasm_result_ok(rets);
}


static ngx_int_t
ngx_proxy_wasm_hfuncs_nop(ngx_wavm_instance_t *instance,
    wasm_val_t args[], wasm_val_t rets[])
{
    ngx_wasm_log_error(NGX_LOG_WASM_NYI, instance->log, 0,
                       "NYI: proxy_wasm hfunc");

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
     &ngx_proxy_wasm_hfuncs_nop,
     ngx_wavm_arity_i32,
     ngx_wavm_arity_i32 },

   { ngx_string("proxy_context_finalize"),
     &ngx_proxy_wasm_hfuncs_nop,
     NULL,
     ngx_wavm_arity_i32 },

   /* 0.1.0 - 0.2.1 */
   { ngx_string("proxy_done"),
     &ngx_proxy_wasm_hfuncs_nop,
     NULL,
     ngx_wavm_arity_i32 },

   /* stream */

   { ngx_string("proxy_resume_stream"),
     &ngx_proxy_wasm_hfuncs_nop,
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
   /* 0.1.0 - 0.2.1 */
   { ngx_string("proxy_send_local_response"),
     &ngx_proxy_wasm_hfuncs_send_local_response,
     ngx_wavm_arity_i32x8,
     ngx_wavm_arity_i32 },

   { ngx_string("proxy_resume_http_stream"),
     &ngx_proxy_wasm_hfuncs_nop,
     ngx_wavm_arity_i32,
     ngx_wavm_arity_i32 },
   /* 0.1.0 - 0.2.1 */
   { ngx_string("proxy_continue_request"),
     &ngx_proxy_wasm_hfuncs_nop,
     NULL,
     ngx_wavm_arity_i32 },
   { ngx_string("proxy_continue_response"),
     &ngx_proxy_wasm_hfuncs_nop,
     NULL,
     ngx_wavm_arity_i32 },
   /* 0.2.0 - 0.2.1 */
   { ngx_string("proxy_continue_stream"),
     &ngx_proxy_wasm_hfuncs_nop,
     ngx_wavm_arity_i32,
     ngx_wavm_arity_i32 },
   /* 0.2.0 - 0.2.1 */
   { ngx_string("proxy_close_stream"),
     &ngx_proxy_wasm_hfuncs_nop,
     ngx_wavm_arity_i32,
     ngx_wavm_arity_i32 },
   /* 0.2.?+ */
   { ngx_string("proxy_grpc_stream"),
     &ngx_proxy_wasm_hfuncs_nop,
     ngx_wavm_arity_i64,
     ngx_wavm_arity_i32 },
   { ngx_string("proxy_grpc_call"),
     &ngx_proxy_wasm_hfuncs_nop,
     ngx_wavm_arity_i64,
     ngx_wavm_arity_i32 },
   { ngx_string("proxy_grpc_send"),
     &ngx_proxy_wasm_hfuncs_nop,
     ngx_wavm_arity_i64,
     ngx_wavm_arity_i32 },
   { ngx_string("proxy_grpc_close"),
     &ngx_proxy_wasm_hfuncs_nop,
     ngx_wavm_arity_i64,
     ngx_wavm_arity_i32 },
   { ngx_string("proxy_grpc_cancel"),
     &ngx_proxy_wasm_hfuncs_nop,
     ngx_wavm_arity_i64,
     ngx_wavm_arity_i32 },

   { ngx_string("proxy_close_http_stream"),
     &ngx_proxy_wasm_hfuncs_nop,
     ngx_wavm_arity_i32,
     ngx_wavm_arity_i32 },

#endif

   /* buffers */

   { ngx_string("proxy_get_buffer"),
     &ngx_proxy_wasm_hfuncs_get_buffer,
     ngx_wavm_arity_i32x5,
     ngx_wavm_arity_i32 },
   /* 0.1.0 - 0.2.1 */
   { ngx_string("proxy_get_buffer_bytes"),
     &ngx_proxy_wasm_hfuncs_get_buffer,
     ngx_wavm_arity_i32x5,
     ngx_wavm_arity_i32 },

   { ngx_string("proxy_set_buffer"),
     &ngx_proxy_wasm_hfuncs_set_buffer,
     ngx_wavm_arity_i32x5,
     ngx_wavm_arity_i32 },
   /* 0.1.0 - 0.2.1 */
   { ngx_string("proxy_set_buffer_bytes"),
     &ngx_proxy_wasm_hfuncs_set_buffer,
     ngx_wavm_arity_i32x5,
     ngx_wavm_arity_i32 },

   /* maps */

   { ngx_string("proxy_get_map_values"),
     &ngx_proxy_wasm_hfuncs_nop,
     ngx_wavm_arity_i32x5,
     ngx_wavm_arity_i32 },
   /* 0.1.0 - 0.2.1 */
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
   /* 0.1.0 - 0.2.1 */
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
   /* 0.1.0 - 0.2.1 */
   { ngx_string("proxy_get_shared_data"),
     &ngx_proxy_wasm_hfuncs_nop,
     ngx_wavm_arity_i32x5,
     ngx_wavm_arity_i32 },

   { ngx_string("proxy_set_shared_kvstore_key_values"),
     &ngx_proxy_wasm_hfuncs_nop,
     ngx_wavm_arity_i32x6,
     ngx_wavm_arity_i32 },
   /* 0.1.0 - 0.2.1 */
   { ngx_string("proxy_set_shared_data"),
     &ngx_proxy_wasm_hfuncs_nop,
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

   /* 0.1.0 - 0.2.1 */
   { ngx_string("proxy_resolve_shared_queue"),
     &ngx_proxy_wasm_hfuncs_nop,
     ngx_wavm_arity_i32x5,
     ngx_wavm_arity_i32 },

   /* shared queue */

   { ngx_string("proxy_open_shared_queue"),
     &ngx_proxy_wasm_hfuncs_nop,
     ngx_wavm_arity_i32x4,
     ngx_wavm_arity_i32 },
   /* 0.1.0 - 0.2.1 */
   { ngx_string("proxy_register_shared_queue"),
     &ngx_proxy_wasm_hfuncs_nop,
     ngx_wavm_arity_i32x3,
     ngx_wavm_arity_i32 },

   { ngx_string("proxy_dequeue_shared_queue_item"),
     &ngx_proxy_wasm_hfuncs_nop,
     ngx_wavm_arity_i32x3,
     ngx_wavm_arity_i32 },
   /* 0.1.0 - 0.2.1 */
   { ngx_string("proxy_dequeue_shared_queue"),
     &ngx_proxy_wasm_hfuncs_nop,
     ngx_wavm_arity_i32x3,
     ngx_wavm_arity_i32 },

   { ngx_string("proxy_enqueue_shared_queue_item"),
     &ngx_proxy_wasm_hfuncs_nop,
     ngx_wavm_arity_i32x3,
     ngx_wavm_arity_i32 },
   { ngx_string("proxy_enqueue_shared_queue"),
     &ngx_proxy_wasm_hfuncs_nop,
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
   /* 0.1.0 - 0.2.1 */
   { ngx_string("proxy_define_metric"),
     &ngx_proxy_wasm_hfuncs_nop,
     ngx_wavm_arity_i32x4,
     ngx_wavm_arity_i32 },

   { ngx_string("proxy_get_metric_value"),
     &ngx_proxy_wasm_hfuncs_nop,
     ngx_wavm_arity_i32x2,
     ngx_wavm_arity_i32 },
   /* 0.1.0 - 0.2.1 */
   { ngx_string("proxy_get_metric"),
     &ngx_proxy_wasm_hfuncs_nop,
     ngx_wavm_arity_i32x2,
     ngx_wavm_arity_i32 },

   { ngx_string("proxy_set_metric_value"),
     &ngx_proxy_wasm_hfuncs_nop,
     ngx_wavm_arity_i32_i64,
     ngx_wavm_arity_i32 },
   /* 0.1.0 - 0.2.1 */
   { ngx_string("proxy_record_metric"),
     &ngx_proxy_wasm_hfuncs_nop,
     ngx_wavm_arity_i32_i64,
     ngx_wavm_arity_i32 },

   { ngx_string("proxy_increment_metric_value"),
     &ngx_proxy_wasm_hfuncs_nop,
     ngx_wavm_arity_i32_i64,
     ngx_wavm_arity_i32 },
   /* 0.1.0 - 0.2.1 */
   { ngx_string("proxy_increment_metric"),
     &ngx_proxy_wasm_hfuncs_nop,
     ngx_wavm_arity_i32_i64,
     ngx_wavm_arity_i32 },

   { ngx_string("proxy_delete_metric"),
     &ngx_proxy_wasm_hfuncs_nop,
     ngx_wavm_arity_i32,
     ngx_wavm_arity_i32 },

   /* http callouts */

   { ngx_string("proxy_dispatch_http_call"),
     &ngx_proxy_wasm_hfuncs_nop,
     ngx_wavm_arity_i32x10,
     ngx_wavm_arity_i32 },
   /* 0.1.0 - 0.2.1 */
   { ngx_string("proxy_http_call"),
     &ngx_proxy_wasm_hfuncs_nop,
     ngx_wavm_arity_i32x10,
     ngx_wavm_arity_i32 },

   /* grpc callouts */

   { ngx_string("proxy_dispatch_grpc_call"),
     &ngx_proxy_wasm_hfuncs_nop,
     ngx_wavm_arity_i32x12,
     ngx_wavm_arity_i32 },
   /* 0.2.0 - 0.2.1 */
   { ngx_string("proxy_grpc_call"),
     &ngx_proxy_wasm_hfuncs_nop,
     ngx_wavm_arity_i32x12,
     ngx_wavm_arity_i32 },

   { ngx_string("proxy_open_grpc_stream"),
     &ngx_proxy_wasm_hfuncs_nop,
     ngx_wavm_arity_i32x9,
     ngx_wavm_arity_i32 },
   /* 0.2.0 - 0.2.1 */
   { ngx_string("proxy_grpc_stream"),
     &ngx_proxy_wasm_hfuncs_nop,
     ngx_wavm_arity_i32x9,
     ngx_wavm_arity_i32 },

   { ngx_string("proxy_send_grpc_stream_message"),
     &ngx_proxy_wasm_hfuncs_nop,
     ngx_wavm_arity_i32x3,
     ngx_wavm_arity_i32 },
   /* 0.2.0 - 0.2.1 */
   { ngx_string("proxy_grpc_send"),
     &ngx_proxy_wasm_hfuncs_nop,
     ngx_wavm_arity_i32x3,
     ngx_wavm_arity_i32 },

   { ngx_string("proxy_cancel_grpc_call"),
     &ngx_proxy_wasm_hfuncs_nop,
     ngx_wavm_arity_i32,
     ngx_wavm_arity_i32 },
   /* 0.2.0 - 0.2.1 */
   { ngx_string("proxy_grpc_cancel"),
     &ngx_proxy_wasm_hfuncs_nop,
     ngx_wavm_arity_i32,
     ngx_wavm_arity_i32 },

   { ngx_string("proxy_close_grpc_call"),
     &ngx_proxy_wasm_hfuncs_nop,
     ngx_wavm_arity_i32,
     ngx_wavm_arity_i32 },
   /* 0.2.0 - 0.2.1 */
   { ngx_string("proxy_grpc_close"),
     &ngx_proxy_wasm_hfuncs_nop,
     ngx_wavm_arity_i32,
     ngx_wavm_arity_i32 },

   /* custom */

   { ngx_string("proxy_call_custom_function"),
     &ngx_proxy_wasm_hfuncs_nop,
     ngx_wavm_arity_i32x5,
     ngx_wavm_arity_i32 },

   /* 0.2.1 */

   { ngx_string("proxy_get_log_level"),
     &ngx_proxy_wasm_hfuncs_nop,
     ngx_wavm_arity_i32x2,
     ngx_wavm_arity_i32 },

   /* 0.1.0 - 0.2.1 */

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

   { ngx_string("proxy_clear_route_cache"),
     &ngx_proxy_wasm_hfuncs_nop,
     NULL,
     ngx_wavm_arity_i32 },

   { ngx_string("proxy_get_property"),
     &ngx_proxy_wasm_hfuncs_nop,
     ngx_wavm_arity_i32x4,
     ngx_wavm_arity_i32 },

   { ngx_string("proxy_set_property"),
     &ngx_proxy_wasm_hfuncs_nop,
     ngx_wavm_arity_i32x4,
     ngx_wavm_arity_i32 },

   ngx_wavm_hfunc_null
};


ngx_wavm_host_def_t  ngx_proxy_wasm_host = {
    ngx_string("ngx_proxy_wasm"),
    ngx_proxy_wasm_hfuncs,
};
