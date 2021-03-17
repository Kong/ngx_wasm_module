#ifndef _NGX_PROXY_WASM_H_INCLUDED_
#define _NGX_PROXY_WASM_H_INCLUDED_


#include <ngx_wavm.h>


#define NGX_PROXY_WASM_ROOT_CTX_ID  0


typedef enum {
    NGX_PROXY_WASM_ERR_UNKNOWN_ABI = 1,
    NGX_PROXY_WASM_ERR_BAD_ABI,
    NGX_PROXY_WASM_ERR_BAD_HOST_INTERFACE,
    NGX_PROXY_WASM_ERR_UNKNOWN
} ngx_proxy_wasm_err_t;


typedef enum {
    NGX_PROXY_WASM_UNKNOWN,
    NGX_PROXY_WASM_0_1_0,
    NGX_PROXY_WASM_0_2_0,
    NGX_PROXY_WASM_0_2_1,
} ngx_proxy_wasm_abi_version;


typedef enum {
    /*
    NGX_PROXY_WASM_RESULT_OK = 0,
    NGX_PROXY_WASM_RESULT_EMPTY = 1,
    NGX_PROXY_WASM_RESULT_NOT_FOUND = 2,
    NGX_PROXY_WASM_RESULT_NOT_ALLOWED = 3,
    NGX_PROXY_WASM_RESULT_BAD_ARGUMENT = 4,
    NGX_PROXY_WASM_RESULT_BAD_MEM_ACCESS = 5,
    NGX_PROXY_WASM_RESULT_BAD_OPERATION = 6,
    NGX_PROXY_WASM_RESULT_CAS_MISMATCH = 7,
    */

    NGX_PROXY_WASM_RESULT_OK = 0,
    NGX_PROXY_WASM_RESULT_NOT_FOUND = 1,
    NGX_PROXY_WASM_RESULT_BAD_ARGUMENT = 2,
    NGX_PROXY_WASM_RESULT_EMPTY = 7,
    NGX_PROXY_WASM_RESULT_CAS_MISMATCH = 8,
    NGX_PROXY_WASM_RESULT_INTERNAL_FAILURE = 10,
} ngx_proxy_wasm_result;


typedef enum {
    /*
    NGX_PROXY_WASM_ACTION_CONTINUE = 1,
    NGX_PROXY_WASM_ACTION_END_STREAM = 2,
    NGX_PROXY_WASM_ACTION_DONE = 3,
    NGX_PROXY_WASM_ACTION_PAUSE = 4,
    NGX_PROXY_WASM_ACTION_WAIT = 5,
    NGX_PROXY_WASM_ACTION_WAIT_FULL = 6,
    NGX_PROXY_WASM_ACTION_CLOSE = 7,
    */

    NGX_PROXY_WASM_ACTION_CONTINUE = 0,
    NGX_PROXY_WASM_ACTION_PAUSE = 1,
} ngx_proxy_wasm_action;


typedef enum {
    NGX_PROXY_WASM_LOG_TRACE = 0,
    NGX_PROXY_WASM_LOG_DEBUG = 1,
    NGX_PROXY_WASM_LOG_INFO = 2,
    NGX_PROXY_WASM_LOG_WARNING = 3,
    NGX_PROXY_WASM_LOG_ERROR = 4,

    NGX_PROXY_WASM_LOG_CRITICAL = 5,
} ngx_proxy_wasm_log_level;


typedef enum {
    /*
    NGX_PROXY_WASM_CONTEXT_VM = 1,
    NGX_PROXY_WASM_CONTEXT_PLUGIN = 2,
    */

    NGX_PROXY_WASM_CONTEXT_HTTP = 0,
    NGX_PROXY_WASM_CONTEXT_STREAM = 1,
} ngx_proxy_wasm_context;


typedef enum {
    NGX_PROXY_WASM_STREAM_TYPE_DOWNSTREAM = 1,
    NGX_PROXY_WASM_STREAM_TYPE_UPSTREAM = 2,
    NGX_PROXY_WASM_STREAM_TYPE_HTTP_REQUEST = 3,
    NGX_PROXY_WASM_STREAM_TYPE_HTTP_RESPONSE = 4,
} ngx_proxy_wasm_stream_type;


typedef enum {
    NGX_PROXY_WASM_CLOSE_SOURCE_LOCAL = 1,
    NGX_PROXY_WASM_CLOSE_SOURCE_REMOTE = 2,

    NGX_PROXY_WASM_CLOSE_SOURCE_UNKNOWN = 0,
} ngx_proxy_wasm_close_source_type;


typedef enum {
    /*
    NGX_PROXY_WASM_BUFFER_VM_CONFIGURATION = 1,
    NGX_PROXY_WASM_BUFFER_PLUGIN_CONFIGURATION = 2,
    */

    NGX_PROXY_WASM_BUFFER_HTTP_REQUEST_BODY = 0,
    NGX_PROXY_WASM_BUFFER_HTTP_RESPONSE_BODY = 1,
    NGX_PROXY_WASM_BUFFER_DOWNSTREAM_DATA = 2,
    NGX_PROXY_WASM_BUFFER_UPSTREAM_DATA = 3,
    NGX_PROXY_WASM_BUFFER_HTTP_CALL_RESPONSE_BODY = 4,
} ngx_proxy_wasm_buffer_type;


