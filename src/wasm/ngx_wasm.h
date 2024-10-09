#ifndef _NGX_WASM_H_INCLUDED_
#define _NGX_WASM_H_INCLUDED_


#include <ngx_wasmx.h>
#include <ngx_wrt.h>
#if (NGX_SSL)
#include <ngx_wasm_ssl.h>
#endif


#define NGX_LOG_DEBUG_WASM           NGX_LOG_DEBUG_ALL
#define NGX_LOG_WASM_NYI             NGX_LOG_ALERT

#define NGX_WASM_MODULE              0x5741534d   /* "WASM" */
#define NGX_WASM_CONF                0x10000000
#define NGX_WASMTIME_CONF            0x20000000
#define NGX_WASMER_CONF              0x40000000
#define NGX_V8_CONF                  0x80000000
#define NGX_METRICS_CONF             0x16000000

#define NGX_WASM_DONE_PHASE          15
#define NGX_WASM_BACKGROUND_PHASE    16

#define NGX_WASM_CONF_ERR_NO_WASM                                            \
    "is specified but config has no \"wasm\" section"

#define NGX_WASM_DEFAULT_RESOLVER_IP          "8.8.8.8"
#define NGX_WASM_DEFAULT_RESOLVER_TIMEOUT     30000
#define NGX_WASM_DEFAULT_SOCK_CONN_TIMEOUT    60000
#define NGX_WASM_DEFAULT_SOCK_SEND_TIMEOUT    60000
#define NGX_WASM_DEFAULT_SOCK_RECV_TIMEOUT    60000
#define NGX_WASM_DEFAULT_SOCK_BUF_SIZE        1024
#define NGX_WASM_DEFAULT_SOCK_LARGE_BUF_NUM   4
#define NGX_WASM_DEFAULT_SOCK_LARGE_BUF_SIZE  8192
#define NGX_WASM_DEFAULT_RESP_BODY_BUF_NUM    4
#define NGX_WASM_DEFAULT_RESP_BODY_BUF_SIZE   4096

#define ngx_wasm_core_cycle_get_conf(cycle)                                  \
    (cycle->conf_ctx[ngx_wasmx_module.index]                                 \
    ? ((ngx_wa_conf_t *) cycle->conf_ctx[ngx_wasmx_module.index])            \
        ->wasm_confs[ngx_wasm_core_module.ctx_index]                         \
    : NULL)

#define ngx_wasm_vec_set_i32(vec, i, v)                                      \
    (((wasm_val_vec_t *) (vec))->data[i].kind = WASM_I32);                   \
     ((wasm_val_vec_t *) (vec))->data[i].of.i32 = v

#define ngx_wasm_vec_set_i64(vec, i, v)                                      \
    (((wasm_val_vec_t *) (vec))->data[i].kind = WASM_I64);                   \
     ((wasm_val_vec_t *) (vec))->data[i].of.i64 = v

#define ngx_wasm_vec_set_f32(vec, i, v)                                      \
    (((wasm_val_vec_t *) (vec))->data[i].kind = WASM_F32);                   \
     ((wasm_val_vec_t *) (vec))->data[i].of.f32 = v

#define ngx_wasm_vec_set_f64(vec, i, v)                                      \
    (((wasm_val_vec_t *) (vec))->data[i].kind = WASM_F64);                   \
     ((wasm_val_vec_t *) (vec))->data[i].of.f64 = v


typedef struct ngx_wavm_s  ngx_wavm_t;
typedef struct ngx_wavm_store_s  ngx_wavm_store_t;
typedef struct ngx_wavm_module_s  ngx_wavm_module_t;
typedef struct ngx_wavm_funcref_s  ngx_wavm_funcref_t;
typedef struct ngx_wavm_func_s  ngx_wavm_func_t;
typedef uint32_t  ngx_wavm_ptr_t;


#if (NGX_WASM_HTTP)
typedef struct ngx_http_wasm_req_ctx_s  ngx_http_wasm_req_ctx_t;
#endif
#if (NGX_WASM_STREAM)
typedef struct ngx_stream_wasm_ctx_s  ngx_stream_wasm_ctx_t;
#endif


typedef struct {
    ngx_wa_create_conf_pt              create_conf;
    ngx_wa_init_conf_pt                init_conf;
    ngx_wa_init_pt                     init;
} ngx_wasm_module_t;


typedef struct {
    ngx_wavm_t                        *vm;
    ngx_wavm_conf_t                    vm_conf;
#if (NGX_SSL)
    ngx_wasm_ssl_conf_t                ssl_conf;
#endif
    ngx_msec_t                         resolver_timeout;
    ngx_msec_t                         connect_timeout;
    ngx_msec_t                         send_timeout;
    ngx_msec_t                         recv_timeout;

    size_t                             socket_buffer_size;
    ngx_bufs_t                         socket_large_buffers;
    ngx_flag_t                         socket_buffer_reuse;

    ngx_resolver_t                    *resolver;
    ngx_resolver_t                    *user_resolver;

    ngx_flag_t                         pwm_lua_resolver;
} ngx_wasm_core_conf_t;


