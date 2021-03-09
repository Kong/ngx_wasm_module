#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_http_wasm.h>
#include <ngx_proxy_wasm.h>
#include <ngx_event.h>
#include <ngx_wavm.h>


#define ngx_proxy_wasm_get_pwm(instance)                                     \
    ((ngx_proxy_wasm_module_t *)                                             \
     ((u_char *) (instance)->ctx - offsetof(ngx_proxy_wasm_module_t, wv_ctx)))


static ngx_int_t
ngx_proxy_wasm_hfuncs_on_memory_allocate(ngx_wavm_instance_t *instance,
    wasm_val_t args[], wasm_val_t rets[])
{
    uint32_t   size = args[0].of.i32;
    u_char    *p;

    p = ngx_alloc(size, instance->log);
    if (p == NULL) {
        ngx_wasm_vec_set_i32(rets, 0, (uintptr_t) 0);
        return NGX_WAVM_OK;
    }

    ngx_wasm_vec_set_i32(rets, 0, (uintptr_t) p);

    return NGX_WAVM_OK;
}


static ngx_int_t
ngx_proxy_wasm_hfuncs_proxy_log(ngx_wavm_instance_t *instance,
    wasm_val_t args[], wasm_val_t rets[])
{
    ngx_uint_t   level;
    uint32_t     log_level, msg_size, msg_data;

    log_level = args[0].of.i32;
    msg_data = args[1].of.i32;
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
        ngx_wasm_log_error(NGX_LOG_ALERT, instance->log, 0,
                           "NYI: unknown log level \"%d\"", log_level);

        return ngx_proxy_wasm_result_badarg(rets);

    }

    ngx_wasm_log_error(level, instance->log, 0, "%*s",
                       msg_size, instance->mem_offset + msg_data);

    return ngx_proxy_wasm_result_ok(rets);
}


static ngx_int_t
ngx_proxy_wasm_hfuncs_set_tick_period(ngx_wavm_instance_t *instance,
    wasm_val_t args[], wasm_val_t rets[])
{
    ngx_event_t              *ev;
    ngx_proxy_wasm_module_t  *pwm;
    uint32_t                  period = args[0].of.i32;

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
    ngx_time_t  *tp;
    uint64_t    *ret_time = (uint64_t *)
                            (instance->mem_offset + args[0].of.i32);

    ngx_time_update();

    tp = ngx_timeofday();

    *ret_time = (tp->sec * 1000 + tp->msec) * 1e6;

    return ngx_proxy_wasm_result_ok(rets);
}


static ngx_int_t
ngx_proxy_wasm_hfuncs_set_buffer_bytes(ngx_wavm_instance_t *instance,
    wasm_val_t args[], wasm_val_t rets[])
{
    uint32_t   buf_type;//, buf_start, max_size;
    buf_type = args[0].of.i32;

    if (buf_type > NGX_PROXY_WASM_BUFFER_HTTP_CALL_RESPONSE_BODY) {
        return NGX_WAVM_BAD_USAGE;
    }

    switch (buf_type) {

    default:
        ngx_wasm_log_error(NGX_LOG_ALERT, instance->log, 0, "NYI");
        break;

    }

    return ngx_proxy_wasm_result_ok(rets);
}


static ngx_int_t
ngx_proxy_wasm_hfuncs_get_buffer_bytes(ngx_wavm_instance_t *instance,
    wasm_val_t args[], wasm_val_t rets[])
{
    uint32_t   buf_type, buf_start, max_size;
    //uint32_t  *rbuf, *rsize;

    buf_type = args[0].of.i32;
    buf_start = args[1].of.i32;
    max_size = args[2].of.i32;
    //rbuf = (uint32_t *) (instance->mem_offset + args[3].of.i32);
    //rsize = (uint32_t *) (instance->mem_offset + args[4].of.i32);

    if (buf_type > NGX_PROXY_WASM_BUFFER_HTTP_CALL_RESPONSE_BODY) {
        return NGX_WAVM_BAD_USAGE;
    }

    switch (buf_type) {

    default:
        ngx_wasm_log_error(NGX_LOG_ALERT, instance->log, 0, "NYI");
        break;

    }

    if (buf_start > buf_start + max_size) {
        return NGX_WAVM_BAD_USAGE;
    }

    return ngx_proxy_wasm_result_ok(rets);
}


