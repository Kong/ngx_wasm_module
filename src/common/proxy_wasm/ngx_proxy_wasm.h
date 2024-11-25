#ifndef _NGX_PROXY_WASM_H_INCLUDED_
#define _NGX_PROXY_WASM_H_INCLUDED_


#include <ngx_event.h>
#include <ngx_wavm.h>


#define NGX_PROXY_WASM_ROOT_CTX_ID  0


typedef enum {
    NGX_PROXY_WASM_0_1_0 = 0,
    NGX_PROXY_WASM_0_2_0,
    NGX_PROXY_WASM_0_2_1,
    NGX_PROXY_WASM_VNEXT,
    NGX_PROXY_WASM_UNKNOWN,
} ngx_proxy_wasm_abi_version_e;


typedef enum {
    NGX_PROXY_WASM_CONTEXT_HTTP = 0,
    NGX_PROXY_WASM_CONTEXT_STREAM = 1,
} ngx_proxy_wasm_context_type_e;


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
} ngx_proxy_wasm_result_e;


typedef enum {
    NGX_PROXY_WASM_ERR_NONE = 0,
    NGX_PROXY_WASM_ERR_UNKNOWN_ABI = 1,
    NGX_PROXY_WASM_ERR_BAD_ABI = 2,
    NGX_PROXY_WASM_ERR_BAD_HOST_INTERFACE = 3,
    NGX_PROXY_WASM_ERR_BAD_MODULE_INTERFACE = 4,
    NGX_PROXY_WASM_ERR_START_FAILED = 5,
    NGX_PROXY_WASM_ERR_VM_START_FAILED = 6,
    NGX_PROXY_WASM_ERR_CONFIGURE_FAILED = 7,
    NGX_PROXY_WASM_ERR_INSTANCE_TRAPPED = 8,
    NGX_PROXY_WASM_ERR_RETURN_ACTION = 9,
    NGX_PROXY_WASM_ERR_UNKNOWN = 10,
} ngx_proxy_wasm_err_e;


typedef enum {
    NGX_PROXY_WASM_ACTION_CONTINUE = 0,
    NGX_PROXY_WASM_ACTION_PAUSE = 1,
    NGX_PROXY_WASM_ACTION_END_STREAM = 2,
    NGX_PROXY_WASM_ACTION_DONE = 3,
    NGX_PROXY_WASM_ACTION_WAIT = 4,
    NGX_PROXY_WASM_ACTION_WAIT_FULL = 5,
    NGX_PROXY_WASM_ACTION_CLOSE = 6,
} ngx_proxy_wasm_action_e;


typedef enum {
    NGX_PROXY_WASM_STEP_REQ_HEADERS = 1,
    NGX_PROXY_WASM_STEP_REQ_BODY,
    NGX_PROXY_WASM_STEP_REQ_TRAILERS,
    NGX_PROXY_WASM_STEP_RESP_HEADERS,
    NGX_PROXY_WASM_STEP_RESP_BODY,
    NGX_PROXY_WASM_STEP_RESP_TRAILERS,
    NGX_PROXY_WASM_STEP_LOG,
    NGX_PROXY_WASM_STEP_DONE,
    NGX_PROXY_WASM_STEP_TICK,
    NGX_PROXY_WASM_STEP_DISPATCH_RESPONSE,
} ngx_proxy_wasm_step_e;


typedef enum {
    NGX_PROXY_WASM_ISOLATION_UNSET = 0,  /* FFI only */
    NGX_PROXY_WASM_ISOLATION_NONE = 1,
    NGX_PROXY_WASM_ISOLATION_STREAM = 2,
    NGX_PROXY_WASM_ISOLATION_FILTER = 3,
} ngx_proxy_wasm_isolation_mode_e;


typedef enum {
    NGX_PROXY_WASM_LOG_TRACE = 0,
    NGX_PROXY_WASM_LOG_DEBUG = 1,
    NGX_PROXY_WASM_LOG_INFO = 2,
    NGX_PROXY_WASM_LOG_WARNING = 3,
    NGX_PROXY_WASM_LOG_ERROR = 4,
    NGX_PROXY_WASM_LOG_CRITICAL = 5,
} ngx_proxy_wasm_log_level_e;


typedef enum {
    NGX_PROXY_WASM_STREAM_TYPE_HTTP_REQUEST = 0,
    NGX_PROXY_WASM_STREAM_TYPE_HTTP_RESPONSE = 1,
    NGX_PROXY_WASM_STREAM_TYPE_DOWNSTREAM = 2,
    NGX_PROXY_WASM_STREAM_TYPE_UPSTREAM = 3,
} ngx_proxy_wasm_stream_type_e;


