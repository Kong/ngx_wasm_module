#ifndef _NGX_PROXY_WASM_H_INCLUDED_
#define _NGX_PROXY_WASM_H_INCLUDED_


#include <ngx_event.h>
#include <ngx_wavm.h>


#define NGX_PROXY_WASM_ROOT_CTX_ID  0


typedef enum {
    NGX_PROXY_WASM_ERR_NONE = 0,
    NGX_PROXY_WASM_ERR_UNKNOWN_ABI = 1,
    NGX_PROXY_WASM_ERR_BAD_ABI = 2,
    NGX_PROXY_WASM_ERR_BAD_HOST_INTERFACE = 3,
    NGX_PROXY_WASM_ERR_BAD_MODULE_INTERFACE = 4,
    NGX_PROXY_WASM_ERR_UNKNOWN = 5
} ngx_proxy_wasm_err_t;


typedef enum {
    NGX_PROXY_WASM_0_1_0 = 0,
    NGX_PROXY_WASM_0_2_0 = 1,
    NGX_PROXY_WASM_0_2_1 = 2,
    NGX_PROXY_WASM_VNEXT = 3,
    NGX_PROXY_WASM_UNKNOWN = 4,
} ngx_proxy_wasm_abi_version_e;


typedef enum {
    NGX_PROXY_WASM_RESULT_OK = 0,
    NGX_PROXY_WASM_RESULT_NOT_FOUND = 1,
    NGX_PROXY_WASM_RESULT_BAD_ARGUMENT = 2,
    NGX_PROXY_WASM_RESULT_SERIALIZATION_FAILURE = 3,
    NGX_PROXY_WASM_RESULT_PARSE_FAILURE = 4,
    NGX_PROXY_WASM_RESULT_BAD_EXPRESSION = 5,
    NGX_PROXY_WASM_RESULT_INVALID_MEM_ACCESS = 6,
    NGX_PROXY_WASM_RESULT_EMPTY = 7,
    NGX_PROXY_WASM_RESULT_CAS_MISMATCH = 8,
    NGX_PROXY_WASM_RESULT_RESULT_MISMATCH = 9,
    NGX_PROXY_WASM_RESULT_INTERNAL_FAILURE = 10,
    NGX_PROXY_WASM_RESULT_BROKEN_CONNECTION = 11,
    NGX_PROXY_WASM_RESULT_NYI = 12,
} ngx_proxy_wasm_result_t;


typedef enum {
    NGX_PROXY_WASM_ACTION_CONTINUE = 0,
    NGX_PROXY_WASM_ACTION_PAUSE = 1,
    NGX_PROXY_WASM_ACTION_END_STREAM = 2,
    NGX_PROXY_WASM_ACTION_DONE = 3,
    NGX_PROXY_WASM_ACTION_WAIT = 4,
    NGX_PROXY_WASM_ACTION_WAIT_FULL = 5,
    NGX_PROXY_WASM_ACTION_CLOSE = 6,
} ngx_proxy_wasm_action_t;


typedef enum {
    NGX_PROXY_WASM_LOG_TRACE = 0,
    NGX_PROXY_WASM_LOG_DEBUG = 1,
    NGX_PROXY_WASM_LOG_INFO = 2,
    NGX_PROXY_WASM_LOG_WARNING = 3,
    NGX_PROXY_WASM_LOG_ERROR = 4,
    NGX_PROXY_WASM_LOG_CRITICAL = 5,
} ngx_proxy_wasm_log_level_t;


typedef enum {
    NGX_PROXY_WASM_CONTEXT_HTTP = 0,
    NGX_PROXY_WASM_CONTEXT_STREAM = 1,
} ngx_proxy_wasm_context_type_t;


typedef enum {
    NGX_PROXY_WASM_STREAM_TYPE_DOWNSTREAM = 1,
    NGX_PROXY_WASM_STREAM_TYPE_UPSTREAM = 2,
    NGX_PROXY_WASM_STREAM_TYPE_HTTP_REQUEST = 3,
    NGX_PROXY_WASM_STREAM_TYPE_HTTP_RESPONSE = 4,
} ngx_proxy_wasm_stream_type_t;


typedef enum {
    NGX_PROXY_WASM_CLOSE_SOURCE_LOCAL = 1,
    NGX_PROXY_WASM_CLOSE_SOURCE_REMOTE = 2,
} ngx_proxy_wasm_close_source_type_t;


typedef enum {
    NGX_PROXY_WASM_BUFFER_HTTP_REQUEST_BODY = 0,
    NGX_PROXY_WASM_BUFFER_HTTP_RESPONSE_BODY = 1,
    NGX_PROXY_WASM_BUFFER_DOWNSTREAM_DATA = 2,
    NGX_PROXY_WASM_BUFFER_UPSTREAM_DATA = 3,
    NGX_PROXY_WASM_BUFFER_HTTP_CALL_RESPONSE_BODY = 4,
    NGX_PROXY_WASM_BUFFER_GRPC_RECEIVE_BUFFER = 5,
    NGX_PROXY_WASM_BUFFER_VM_CONFIGURATION = 6,
    NGX_PROXY_WASM_BUFFER_PLUGIN_CONFIGURATION = 7,
    NGX_PROXY_WASM_BUFFER_CALL_DATA = 8,
} ngx_proxy_wasm_buffer_type_t;