static uint32_t
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


static ngx_int_t
ngx_proxy_wasm_hfuncs_get_header_map_pairs(ngx_wavm_instance_t *instance,
    wasm_val_t args[], wasm_val_t rets[])
{
    size_t                    size;
    uint64_t                 *rbuf, *rsize, map_type, p;
    ngx_http_wasm_req_ctx_t  *rctx = instance->ctx->data;
    ngx_http_request_t       *r = rctx->r;
    ngx_proxy_wasm_module_t  *pwm;

    map_type = args[0].of.i32;
    rbuf = (uint64_t *) (instance->mem_offset + args[1].of.i32);
    rsize = (uint64_t *) (instance->mem_offset + args[2].of.i32);

    if (map_type > NGX_PROXY_WASM_MAP_HTTP_CALL_RESPONSE_TRAILERS) {
        return ngx_proxy_wasm_result_badarg(rets);
    }

    switch (map_type) {

    case NGX_PROXY_WASM_MAP_HTTP_REQUEST_HEADERS:
        size = ngx_proxy_wasm_pairs_size(&r->headers_in.headers);

        pwm = ngx_proxy_wasm_get_pwm(instance);

        p = ngx_proxy_wasm_alloc(pwm, size);
        if (p == 0) {
            return ngx_proxy_wasm_result_err(rets);
        }

        ngx_proxy_wasm_marshal_pairs(&r->headers_in.headers,
                                     (instance->mem_offset + p));
        *rbuf = p;
        *rsize = size;
        break;

    default:
        ngx_wasm_log_error(NGX_LOG_ALERT, instance->log, 0, "NYI");
        break;

    }

    return ngx_proxy_wasm_result_ok(rets);
}


static ngx_int_t
ngx_proxy_wasm_hfuncs_nop(ngx_wavm_instance_t *instance,
    wasm_val_t args[], wasm_val_t rets[])
{
    ngx_wasm_log_error(NGX_LOG_ALERT, instance->log, 0,
                       "NYI: proxy_wasm hfunc");

    return NGX_WAVM_BUSY; /* NYI */
}


