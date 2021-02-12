#ifndef _NGX_PROXY_WASM_H_INCLUDED_
#define _NGX_PROXY_WASM_H_INCLUDED_


#include <ngx_wasm.h>


#define ngx_proxy_wasm_result_ok()                                           \
    (wasm_val_t) WASM_I32_VAL(NGX_PROXY_WASM_RESULT_OK)
#define ngx_proxy_wasm_result_bad_arg()                                      \
    (wasm_val_t) WASM_I32_VAL(NGX_PROXY_WASM_RESULT_BAD_ARGUMENT)


typedef enum {
    NGX_PROXY_WASM_0_1_0,
    NGX_PROXY_WASM_0_2_0,
    NGX_PROXY_WASM_0_2_1,
} ngx_proxy_wasm_abi_version;


typedef enum {
    NGX_PROXY_WASM_RESULT_OK = 0,
    NGX_PROXY_WASM_RESULT_EMPTY = 1,
    NGX_PROXY_WASM_RESULT_NOT_FOUND = 2,
    NGX_PROXY_WASM_RESULT_NOT_ALLOWED = 3,
    NGX_PROXY_WASM_RESULT_BAD_ARGUMENT = 4,
    NGX_PROXY_WASM_RESULT_BAD_MEM_ACCESS = 5,
    NGX_PROXY_WASM_RESULT_BAD_OPERATION = 6,
    NGX_PROXY_WASM_RESULT_CAS_MISMATCH = 7,
} ngx_proxy_wasm_result;


typedef enum {
    NGX_PROXY_WASM_ACTION_CONTINUE = 1,
    NGX_PROXY_WASM_ACTION_END_STREAM = 2,
    NGX_PROXY_WASM_ACTION_DONE = 3,
    NGX_PROXY_WASM_ACTION_PAUSE = 4,
    NGX_PROXY_WASM_ACTION_WAIT = 5,
    NGX_PROXY_WASM_ACTION_WAIT_FULL = 6,
    NGX_PROXY_WASM_ACTION_CLOSE = 7,
} ngx_proxy_wasm_action;


typedef enum {
    NGX_PROXY_WASM_LOG_TRACE = 1,
    NGX_PROXY_WASM_LOG_DEBUG = 2,
    NGX_PROXY_WASM_LOG_INFO = 3,
    NGX_PROXY_WASM_LOG_WARNING = 4,
    NGX_PROXY_WASM_LOG_ERROR = 5,
} ngx_proxy_wasm_log_level;


typedef enum {
    NGX_PROXY_WASM_CONTEXT_VM = 1,
    NGX_PROXY_WASM_CONTEXT_PLUGIN = 2,
    NGX_PROXY_WASM_CONTEXT_STREAM = 3,
    NGX_PROXY_WASM_CONTEXT_HTTP = 4,
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
} ngx_proxy_wasm_close_source_type;


typedef enum {
    NGX_PROXY_WASM_BUFFER_VM_CONFIGURATION = 1,
    NGX_PROXY_WASM_BUFFER_PLUGIN_CONFIGURATION = 2,
    NGX_PROXY_WASM_BUFFER_DOWNSTREAM_DATA = 3,
    NGX_PROXY_WASM_BUFFER_UPSTREAM_DATA = 4,
    NGX_PROXY_WASM_BUFFER_HTTP_REQUEST_BODY = 5,
    NGX_PROXY_WASM_BUFFER_HTTP_RESPONSE_BODY = 6,
    NGX_PROXY_WASM_BUFFER_HTTP_CALL_RESPONSE_BODY = 7,
} ngx_proxy_wasm_buffer_type;


typedef enum {
    NGX_PROXY_WASM_MAP_HTTP_REQUEST_HEADERS = 1,
    NGX_PROXY_WASM_MAP_HTTP_REQUEST_TRAILERS = 2,
    NGX_PROXY_WASM_MAP_HTTP_REQUEST_METADATA = 3,
    NGX_PROXY_WASM_MAP_HTTP_RESPONSE_HEADERS = 4,
    NGX_PROXY_WASM_MAP_HTTP_RESPONSE_TRAILERS = 5,
    NGX_PROXY_WASM_MAP_HTTP_RESPONSE_METADATA = 6,
    NGX_PROXY_WASM_MAP_HTTP_CALL_RESPONSE_HEADERS = 7,
    NGX_PROXY_WASM_MAP_HTTP_CALL_RESPONSE_TRAILERS = 8,
    NGX_PROXY_WASM_MAP_HTTP_CALL_RESPONSE_METADATA = 9,
} ngx_proxy_wasm_map_type;


typedef enum {
    NGX_PROXY_WASM_METRIC_COUNTER = 1,
    NGX_PROXY_WASM_METRIC_GAUGE = 2,
    NGX_PROXY_WASM_METRIC_HISTOGRAM = 3
} ngx_proxy_wasm_metric_type;


typedef struct {

    /* integration */

    ngx_wavm_func_t               *proxy_on_memory_allocate;

    /* context */

    ngx_wavm_func_t               *proxy_on_context_create;
    ngx_wavm_func_t               *proxy_on_context_finalize;

    /* configuration */

    ngx_wavm_func_t               *proxy_on_vm_start;
    ngx_wavm_func_t               *proxy_on_plugin_start;

    /* stream */

    ngx_wavm_func_t               *proxy_on_new_connection;
    ngx_wavm_func_t               *proxy_on_downstream_data;
    ngx_wavm_func_t               *proxy_on_downstream_close;
    ngx_wavm_func_t               *proxy_on_upstream_data;
    ngx_wavm_func_t               *proxy_on_upstream_close;

    /* http */

    ngx_wavm_func_t               *proxy_on_http_request_headers;
    ngx_wavm_func_t               *proxy_on_http_request_body;
    ngx_wavm_func_t               *proxy_on_http_request_trailers;
    ngx_wavm_func_t               *proxy_on_http_request_metadata;
    ngx_wavm_func_t               *proxy_on_http_response_headers;
    ngx_wavm_func_t               *proxy_on_http_response_body;
    ngx_wavm_func_t               *proxy_on_http_response_trailers;
    ngx_wavm_func_t               *proxy_on_http_response_metadata;

    /* shared queue */

    ngx_wavm_func_t               *proxy_on_queue_ready;

    /* http callouts */

    ngx_wavm_func_t               *proxy_on_http_call_response;

    /* grpc callouts */

    ngx_wavm_func_t               *proxy_on_grpc_call_response_header_metadata;
    ngx_wavm_func_t               *proxy_on_grpc_call_response_message;
    ngx_wavm_func_t               *proxy_on_grpc_call_response_trailer_metadata;
    ngx_wavm_func_t               *proxy_on_grpc_call_close;

    /* custom */

    ngx_wavm_func_t               *proxy_on_custom_callback;

} ngx_proxy_wasm_module_t;


#endif /* _NGX_PROXY_WASM_H_INCLUDED_ */