#if 0
typedef enum {
    NGX_PROXY_WASM_CLOSE_SOURCE_LOCAL = 1,
    NGX_PROXY_WASM_CLOSE_SOURCE_REMOTE = 2,
} ngx_proxy_wasm_close_source_type_e;
#endif


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
} ngx_proxy_wasm_buffer_type_e;


typedef enum {
    NGX_PROXY_WASM_MAP_HTTP_REQUEST_HEADERS = 0,
    NGX_PROXY_WASM_MAP_HTTP_REQUEST_TRAILERS = 1,
    NGX_PROXY_WASM_MAP_HTTP_RESPONSE_HEADERS = 2,
    NGX_PROXY_WASM_MAP_HTTP_RESPONSE_TRAILERS = 3,
    NGX_PROXY_WASM_MAP_GRPC_RECEIVE_INITIAL_METADATA = 4,
    NGX_PROXY_WASM_MAP_GRPC_RECEIVE_TRAILING_METADATA = 5,
    NGX_PROXY_WASM_MAP_HTTP_CALL_RESPONSE_HEADERS = 6,
    NGX_PROXY_WASM_MAP_HTTP_CALL_RESPONSE_TRAILERS = 7,
} ngx_proxy_wasm_map_type_e;


typedef enum {
    NGX_PROXY_WASM_METRIC_COUNTER = 0,
    NGX_PROXY_WASM_METRIC_GAUGE = 1,
    NGX_PROXY_WASM_METRIC_HISTOGRAM = 2,
} ngx_proxy_wasm_metric_type_e;


typedef struct ngx_proxy_wasm_ctx_s  ngx_proxy_wasm_ctx_t;
typedef struct ngx_proxy_wasm_filter_s  ngx_proxy_wasm_filter_t;
typedef struct ngx_proxy_wasm_exec_s  ngx_proxy_wasm_exec_t;
typedef struct ngx_proxy_wasm_instance_s  ngx_proxy_wasm_instance_t;
#ifdef NGX_WASM_HTTP
typedef struct ngx_http_proxy_wasm_dispatch_s  ngx_http_proxy_wasm_dispatch_t;
#endif
typedef ngx_str_t  ngx_proxy_wasm_marshalled_map_t;


typedef struct {
    ngx_queue_t                        busy;
    ngx_queue_t                        free;
    ngx_queue_t                        sweep;
    ngx_pool_t                        *pool;
} ngx_proxy_wasm_store_t;


typedef struct {
    ngx_str_t                          log_prefix;
    ngx_log_t                         *orig_log;
    ngx_proxy_wasm_exec_t             *pwexec;
} ngx_proxy_wasm_log_ctx_t;


struct ngx_proxy_wasm_exec_s {
    ngx_uint_t                         root_id;
    ngx_uint_t                         id;
    ngx_uint_t                         index;
    ngx_uint_t                         tick_period;
    ngx_rbtree_node_t                  node;
    ngx_proxy_wasm_err_e               ecode;
    ngx_pool_t                        *pool;
    ngx_log_t                         *log;
    ngx_proxy_wasm_log_ctx_t           log_ctx;
    ngx_proxy_wasm_ctx_t              *parent;
    ngx_proxy_wasm_filter_t           *filter;
    ngx_proxy_wasm_instance_t         *ictx;
    ngx_proxy_wasm_store_t            *store;
    ngx_event_t                       *ev;
#ifdef NGX_WASM_HTTP
    ngx_http_proxy_wasm_dispatch_t    *dispatch_call;  /* swap pointer for host functions */
#endif
    ngx_queue_t                        dispatch_calls;

    /* flags */

    unsigned                           started:1;
    unsigned                           in_tick:1;
    unsigned                           ecode_logged:1;
};


#if (NGX_WASM_LUA)
typedef ngx_int_t (*ngx_proxy_wasm_properties_ffi_handler_pt)(void *data,
    ngx_str_t *key, ngx_str_t *value, ngx_str_t *err);
#endif


struct ngx_proxy_wasm_ctx_s {
    ngx_uint_t                                    id;      /* r->connection->number */
    ngx_uint_t                                    nfilters;
    ngx_array_t                                   pwexecs;
    ngx_uint_t                                    isolation;
    ngx_proxy_wasm_store_t                        store;
    ngx_proxy_wasm_context_type_e                 type;
    ngx_proxy_wasm_exec_t                        *rexec;  /* root exec ctx */
    ngx_log_t                                    *log;
    ngx_pool_t                                   *pool;
    ngx_pool_t                                   *parent_pool;
    void                                         *data;