static ngx_wavm_host_func_def_t  ngx_proxy_wasm_hfuncs[] = {

   /* 0.1.0 */

   { ngx_string("proxy_on_memory_allocate"),
     &ngx_proxy_wasm_hfuncs_on_memory_allocate,
     ngx_wavm_arity_i32,
     ngx_wavm_arity_i32 },

   { ngx_string("proxy_set_tick_period_milliseconds"),
     &ngx_proxy_wasm_hfuncs_set_tick_period,
     ngx_wavm_arity_i32,
     ngx_wavm_arity_i32 },

   { ngx_string("proxy_get_current_time_nanoseconds"),
     &ngx_proxy_wasm_hfuncs_get_current_time,
     ngx_wavm_arity_i32,
     ngx_wavm_arity_i32 },

   { ngx_string("proxy_get_configuration"),
     &ngx_proxy_wasm_hfuncs_nop,
     ngx_wavm_arity_i32x2,
     ngx_wavm_arity_i32 },

   { ngx_string("proxy_get_buffer_bytes"),
     &ngx_proxy_wasm_hfuncs_get_buffer_bytes,
     ngx_wavm_arity_i32x5,
     ngx_wavm_arity_i32 },

   { ngx_string("proxy_set_buffer_bytes"),
     &ngx_proxy_wasm_hfuncs_set_buffer_bytes,
     ngx_wavm_arity_i32x5,
     ngx_wavm_arity_i32 },

   { ngx_string("proxy_get_header_map_pairs"),
     &ngx_proxy_wasm_hfuncs_get_header_map_pairs,
     ngx_wavm_arity_i32x3,
     ngx_wavm_arity_i32 },

   { ngx_string("proxy_set_header_map_pairs"),
     &ngx_proxy_wasm_hfuncs_nop,
     ngx_wavm_arity_i32x3,
     ngx_wavm_arity_i32 },

   { ngx_string("proxy_replace_header_map_value"),
     &ngx_proxy_wasm_hfuncs_nop,
     ngx_wavm_arity_i32x5,
     ngx_wavm_arity_i32 },

   { ngx_string("proxy_remove_header_map_value"),
     &ngx_proxy_wasm_hfuncs_nop,
     ngx_wavm_arity_i32x3,
     ngx_wavm_arity_i32 },

   { ngx_string("proxy_add_header_map_value"),
     &ngx_proxy_wasm_hfuncs_nop,
     ngx_wavm_arity_i32x5,
     ngx_wavm_arity_i32 },

   { ngx_string("proxy_continue_request"),
     &ngx_proxy_wasm_hfuncs_nop,
     NULL,
     ngx_wavm_arity_i32 },

   { ngx_string("proxy_continue_response"),
     &ngx_proxy_wasm_hfuncs_nop,
     NULL,
     ngx_wavm_arity_i32 },

   { ngx_string("proxy_send_local_response"),
     &ngx_proxy_wasm_hfuncs_nop,
     ngx_wavm_arity_i32x8,
     ngx_wavm_arity_i32 },

   { ngx_string("proxy_clear_route_cache"),
     &ngx_proxy_wasm_hfuncs_nop,
     NULL,
     ngx_wavm_arity_i32 },

   { ngx_string("proxy_get_property"),
     &ngx_proxy_wasm_hfuncs_nop,
     ngx_wavm_arity_i32x4,
     ngx_wavm_arity_i32 },

   { ngx_string("proxy_get_header_map_value"),
     &ngx_proxy_wasm_hfuncs_nop,
     ngx_wavm_arity_i32x5,
     ngx_wavm_arity_i32 },

   { ngx_string("proxy_set_property"),
     &ngx_proxy_wasm_hfuncs_nop,
     ngx_wavm_arity_i32x4,
     ngx_wavm_arity_i32 },

   { ngx_string("proxy_get_shared_data"),
     &ngx_proxy_wasm_hfuncs_nop,
     ngx_wavm_arity_i32x5,
     ngx_wavm_arity_i32 },

   { ngx_string("proxy_set_shared_data"),
     &ngx_proxy_wasm_hfuncs_nop,
     ngx_wavm_arity_i32x5,
     ngx_wavm_arity_i32 },

   { ngx_string("proxy_register_shared_queue"),
     &ngx_proxy_wasm_hfuncs_nop,
     ngx_wavm_arity_i32x3,
     ngx_wavm_arity_i32 },

   { ngx_string("proxy_resolve_shared_queue"),
     &ngx_proxy_wasm_hfuncs_nop,
     ngx_wavm_arity_i32x5,
     ngx_wavm_arity_i32 },

   { ngx_string("proxy_dequeue_shared_queue"),
     &ngx_proxy_wasm_hfuncs_nop,
     ngx_wavm_arity_i32x3,
     ngx_wavm_arity_i32 },

   { ngx_string("proxy_enqueue_shared_queue"),
     &ngx_proxy_wasm_hfuncs_nop,
     ngx_wavm_arity_i32x3,
     ngx_wavm_arity_i32 },

   /* integration */

   { ngx_string("proxy_abi_version_0_1_0"),
     &ngx_proxy_wasm_hfuncs_nop,
     NULL,
     NULL },

   { ngx_string("proxy_log"),
     &ngx_proxy_wasm_hfuncs_proxy_log,
     ngx_wavm_arity_i32x3,
     ngx_wavm_arity_i32 },

   { ngx_string("proxy_http_call"),
     &ngx_proxy_wasm_hfuncs_nop,
     ngx_wavm_arity_i32x10,
     ngx_wavm_arity_i32 },

   { ngx_string("proxy_done"),
     &ngx_proxy_wasm_hfuncs_nop,
     NULL,
     ngx_wavm_arity_i32 },

   /* context lifecycle */

   { ngx_string("proxy_set_effective_context"),
     &ngx_proxy_wasm_hfuncs_nop,
     ngx_wavm_arity_i32,
     ngx_wavm_arity_i32 },

   { ngx_string("proxy_context_finalize"),
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

   { ngx_string("proxy_send_http_response"),
     &ngx_proxy_wasm_hfuncs_nop,
     ngx_wavm_arity_i32x8,
     ngx_wavm_arity_i32 },

   { ngx_string("proxy_resume_http_stream"),
     &ngx_proxy_wasm_hfuncs_nop,
     ngx_wavm_arity_i32,
     ngx_wavm_arity_i32 },

   { ngx_string("proxy_close_http_stream"),
     &ngx_proxy_wasm_hfuncs_nop,
     ngx_wavm_arity_i32,
     ngx_wavm_arity_i32 },

   { ngx_string("proxy_close_http_stream"),
     &ngx_proxy_wasm_hfuncs_nop,
     ngx_wavm_arity_i32,
     ngx_wavm_arity_i32 },

   /* buffers */

   { ngx_string("proxy_get_buffer_bytes"),
     &ngx_proxy_wasm_hfuncs_get_buffer_bytes,
     ngx_wavm_arity_i32x5,
     ngx_wavm_arity_i32 },

   { ngx_string("proxy_set_buffer_bytes"),
     &ngx_proxy_wasm_hfuncs_set_buffer_bytes,
     ngx_wavm_arity_i32x5,
     ngx_wavm_arity_i32 },

   { ngx_string("proxy_get_map_values"),
     &ngx_proxy_wasm_hfuncs_nop,
     ngx_wavm_arity_i32x5,
     ngx_wavm_arity_i32 },

   { ngx_string("proxy_set_map_values"),
     &ngx_proxy_wasm_hfuncs_nop,
     ngx_wavm_arity_i32x5,
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

   { ngx_string("proxy_set_shared_kvstore_key_values"),
     &ngx_proxy_wasm_hfuncs_nop,
     ngx_wavm_arity_i32x6,
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

   { ngx_string("proxy_dequeue_shared_queue_item"),
     &ngx_proxy_wasm_hfuncs_nop,
     ngx_wavm_arity_i32x4,
     ngx_wavm_arity_i32 },

   { ngx_string("proxy_enqueue_shared_queue_item"),
     &ngx_proxy_wasm_hfuncs_nop,
     ngx_wavm_arity_i32x4,
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

   { ngx_string("proxy_get_metric_value"),
     &ngx_proxy_wasm_hfuncs_nop,
     ngx_wavm_arity_i32x2,
     ngx_wavm_arity_i32 },

   { ngx_string("proxy_set_metric_value"),
     &ngx_proxy_wasm_hfuncs_nop,
     ngx_wavm_arity_i32_i64,
     ngx_wavm_arity_i32 },

   { ngx_string("proxy_increment_metric_value"),
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

   /* grpc callouts */

   { ngx_string("proxy_dispatch_grpc_call"),
     &ngx_proxy_wasm_hfuncs_nop,
     ngx_wavm_arity_i32x12,
     ngx_wavm_arity_i32 },

   { ngx_string("proxy_open_grpc_stream"),
     &ngx_proxy_wasm_hfuncs_nop,
     ngx_wavm_arity_i32x9,
     ngx_wavm_arity_i32 },

   { ngx_string("proxy_send_grpc_stream_message"),
     &ngx_proxy_wasm_hfuncs_nop,
     ngx_wavm_arity_i32x3,
     ngx_wavm_arity_i32 },

   { ngx_string("proxy_cancel_grpc_call"),
     &ngx_proxy_wasm_hfuncs_nop,
     ngx_wavm_arity_i32,
     ngx_wavm_arity_i32 },

   { ngx_string("proxy_close_grpc_call"),
     &ngx_proxy_wasm_hfuncs_nop,
     ngx_wavm_arity_i32,
     ngx_wavm_arity_i32 },

   /* custom */

   { ngx_string("proxy_call_custom_function"),
     &ngx_proxy_wasm_hfuncs_nop,
     ngx_wavm_arity_i32x5,
     ngx_wavm_arity_i32 },

   ngx_wavm_hfunc_null
};


ngx_wavm_host_def_t  ngx_proxy_wasm_host = {
    ngx_string("ngx_proxy_wasm"),
    ngx_proxy_wasm_hfuncs,
};