typedef enum {
    /*
    NGX_PROXY_WASM_MAP_HTTP_REQUEST_METADATA = 3,
    NGX_PROXY_WASM_MAP_HTTP_RESPONSE_METADATA = 6,
    NGX_PROXY_WASM_MAP_HTTP_CALL_RESPONSE_METADATA = 9,
    */

    NGX_PROXY_WASM_MAP_HTTP_REQUEST_HEADERS = 0,
    NGX_PROXY_WASM_MAP_HTTP_REQUEST_TRAILERS = 1,
    NGX_PROXY_WASM_MAP_HTTP_RESPONSE_HEADERS = 2,
    NGX_PROXY_WASM_MAP_HTTP_RESPONSE_TRAILERS = 3,
    NGX_PROXY_WASM_MAP_HTTP_CALL_RESPONSE_HEADERS = 6,
    NGX_PROXY_WASM_MAP_HTTP_CALL_RESPONSE_TRAILERS = 7,
} ngx_proxy_wasm_map_type;


typedef enum {
    /*
    NGX_PROXY_WASM_METRIC_COUNTER = 1,
    NGX_PROXY_WASM_METRIC_GAUGE = 2,
    NGX_PROXY_WASM_METRIC_HISTOGRAM = 3
    */

    NGX_PROXY_WASM_METRIC_COUNTER = 0,
    NGX_PROXY_WASM_METRIC_GAUGE = 1,
    NGX_PROXY_WASM_METRIC_HISTOGRAM = 2,
} ngx_proxy_wasm_metric_type;


typedef struct ngx_proxy_wasm_module_s  ngx_proxy_wasm_module_t;

struct ngx_proxy_wasm_module_s {

    ngx_pool_t                    *pool;
    ngx_log_t                     *log;
    ngx_wavm_module_t             *module;
    ngx_wavm_linked_module_t      *lmodule;
    ngx_wavm_instance_t           *instance;
    ngx_wavm_ctx_t                 wv_ctx;
    ngx_proxy_wasm_abi_version     abi_version;

    /* control flow */

    ngx_uint_t                     ecode;
    ngx_uint_t                     ctxid;
    uint32_t                       tick_period;

    /* integration */

    ngx_wavm_funcref_t            *proxy_on_memory_allocate;
    ngx_wavm_funcref_t            *proxy_on_tick;

    /* context */

    ngx_wavm_funcref_t            *proxy_on_context_create;
    ngx_wavm_funcref_t            *proxy_on_log; /* 0.1.0 */
    ngx_wavm_funcref_t            *proxy_on_context_finalize;

    /* configuration */

    ngx_wavm_funcref_t            *proxy_on_vm_start;
    ngx_wavm_funcref_t            *proxy_on_plugin_start;

    /* stream */

    ngx_wavm_funcref_t            *proxy_on_new_connection;
    ngx_wavm_funcref_t            *proxy_on_downstream_data;
    ngx_wavm_funcref_t            *proxy_on_downstream_close;
    ngx_wavm_funcref_t            *proxy_on_upstream_data;
    ngx_wavm_funcref_t            *proxy_on_upstream_close;

    /* http */

    ngx_wavm_funcref_t            *proxy_on_request_headers;
    ngx_wavm_funcref_t            *proxy_on_request_body;
    ngx_wavm_funcref_t            *proxy_on_request_trailers;
    ngx_wavm_funcref_t            *proxy_on_request_metadata;
    ngx_wavm_funcref_t            *proxy_on_response_headers;
    ngx_wavm_funcref_t            *proxy_on_response_body;
    ngx_wavm_funcref_t            *proxy_on_response_trailers;
    ngx_wavm_funcref_t            *proxy_on_response_metadata;

    /* shared queue */

    ngx_wavm_funcref_t            *proxy_on_queue_ready;

    /* http callouts */

    ngx_wavm_funcref_t            *proxy_on_http_call_response;

    /* grpc callouts */

    ngx_wavm_funcref_t            *proxy_on_grpc_call_response_header_metadata;
    ngx_wavm_funcref_t            *proxy_on_grpc_call_response_message;
    ngx_wavm_funcref_t            *proxy_on_grpc_call_response_trailer_metadata;
    ngx_wavm_funcref_t            *proxy_on_grpc_call_close;

    /* custom */

    ngx_wavm_funcref_t            *proxy_on_custom_callback;

};


ngx_int_t ngx_proxy_wasm_module_init(ngx_proxy_wasm_module_t *pwmodule);
ngx_int_t ngx_proxy_wasm_module_resume(ngx_proxy_wasm_module_t *pwm,
    ngx_wasm_phase_t *phase, ngx_wavm_ctx_t *ctx);
void ngx_proxy_wasm_module_destroy(ngx_proxy_wasm_module_t *pwmodule);
void ngx_proxy_wasm_log_error(ngx_uint_t level, ngx_log_t *log,
    ngx_proxy_wasm_err_t err, const char *fmt, ...);


uint32_t ngx_proxy_wasm_alloc(ngx_proxy_wasm_module_t *pwm, size_t size);
ngx_uint_t ngx_proxy_wasm_pairs_count(ngx_list_t *list);
size_t ngx_proxy_wasm_pairs_size(ngx_list_t *list);
void ngx_proxy_wasm_pairs_marshal(ngx_list_t *list, u_char *buf);
ngx_str_t *ngx_proxy_wasm_get_map_value(ngx_list_t *map, u_char *key,
    size_t key_len);
void ngx_proxy_wasm_tick_handler(ngx_event_t *ev);


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