    /* control */

    ngx_wasm_phase_t                             *phase;
    ngx_proxy_wasm_action_e                       action;
    ngx_proxy_wasm_step_e                         step;
    ngx_proxy_wasm_step_e                         last_completed_step;
    ngx_uint_t                                    exec_index;

    /* cache */

    size_t                                        req_body_len;
    ngx_str_t                                     authority;
    ngx_str_t                                     scheme;
    ngx_str_t                                     path;                  /* r->uri + r->args */
    ngx_str_t                                     start_time;            /* r->start_sec + r->start_msec */
    ngx_str_t                                     upstream_address;      /* 1st part of ngx.upstream_addr */
    ngx_str_t                                     upstream_port;         /* 2nd part of ngx.upstsream_addr */
    ngx_str_t                                     connection_id;         /* r->connection->number */
    ngx_str_t                                     mtls;                  /* ngx.https && ngx.ssl_client_verify */
    ngx_str_t                                     root_id;               /* pwexec->root_id */
    ngx_str_t                                     dispatch_call_status;  /* dispatch response status */
    ngx_str_t                                     response_status;       /* response status */
#if (NGX_DEBUG)
    ngx_str_t                                     worker_id;             /* ngx_worker */
#endif
    ngx_uint_t                                    dispatch_call_code;
    ngx_uint_t                                    response_code;

    /* host properties */

    ngx_rbtree_t                                  host_props_tree;
    ngx_rbtree_node_t                             host_props_sentinel;
#if (NGX_WASM_LUA)
    ngx_proxy_wasm_properties_ffi_handler_pt      host_props_ffi_getter;
    ngx_proxy_wasm_properties_ffi_handler_pt      host_props_ffi_setter;
    void                                         *host_props_ffi_handler_data;  /* r */
#endif

    /* flags */

    unsigned                                      main:1;                /* r->main */
    unsigned                                      init:1;                /* can be utilized (has no filters) */
    unsigned                                      ready:1;               /* filters chain ready */
    unsigned                                      req_headers_in_access:1;
};


struct ngx_proxy_wasm_instance_s {
    ngx_queue_t                        q;                 /* store busy/free/sweep */
    ngx_rbtree_t                       tree_ctxs;
    ngx_rbtree_t                       root_ctxs;
    ngx_rbtree_node_t                  sentinel_ctxs;
    ngx_rbtree_node_t                  sentinel_root_ctxs;
    ngx_wavm_module_t                 *module;
    ngx_wavm_instance_t               *instance;
    ngx_proxy_wasm_store_t            *store;
    ngx_pool_t                        *pool;
    ngx_log_t                         *log;

    /* swap */

    ngx_proxy_wasm_exec_t             *pwexec;            /* current pwexec */
};


typedef struct {
    ngx_proxy_wasm_ctx_t              *(*get_context)(void *data);
    ngx_int_t                          (*resume)(ngx_proxy_wasm_exec_t *pwexec,
                                                 ngx_proxy_wasm_step_e step,
                                                 ngx_proxy_wasm_action_e *out);
    ngx_int_t                          (*ecode)(ngx_proxy_wasm_err_e ecode);
} ngx_proxy_wasm_subsystem_t;


struct ngx_proxy_wasm_filter_s {
    ngx_str_t                     *name;
    ngx_log_t                     *log;
    ngx_pool_t                    *pool;
    ngx_str_t                      config;
    uintptr_t                      data;
    ngx_rbtree_node_t              node;
    ngx_wavm_module_t             *module;
    ngx_proxy_wasm_subsystem_t    *subsystem;
    ngx_proxy_wasm_store_t        *store;   /* mcf->pwroot.store */
    ngx_proxy_wasm_err_e           ecode;

    /* dyn config */

    ngx_uint_t                     id;
    ngx_uint_t                     max_pairs;

    /**
     * SDK
     * - targets: vNEXT-reviewed (proxy-wasm/spec#1)
     *            0.1.0 (rust-sdk)
     *            0.2.1 (cpp-sdk)
     * - tested: 0.1.0
     */

    ngx_proxy_wasm_abi_version_e   abi_version;

    ngx_wavm_funcref_t            *proxy_on_memory_allocate;

    /* context */