typedef enum {
    NGX_PROXY_WASM_MAP_HTTP_REQUEST_HEADERS = 0,
    NGX_PROXY_WASM_MAP_HTTP_REQUEST_TRAILERS = 1,
    NGX_PROXY_WASM_MAP_HTTP_RESPONSE_HEADERS = 2,
    NGX_PROXY_WASM_MAP_HTTP_RESPONSE_TRAILERS = 3,
    NGX_PROXY_WASM_MAP_GRPC_RECEIVE_INITIAL_METADATA = 4,
    NGX_PROXY_WASM_MAP_GRPC_RECEIVE_TRAILING_METADATA = 5,
    NGX_PROXY_WASM_MAP_HTTP_CALL_RESPONSE_HEADERS = 6,
    NGX_PROXY_WASM_MAP_HTTP_CALL_RESPONSE_TRAILERS = 7,
} ngx_proxy_wasm_map_type_t;


typedef enum {
    NGX_PROXY_WASM_METRIC_COUNTER = 0,
    NGX_PROXY_WASM_METRIC_GAUGE = 1,
    NGX_PROXY_WASM_METRIC_HISTOGRAM = 2,
} ngx_proxy_wasm_metric_type_t;


typedef struct ngx_proxy_wasm_s  ngx_proxy_wasm_t;

typedef ngx_int_t (*ngx_proxy_wasm_resume_pt)(ngx_proxy_wasm_t *pwm,
    ngx_wasm_phase_t *phase, ngx_wavm_ctx_t *wvctx);
typedef ngx_uint_t (*ngx_proxy_wasm_ctxid_pt)(ngx_proxy_wasm_t *pwm);
typedef ngx_uint_t (*ngx_proxy_wasm_ecode_pt)(ngx_proxy_wasm_t *pwm, ngx_uint_t ecode);

struct ngx_proxy_wasm_s {

    ngx_event_t                        yield_ev;
    ngx_str_t                          config;
    ngx_proxy_wasm_resume_pt           resume_;
    ngx_proxy_wasm_ctxid_pt            ctxid_;
    ngx_proxy_wasm_ecode_pt            ecode_;
    ngx_pool_t                        *pool;
    ngx_log_t                         *log;
    ngx_wavm_module_t                 *module;
    ngx_wavm_linked_module_t          *lmodule;

    /* dyn config */

    ngx_uint_t                         max_pairs;

    /* control flow */

    ngx_uint_t                         ecode;
    ngx_uint_t                         rctxid;
    ngx_uint_t                         tick_period;

    /**
     * SDK
     * - targets: vNEXT-reviewed (proxy-wasm/spec#1)
     *            0.1.0 (rust-sdk)
     *            0.2.1 (cpp-sdk)
     * - tested: 0.1.0
     */

    ngx_wavm_ctx_t                     wvctx;
    ngx_wavm_instance_t               *instance;
    ngx_proxy_wasm_abi_version_e       abi_version;

    ngx_wavm_funcref_t                *proxy_on_memory_allocate;

    /* context */

    ngx_wavm_funcref_t                *proxy_on_context_create;
    ngx_wavm_funcref_t                *proxy_on_context_finalize;
    ngx_wavm_funcref_t                *proxy_on_done; // legacy: 0.1.0 - 0.2.1
    ngx_wavm_funcref_t                *proxy_on_log; // legacy: 0.1.0 - 0.2.1

    /* configuration */

    ngx_wavm_funcref_t                *proxy_on_vm_start;
    ngx_wavm_funcref_t                *proxy_on_plugin_start;

    /* stream */

    ngx_wavm_funcref_t                *proxy_on_new_connection;
    ngx_wavm_funcref_t                *proxy_on_downstream_data;
    ngx_wavm_funcref_t                *proxy_on_upstream_data;
    ngx_wavm_funcref_t                *proxy_on_downstream_close;
    ngx_wavm_funcref_t                *proxy_on_upstream_close;

    /* http */

    ngx_wavm_funcref_t                *proxy_on_http_request_headers;
    ngx_wavm_funcref_t                *proxy_on_http_request_body;
    ngx_wavm_funcref_t                *proxy_on_http_request_trailers;
    ngx_wavm_funcref_t                *proxy_on_http_request_metadata;
    ngx_wavm_funcref_t                *proxy_on_http_response_headers;
    ngx_wavm_funcref_t                *proxy_on_http_response_body;
    ngx_wavm_funcref_t                *proxy_on_http_response_trailers;
    ngx_wavm_funcref_t                *proxy_on_http_response_metadata;