ngx_wavm_t *ngx_wasm_main_vm(ngx_cycle_t *cycle);
void ngx_wasm_chain_log_debug(ngx_log_t *log, ngx_chain_t *in, char *label);
size_t ngx_wasm_chain_len(ngx_chain_t *in, unsigned *eof);
ngx_uint_t ngx_wasm_chain_clear(ngx_chain_t *in, size_t offset, unsigned *eof,
    unsigned *flush);
ngx_chain_t *ngx_wasm_chain_get_free_buf(ngx_pool_t *p,
    ngx_chain_t **free, size_t len, ngx_buf_tag_t tag, unsigned reuse);
ngx_int_t ngx_wasm_chain_prepend(ngx_pool_t *pool, ngx_chain_t **in,
    ngx_str_t *str, ngx_chain_t **free, ngx_buf_tag_t tag);
ngx_int_t ngx_wasm_chain_append(ngx_pool_t *pool, ngx_chain_t **in, size_t at,
    ngx_str_t *str, ngx_chain_t **free, ngx_buf_tag_t tag, unsigned extend);
ngx_int_t ngx_wasm_bytes_from_path(wasm_byte_vec_t *out, u_char *path,
    ngx_log_t *log);
ngx_uint_t ngx_wasm_list_nelts(ngx_list_t *list);
ngx_str_t *ngx_wasm_get_list_elem(ngx_list_t *map, u_char *key, size_t key_len);
ngx_msec_t ngx_wasm_monotonic_time();
void ngx_wasm_wall_time(void *rtime);
void ngx_wasm_log_error(ngx_uint_t level, ngx_log_t *log, ngx_err_t err,
    const char *fmt, ...);
#if (NGX_WASM_DYNAMIC_MODULE && (NGX_WASM_LUA || NGX_WASM_HTTP))
void swap_modules_if_needed(ngx_conf_t *cf, const char *m1, const char *m2);
#endif

/* blocks */
char *ngx_wasm_core_wasmtime_block(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
char *ngx_wasm_core_wasmer_block(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
char *ngx_wasm_core_v8_block(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
char *ngx_wasm_core_metrics_block(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);

/* directives */
char *ngx_wasm_core_flag_directive(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
char *ngx_wasm_core_module_directive(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
char *ngx_wasm_core_shm_kv_directive(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
char *ngx_wasm_core_shm_queue_directive(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
char *ngx_wasm_core_metrics_slab_size_directive(ngx_conf_t *cf,
    ngx_command_t *cmd, void *conf);
char *ngx_wasm_core_metrics_max_metric_name_length_directive(ngx_conf_t *cf,
    ngx_command_t *cmd, void *conf);
char *ngx_wasm_core_resolver_directive(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
char *ngx_wasm_core_pwm_lua_resolver_directive(ngx_conf_t *cf,
    ngx_command_t *cmd, void *conf);


extern ngx_module_t  ngx_wasm_core_module;


/* subsystems & phases */
typedef enum {
    NGX_WASM_SUBSYS_HTTP = 1,
    NGX_WASM_SUBSYS_STREAM,
} ngx_wasm_subsys_e;


typedef enum {
    NGX_WASM_STATE_CONTINUE,
    NGX_WASM_STATE_ERROR,
    NGX_WASM_STATE_YIELD,
} ngx_wasm_state_e;


typedef struct {
    ngx_str_t                                name;
    ngx_uint_t                               index;
    ngx_uint_t                               real_index;
    ngx_uint_t                               on;
} ngx_wasm_phase_t;


typedef struct {
    ngx_uint_t                               nphases;
    ngx_wasm_subsys_e                        kind;
    ngx_wasm_phase_t                        *phases;
} ngx_wasm_subsystem_t;


typedef struct ngx_wasm_lua_ctx_s  ngx_wasm_lua_ctx_t;

typedef struct {
    ngx_wasm_state_e                         state;
    ngx_connection_t                        *connection;
    ngx_buf_tag_t                           *buf_tag;
    ngx_wasm_subsystem_t                    *subsys;
#if (NGX_SSL)
    ngx_wasm_ssl_conf_t                     *ssl_conf;
#endif
#if (NGX_WASM_LUA)
    ngx_wasm_lua_ctx_t                      *entry_lctx;
#endif

    union {
#if (NGX_WASM_HTTP)
        ngx_http_wasm_req_ctx_t             *rctx;
#endif
#if (NGX_WASM_STREAM)
        ngx_stream_wasm_ctx_t               *sctx;
#endif
    } ctx;
} ngx_wasm_subsys_env_t;


static const ngx_str_t  NGX_WASM_STR_NO_HTTP =
    ngx_string("failed getting http main conf (no http{} block?)");
static const ngx_str_t  NGX_WASM_STR_NO_HTTP_UPSTREAM =
    ngx_string("failed getting http_upstream main conf (no http{} block?)");


#endif /* _NGX_WASM_H_INCLUDED_ */