    ngx_wavm_funcref_t            *proxy_on_context_create;
    ngx_wavm_funcref_t            *proxy_on_context_finalize;
    ngx_wavm_funcref_t            *proxy_on_done;
    ngx_wavm_funcref_t            *proxy_on_log;

    /* configuration */

    ngx_wavm_funcref_t            *proxy_on_vm_start;
    ngx_wavm_funcref_t            *proxy_on_plugin_start;

    /* stream */

    ngx_wavm_funcref_t            *proxy_on_new_connection;
    ngx_wavm_funcref_t            *proxy_on_downstream_data;
    ngx_wavm_funcref_t            *proxy_on_upstream_data;
    ngx_wavm_funcref_t            *proxy_on_downstream_close;
    ngx_wavm_funcref_t            *proxy_on_upstream_close;

    /* http */

    ngx_wavm_funcref_t            *proxy_on_http_request_headers;
    ngx_wavm_funcref_t            *proxy_on_http_request_body;
    ngx_wavm_funcref_t            *proxy_on_http_request_trailers;
    ngx_wavm_funcref_t            *proxy_on_http_request_metadata;
    ngx_wavm_funcref_t            *proxy_on_http_response_headers;
    ngx_wavm_funcref_t            *proxy_on_http_response_body;
    ngx_wavm_funcref_t            *proxy_on_http_response_trailers;
    ngx_wavm_funcref_t            *proxy_on_http_response_metadata;

    /* shared queue */

    ngx_wavm_funcref_t            *proxy_on_queue_ready;

    /* timers */

    ngx_wavm_funcref_t            *proxy_create_timer;
    ngx_wavm_funcref_t            *proxy_delete_timer;
    ngx_wavm_funcref_t            *proxy_on_timer_ready;

    /* http callouts */

    ngx_wavm_funcref_t            *proxy_on_http_call_response;

    /* grpc callouts */

    ngx_wavm_funcref_t            *proxy_on_grpc_call_response_header_metadata;
    ngx_wavm_funcref_t            *proxy_on_grpc_call_response_message;
    ngx_wavm_funcref_t            *proxy_on_grpc_call_response_trailer_metadata;
    ngx_wavm_funcref_t            *proxy_on_grpc_call_close;

    /* custom extensions */

    ngx_wavm_funcref_t            *proxy_on_custom_callback;

    /* flags */

    unsigned                       loaded:1;
    unsigned                       started:1;
};


typedef struct {
    ngx_array_t                    filter_ids;
    ngx_rbtree_t                   tree;
    ngx_rbtree_node_t              sentinel;
    ngx_proxy_wasm_store_t         store;
    unsigned                       init:1;
} ngx_proxy_wasm_filters_root_t;


/* root context */
ngx_proxy_wasm_filters_root_t *ngx_proxy_wasm_root_alloc(ngx_pool_t *pool);
void ngx_proxy_wasm_root_init(ngx_proxy_wasm_filters_root_t *pwroot,
    ngx_pool_t *pool);
void ngx_proxy_wasm_root_destroy(ngx_proxy_wasm_filters_root_t *pwroot);
ngx_int_t ngx_proxy_wasm_load(ngx_proxy_wasm_filters_root_t *pwroot,
    ngx_proxy_wasm_filter_t *filter, ngx_log_t *log);
ngx_int_t ngx_proxy_wasm_start(ngx_proxy_wasm_filters_root_t *pwroot);


/* stream context */
ngx_proxy_wasm_ctx_t *ngx_proxy_wasm_ctx_alloc(ngx_pool_t *pool);
ngx_proxy_wasm_ctx_t *ngx_proxy_wasm_ctx(ngx_proxy_wasm_filters_root_t *pwroot,
    ngx_array_t *filter_ids, ngx_uint_t isolation,
    ngx_proxy_wasm_subsystem_t *subsys, void *data);
void ngx_proxy_wasm_ctx_destroy(ngx_proxy_wasm_ctx_t *pwctx);
ngx_int_t ngx_proxy_wasm_resume(ngx_proxy_wasm_ctx_t *pwctx,
    ngx_wasm_phase_t *phase, ngx_proxy_wasm_step_e step);
ngx_proxy_wasm_err_e ngx_proxy_wasm_run_step(ngx_proxy_wasm_exec_t *pwexec,
    ngx_proxy_wasm_step_e step);
ngx_uint_t ngx_proxy_wasm_dispatch_calls_total(ngx_proxy_wasm_exec_t *pwexec);
void ngx_proxy_wasm_dispatch_calls_cancel(ngx_proxy_wasm_exec_t *pwexec);