    /* shared queue */

    ngx_wavm_funcref_t                *proxy_on_queue_ready;

    /* timers */

    ngx_wavm_funcref_t                *proxy_create_timer;
    ngx_wavm_funcref_t                *proxy_delete_timer;
    ngx_wavm_funcref_t                *proxy_on_timer_ready;

    /* http callouts */

    ngx_wavm_funcref_t                *proxy_on_http_call_response;

    /* grpc callouts */

    ngx_wavm_funcref_t                *proxy_on_grpc_call_response_header_metadata;
    ngx_wavm_funcref_t                *proxy_on_grpc_call_response_message;
    ngx_wavm_funcref_t                *proxy_on_grpc_call_response_trailer_metadata;
    ngx_wavm_funcref_t                *proxy_on_grpc_call_close;

    /* custom extensions */

    ngx_wavm_funcref_t                *proxy_on_custom_callback;

    /* control flow: flags */

    unsigned                           trap:1;
};


/* ngx_proxy_wasm_t */
ngx_int_t ngx_proxy_wasm_init(ngx_proxy_wasm_t *pwmodule);
void ngx_proxy_wasm_destroy(ngx_proxy_wasm_t *pwmodule);

/* ngx_proxy_wasm_t utils */
ngx_wavm_ptr_t ngx_proxy_wasm_alloc(ngx_proxy_wasm_t *pwm, size_t size);
unsigned ngx_proxy_wasm_marshal(ngx_proxy_wasm_t *pwm, ngx_list_t *list,
    ngx_wavm_ptr_t *out, size_t *len, u_char *truncated);

/* phases */
ngx_int_t ngx_proxy_wasm_resume(ngx_proxy_wasm_t *pwm, ngx_wasm_phase_t *phase,
     ngx_wavm_ctx_t *wvctx);
ngx_int_t ngx_proxy_wasm_on_log(ngx_proxy_wasm_t *pwm);

/* utils */
ngx_uint_t ngx_proxy_wasm_pairs_count(ngx_list_t *list);
size_t ngx_proxy_wasm_pairs_size(ngx_list_t *list, ngx_uint_t max);
void ngx_proxy_wasm_pairs_marshal(ngx_list_t *list, u_char *buf, ngx_uint_t max, u_char *truncated);
ngx_array_t *ngx_proxy_wasm_pairs_unmarshal(ngx_pool_t *pool, u_char *buf,
    size_t len);
ngx_str_t *ngx_proxy_wasm_get_map_value(ngx_list_t *map, u_char *key,
    size_t key_len);
void ngx_proxy_wasm_tick_handler(ngx_event_t *ev);
void ngx_proxy_wasm_log_error(ngx_uint_t level, ngx_log_t *log,
    ngx_proxy_wasm_err_t err, const char *fmt, ...);


static ngx_inline ngx_int_t
ngx_proxy_wasm_result_ok(wasm_val_t rets[])
{
    rets[0] = (wasm_val_t) WASM_I32_VAL(NGX_PROXY_WASM_RESULT_OK);
    return NGX_WAVM_OK;
}

static ngx_inline ngx_int_t
ngx_proxy_wasm_result_err(wasm_val_t rets[])
{
    rets[0] = (wasm_val_t) WASM_I32_VAL(NGX_PROXY_WASM_RESULT_INTERNAL_FAILURE);
    return NGX_WAVM_OK;
}

static ngx_inline ngx_int_t
ngx_proxy_wasm_result_trap(ngx_proxy_wasm_t *pwm, char *trapmsg,
    wasm_val_t rets[])
{
    ngx_wavm_instance_trap_printf(pwm->instance, trapmsg);
    return ngx_proxy_wasm_result_ok(rets);
}

static ngx_inline ngx_int_t
ngx_proxy_wasm_result_invalid_mem(wasm_val_t rets[])
{
    rets[0] = (wasm_val_t)
                  WASM_I32_VAL(NGX_PROXY_WASM_RESULT_INVALID_MEM_ACCESS);
    return NGX_WAVM_OK;
}

static ngx_inline ngx_int_t
ngx_proxy_wasm_result_badarg(wasm_val_t rets[])
{
    rets[0] = (wasm_val_t) WASM_I32_VAL(NGX_PROXY_WASM_RESULT_BAD_ARGUMENT);
    return NGX_WAVM_OK;
}

static ngx_inline ngx_int_t
ngx_proxy_wasm_result_notfound(wasm_val_t rets[])
{
    rets[0] = (wasm_val_t) WASM_I32_VAL(NGX_PROXY_WASM_RESULT_NOT_FOUND);
    return NGX_WAVM_OK;
}


extern ngx_wavm_host_def_t  ngx_proxy_wasm_host;


#endif /* _NGX_PROXY_WASM_H_INCLUDED_ */