/* host handlers */
ngx_wavm_ptr_t ngx_proxy_wasm_alloc(ngx_proxy_wasm_exec_t *pwexec, size_t size);


/* utils */
ngx_str_t *ngx_proxy_wasm_step_name(ngx_proxy_wasm_step_e step);
ngx_str_t *ngx_proxy_wasm_action_name(ngx_proxy_wasm_action_e action);
u_char *ngx_proxy_wasm_log_error_handler(ngx_log_t *log, u_char *buf,
    size_t len);
void ngx_proxy_wasm_log_error(ngx_uint_t level, ngx_log_t *log,
    ngx_proxy_wasm_err_e err, const char *fmt, ...);
void ngx_proxy_wasm_filter_tick_handler(ngx_event_t *ev);
ngx_int_t ngx_proxy_wasm_pairs_unmarshal(ngx_proxy_wasm_exec_t *pwexec,
    ngx_array_t *dst, ngx_proxy_wasm_marshalled_map_t *map);
unsigned ngx_proxy_wasm_marshal(ngx_proxy_wasm_exec_t *pwexec,
    ngx_list_t *list, ngx_array_t *extras, ngx_wavm_ptr_t *out,
    uint32_t *out_size, ngx_uint_t *truncated);


static ngx_inline void
ngx_proxy_wasm_ctx_set_next_action(ngx_proxy_wasm_ctx_t *pwctx,
    ngx_proxy_wasm_action_e action)
{
    ngx_proxy_wasm_log_error(NGX_LOG_DEBUG, pwctx->log, 0,
                             "setting next action: pwctx->action = \"%V\" "
                             "(pwctx: %p)",
                             ngx_proxy_wasm_action_name(action), pwctx);
    pwctx->action = action;
}


static ngx_inline void
ngx_proxy_wasm_ctx_reset_chain(ngx_proxy_wasm_ctx_t *pwctx)
{
    ngx_proxy_wasm_log_error(NGX_LOG_DEBUG, pwctx->log, 0,
                             "resetting filter chain: pwctx->exec_index "
                             "%l to 0 (pwctx: %p)",
                             pwctx->exec_index, pwctx);

    pwctx->exec_index = 0;
}


static ngx_inline ngx_proxy_wasm_exec_t *
ngx_proxy_wasm_instance2pwexec(ngx_wavm_instance_t *instance)
{
    ngx_proxy_wasm_instance_t  *ictx = instance->data;

    return ictx->pwexec;
}


static ngx_inline ngx_wavm_instance_t *
ngx_proxy_wasm_pwexec2instance(ngx_proxy_wasm_exec_t *pwexec)
{
    return pwexec->ictx->instance;
}


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
ngx_proxy_wasm_result_trap(ngx_proxy_wasm_exec_t *pwexec, char *trapmsg,
    wasm_val_t rets[], ngx_int_t rc)
{
    ngx_wavm_instance_t  *instance = ngx_proxy_wasm_pwexec2instance(pwexec);

    ngx_wavm_instance_trap_printf(instance, trapmsg);

    rets[0] = (wasm_val_t) WASM_I32_VAL(NGX_PROXY_WASM_RESULT_OK);

    return rc;
}


#if 0
static ngx_inline ngx_int_t
ngx_proxy_wasm_result_trap_printf(ngx_proxy_wasm_exec_t *pwexec,
    wasm_val_t rets[], const char *fmt, ...)
{
    ngx_wavm_instance_t  *instance = ngx_proxy_wasm_pwexec2instance(pwexec);
    va_list               args;

    va_start(args, fmt);
    ngx_wavm_instance_trap_vprintf(instance, fmt, args);
    va_end(args);

    return ngx_proxy_wasm_result_ok(rets);
}
#endif


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


static ngx_inline ngx_int_t
ngx_proxy_wasm_result_cas_mismatch(wasm_val_t rets[])
{
    rets[0] = (wasm_val_t) WASM_I32_VAL(NGX_PROXY_WASM_RESULT_CAS_MISMATCH);
    return NGX_WAVM_OK;
}


static ngx_inline ngx_int_t
ngx_proxy_wasm_result_empty(wasm_val_t rets[])
{
    rets[0] = (wasm_val_t) WASM_I32_VAL(NGX_PROXY_WASM_RESULT_EMPTY);
    return NGX_WAVM_OK;
}


extern ngx_wavm_host_def_t  ngx_proxy_wasm_host;


#endif /* _NGX_PROXY_WASM_H_INCLUDED_ */
