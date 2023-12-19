#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_wavm.h>
#include <ngx_wasm_shm.h>


static void *ngx_wasm_core_create_conf(ngx_conf_t *cf);
static char *ngx_wasm_core_init_conf(ngx_conf_t *cf, void *conf);
static ngx_int_t ngx_wasm_core_init(ngx_cycle_t *cycle);
static ngx_int_t ngx_wasm_core_init_process(ngx_cycle_t *cycle);
#if (NGX_SSL)
static ngx_int_t ngx_wasm_core_init_ssl(ngx_cycle_t *cycle);
#endif

extern ngx_wavm_host_def_t  ngx_wasm_core_interface;


static ngx_command_t  ngx_wasm_core_commands[] = {

    /* blocks */

    { ngx_string("wasmtime"),
      NGX_WASM_CONF|NGX_CONF_BLOCK|NGX_CONF_NOARGS,
      ngx_wasm_core_wasmtime_block,
      0,
      0,
      NULL },

    { ngx_string("wasmer"),
      NGX_WASM_CONF|NGX_CONF_BLOCK|NGX_CONF_NOARGS,
      ngx_wasm_core_wasmer_block,
      0,
      0,
      NULL },

    { ngx_string("v8"),
      NGX_WASM_CONF|NGX_CONF_BLOCK|NGX_CONF_NOARGS,
      ngx_wasm_core_v8_block,
      0,
      0,
      NULL },

    /* directives */

    { ngx_string("flag"),
      NGX_WASMTIME_CONF|NGX_WASMER_CONF|NGX_V8_CONF|NGX_CONF_TAKE2,
      ngx_wasm_core_flag_directive,
      0,
      0,
      NULL },

    { ngx_string("compiler"),
      NGX_WASM_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_str_slot,
      0,
      offsetof(ngx_wasm_core_conf_t, vm_conf)
      + offsetof(ngx_wavm_conf_t, compiler),
      NULL },

    { ngx_string("backtraces"),
      NGX_WASM_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      0,
      offsetof(ngx_wasm_core_conf_t, vm_conf)
      + offsetof(ngx_wavm_conf_t, backtraces),
      NULL },

    { ngx_string("module"),
      NGX_WASM_CONF|NGX_CONF_TAKE23,
      ngx_wasm_core_module_directive,
      0,
      0,
      NULL },

    { ngx_string("shm_kv"),
      NGX_WASM_CONF|NGX_CONF_TAKE23|NGX_CONF_TAKE4,
      ngx_wasm_core_shm_kv_directive,
      0,
      0,
      NULL },

    { ngx_string("shm_queue"),
      NGX_WASM_CONF|NGX_CONF_TAKE23|NGX_CONF_TAKE4,
      ngx_wasm_core_shm_queue_directive,
      0,
      0,
      NULL },

#if (NGX_SSL)
    { ngx_string("tls_trusted_certificate"),
      NGX_WASM_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_str_slot,
      0,
      offsetof(ngx_wasm_core_conf_t, ssl_conf)
      + offsetof(ngx_wasm_ssl_conf_t, trusted_certificate),
      NULL },

    { ngx_string("tls_verify_cert"),
      NGX_WASM_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_flag_slot,
      0,
      offsetof(ngx_wasm_core_conf_t, ssl_conf)
      + offsetof(ngx_wasm_ssl_conf_t, verify_cert),
      NULL },

    { ngx_string("tls_verify_host"),
      NGX_WASM_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_flag_slot,
      0,
      offsetof(ngx_wasm_core_conf_t, ssl_conf)
      + offsetof(ngx_wasm_ssl_conf_t, verify_host),
      NULL },

    { ngx_string("tls_no_verify_warn"),
      NGX_WASM_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_flag_slot,
      0,
      offsetof(ngx_wasm_core_conf_t, ssl_conf)
      + offsetof(ngx_wasm_ssl_conf_t, no_verify_warn),
      NULL },
#endif

    { ngx_string("resolver"),
      NGX_WASM_CONF|NGX_CONF_1MORE,
      ngx_wasm_core_resolver_directive,
      0,
      0,
      NULL },

    { ngx_string("resolver_timeout"),
      NGX_WASM_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_msec_slot,
      0,
      offsetof(ngx_wasm_core_conf_t, resolver_timeout),
      NULL },

    { ngx_string("proxy_wasm_lua_resolver"),
      NGX_WASM_CONF|NGX_CONF_TAKE1,
      ngx_wasm_core_pwm_lua_resolver_directive,
      0,
      offsetof(ngx_wasm_core_conf_t, pwm_lua_resolver),
      NULL },

    { ngx_string("socket_connect_timeout"),
      NGX_WASM_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_msec_slot,
      0,
      offsetof(ngx_wasm_core_conf_t, connect_timeout),
      NULL },

    { ngx_string("socket_send_timeout"),
      NGX_WASM_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_msec_slot,
      0,
      offsetof(ngx_wasm_core_conf_t, send_timeout),
      NULL },

    { ngx_string("socket_read_timeout"),
      NGX_WASM_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_msec_slot,
      0,
      offsetof(ngx_wasm_core_conf_t, recv_timeout),
      NULL },

    { ngx_string("socket_buffer_size"),
      NGX_WASM_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_size_slot,
      0,
      offsetof(ngx_wasm_core_conf_t, socket_buffer_size),
      NULL },

    { ngx_string("socket_buffer_reuse"),
      NGX_WASM_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_flag_slot,
      0,
      offsetof(ngx_wasm_core_conf_t, socket_buffer_reuse),
      NULL },

    { ngx_string("socket_large_buffers"),
      NGX_WASM_CONF|NGX_CONF_TAKE2,
      ngx_conf_set_bufs_slot,
      0,
      offsetof(ngx_wasm_core_conf_t, socket_large_buffers),
      NULL },

    ngx_null_command
};


static ngx_wasm_module_t  ngx_wasm_core_module_ctx = {
    ngx_wasm_core_create_conf,             /* create configuration */
    ngx_wasm_core_init_conf,               /* init configuration */
    ngx_wasm_core_init                     /* init module */
};


ngx_module_t  ngx_wasm_core_module = {
    NGX_MODULE_V1,
    &ngx_wasm_core_module_ctx,             /* module context */
    ngx_wasm_core_commands,                /* module directives */
    NGX_WASM_MODULE,                       /* module type */
    NULL,                                  /* init master */
    NULL,                                  /* init module */
    ngx_wasm_core_init_process,            /* init process */
    NULL,                                  /* init thread */
    NULL,                                  /* exit thread */
    NULL,                                  /* exit process */
    NULL,                                  /* exit master */
    NGX_MODULE_V1_PADDING
};


ngx_inline ngx_wavm_t *
ngx_wasm_main_vm(ngx_cycle_t *cycle)
{
    ngx_wasm_core_conf_t  *wcf;

    wcf = ngx_wasm_core_cycle_get_conf(cycle);
    if (wcf == NULL) {
        return NULL;
    }

    return wcf->vm;
}


ngx_inline ngx_array_t *
ngx_wasm_core_shms(ngx_cycle_t *cycle)
{
    ngx_wasm_core_conf_t  *wcf;

    wcf = ngx_wasm_core_cycle_get_conf(cycle);
    ngx_wa_assert(wcf);

    return &wcf->shms;
}


static void
ngx_wasm_core_cleanup_pool(void *data)
{
    ngx_cycle_t  *cycle = data;
    ngx_wavm_t   *vm = ngx_wasm_main_vm(cycle);
#if (NGX_SSL)
    ngx_ssl_t    *ssl = &ngx_wasm_core_ssl_conf(cycle)->ssl;
#endif

    dd("cleaning up pool");

    ngx_wavm_destroy(vm);

#if (NGX_SSL)
    if (ssl->ctx) {
        ngx_ssl_cleanup_ctx(ssl);
    }
#endif
}


#if (NGX_WASM_DYNAMIC_MODULE && (NGX_WASM_LUA || NGX_WASM_HTTP))
static unsigned
get_module_index(ngx_cycle_t *cycle, const char *module_name, ngx_uint_t *out)
{
    size_t  i;

    for (i = 0; cycle->modules[i]; i++) {
        if (ngx_str_eq(cycle->modules[i]->name, -1, module_name, -1)) {
            *out = i;
            return 1;
        }
    }

    return 0;
}


static void
swap_modules_if_needed(ngx_conf_t *cf, const char *m1, const char *m2)
{
    ngx_uint_t     m1_idx, m2_idx;
    ngx_cycle_t   *cycle = cf->cycle;
    ngx_module_t  *tmp;

    if (get_module_index(cycle, m1, &m1_idx)
        && get_module_index(cycle, m2, &m2_idx)
        && m1_idx < m2_idx)
    {
        ngx_wasm_log_error(NGX_LOG_NOTICE, cf->log, 0,
                           "swapping modules: \"%s\" (index: %l) and \"%s\" "
                           "(index: %l)", m1, m1_idx, m2, m2_idx);

        tmp = cycle->modules[m1_idx];
        cycle->modules[m1_idx] = cycle->modules[m2_idx];
        cycle->modules[m2_idx] = tmp;
    }
}
#endif


static void *
ngx_wasm_core_create_conf(ngx_conf_t *cf)
{
    ngx_cycle_t             *cycle = cf->cycle;
    ngx_pool_cleanup_t      *cln;
    ngx_wasm_core_conf_t    *wcf;
    static const ngx_str_t   vm_name = ngx_string("main");
    static const ngx_str_t   runtime_name = ngx_string(NGX_WASM_RUNTIME);
    static const ngx_str_t   ip = ngx_string(NGX_WASM_DEFAULT_RESOLVER_IP);

#if (NGX_WASM_DYNAMIC_MODULE && NGX_WASM_HTTP)
    /**
     * headers_more_filter is expected to run before ngx_wasm_filter in the
     * test suite (filters run in reverse order).
     *
     * Mimic static cycle order if necessary:
     *   1. ngx_http_wasm_filter_module
     *   2. ngx_http_headers_more_filter_module
     */
    swap_modules_if_needed(cf, "ngx_http_headers_more_filter_module",
                           "ngx_http_wasm_filter_module");
#endif
#if (NGX_WASM_DYNAMIC_MODULE && NGX_WASM_LUA)
    /**
     * ngx_lua init_worker may need wasm modules, which are only loaded in
     * ngx_wasm init_worker.
     *
     * Mimic static cycle order if necessary:
     *   1. ngx_wasm_core_module
     *   2. ngx_http_lua_module
     */
    swap_modules_if_needed(cf, "ngx_http_lua_module", "ngx_wasm_core_module");

    /**
     * ngx_http_lua_module filter is expected to run before ngx_wasm_filter in
     * the test suite (filters run in reverse order).
     *
     * Mimic static cycle order if necessary:
     *   1. ngx_http_lua_module (filter)
     *   2. ngx_http_wasm_filter_module
     */
    swap_modules_if_needed(cf, "ngx_http_lua_module",
                           "ngx_http_wasm_filter_module");
#endif

    wcf = ngx_pcalloc(cycle->pool, sizeof(ngx_wasm_core_conf_t));
    if (wcf == NULL) {
        return NULL;
    }

    wcf->vm = ngx_wavm_create(cycle, &vm_name, &wcf->vm_conf,
                              &ngx_wasm_core_interface);
    if (wcf->vm == NULL) {
        return NULL;
    }

    cln = ngx_pool_cleanup_add(cycle->pool, 0);
    if (cln == NULL) {
        return NULL;
    }

    cln->handler = ngx_wasm_core_cleanup_pool;
    cln->data = cycle;

    if (ngx_array_init(&wcf->shms, cycle->pool,
                       1, sizeof(ngx_wasm_shm_mapping_t))
        != NGX_OK)
    {
        /*
         * future ngx_array_push calls will fail allocating memory and cause
         * silent pool corruption
         */
        return NULL;
    }

    wcf->vm_conf.vm_name = wcf->vm->name;
    wcf->vm_conf.runtime_name = &runtime_name;
    wcf->vm_conf.backtraces = NGX_CONF_UNSET;

    if (ngx_array_init(&wcf->vm_conf.flags, cycle->pool,
                       1, sizeof(ngx_wrt_flag_t))
        != NGX_OK)
    {
        return NULL;
    }

#if (NGX_SSL)
    wcf->ssl_conf.verify_cert = NGX_CONF_UNSET;
    wcf->ssl_conf.verify_host = NGX_CONF_UNSET;
    wcf->ssl_conf.no_verify_warn = NGX_CONF_UNSET;
#endif

    wcf->resolver_timeout = NGX_CONF_UNSET_MSEC;
    wcf->connect_timeout = NGX_CONF_UNSET_MSEC;
    wcf->send_timeout = NGX_CONF_UNSET_MSEC;
    wcf->recv_timeout = NGX_CONF_UNSET_MSEC;
    wcf->pwm_lua_resolver = NGX_CONF_UNSET;

    wcf->socket_buffer_size = NGX_CONF_UNSET_SIZE;
    wcf->socket_buffer_reuse = NGX_CONF_UNSET;

    wcf->user_resolver = NULL;
    wcf->resolver = ngx_resolver_create(cf, (ngx_str_t *) &ip, 1);
    if (wcf->resolver == NULL) {
        return NULL;
    }

    return wcf;
}


static char *
ngx_wasm_core_init_conf(ngx_conf_t *cf, void *conf)
{
    ngx_wasm_core_conf_t  *wcf = conf;

#if (NGX_SSL)
    if (wcf->ssl_conf.verify_cert == NGX_CONF_UNSET) {
        wcf->ssl_conf.verify_cert = 0;
    }

    if (wcf->ssl_conf.verify_host == NGX_CONF_UNSET) {
        wcf->ssl_conf.verify_host = 0;
    }

    if (wcf->ssl_conf.no_verify_warn == NGX_CONF_UNSET) {
        wcf->ssl_conf.no_verify_warn = 1;
    }
#endif

    if (wcf->vm_conf.backtraces == NGX_CONF_UNSET) {
        wcf->vm_conf.backtraces = 0;
    }

    if (wcf->resolver_timeout == NGX_CONF_UNSET_MSEC) {
        wcf->resolver_timeout = NGX_WASM_DEFAULT_RESOLVER_TIMEOUT;
    }

    if (wcf->connect_timeout == NGX_CONF_UNSET_MSEC) {
        wcf->connect_timeout = NGX_WASM_DEFAULT_SOCK_CONN_TIMEOUT;
    }

    if (wcf->send_timeout == NGX_CONF_UNSET_MSEC) {
        wcf->send_timeout = NGX_WASM_DEFAULT_SOCK_SEND_TIMEOUT;
    }

    if (wcf->recv_timeout == NGX_CONF_UNSET_MSEC) {
        wcf->recv_timeout = NGX_WASM_DEFAULT_RECV_TIMEOUT;
    }

    if (wcf->socket_buffer_size == NGX_CONF_UNSET_SIZE) {
        wcf->socket_buffer_size = NGX_WASM_DEFAULT_SOCK_BUF_SIZE;
    }

    if (!wcf->socket_large_buffers.num || !wcf->socket_large_buffers.size) {
        wcf->socket_large_buffers.num = NGX_WASM_DEFAULT_SOCK_LARGE_BUF_NUM;
        wcf->socket_large_buffers.size = NGX_WASM_DEFAULT_SOCK_LARGE_BUF_SIZE;
    }

    if (wcf->socket_buffer_reuse == NGX_CONF_UNSET) {
        wcf->socket_buffer_reuse = 1;
    }

    if (wcf->pwm_lua_resolver == NGX_CONF_UNSET) {
        wcf->pwm_lua_resolver = 0;
    }

    return NGX_CONF_OK;
}


static ngx_int_t
ngx_wasm_core_init(ngx_cycle_t *cycle)
{
    ngx_wavm_t            *vm;
    ngx_wasm_core_conf_t  *wcf;

    ngx_log_debug0(NGX_LOG_DEBUG_WASM, cycle->log, 0,
                   "wasm core module initializing");

    wcf = ngx_wasm_core_cycle_get_conf(cycle);
    ngx_wa_assert(wcf); /* ngx_wasmx.c would not invoke init if wcf == NULL */

    vm = wcf->vm;

    if (vm && ngx_wavm_init(vm) != NGX_OK) {
        return NGX_ERROR;
    }

    if (ngx_wasm_shm_init(cycle) != NGX_OK) {
        return NGX_ERROR;
    }

#if (NGX_SSL)
    if (ngx_wasm_core_init_ssl(cycle) != NGX_OK) {
        ngx_wavm_destroy(vm);
        return NGX_ERROR;
    }
#endif

    return NGX_OK;
}


static ngx_int_t
ngx_wasm_core_init_process(ngx_cycle_t *cycle)
{
    ngx_wavm_t  *vm;

    vm = ngx_wasm_main_vm(cycle);
    if (vm == NULL) {
        return NGX_OK;
    }

    if (ngx_wavm_load(vm) != NGX_OK) {
        return NGX_ERROR;
    }

    if (ngx_wasm_shm_init_process(cycle) != NGX_OK) {
        return NGX_ERROR;
    }

    return NGX_OK;
}


#if (NGX_SSL)
ngx_inline ngx_wasm_ssl_conf_t *
ngx_wasm_core_ssl_conf(ngx_cycle_t *cycle)
{
    ngx_wasm_core_conf_t  *wcf;

    wcf = ngx_wasm_core_cycle_get_conf(cycle);
    if (wcf == NULL) {
        return NULL;
    }

    return &wcf->ssl_conf;
}


static ngx_int_t
ngx_wasm_core_init_ssl(ngx_cycle_t *cycle)
{
    ngx_wasm_core_conf_t  *wcf;
    ngx_str_t             *trusted_crt;

    ngx_log_debug0(NGX_LOG_DEBUG_WASM, cycle->log, 0,
                   "wasm initializing tls");

    wcf = ngx_wasm_core_cycle_get_conf(cycle);
    ngx_wa_assert(wcf);

    wcf->ssl_conf.ssl.log = cycle->log;

    if (ngx_ssl_create(&wcf->ssl_conf.ssl,
                       NGX_SSL_TLSv1
                       |NGX_SSL_TLSv1_1
                       |NGX_SSL_TLSv1_2
                       |NGX_SSL_TLSv1_3,
                       NULL)
        != NGX_OK)
    {
        return NGX_ERROR;
    }

    trusted_crt = &wcf->ssl_conf.trusted_certificate;

    if (trusted_crt->len
        && ngx_wasm_trusted_certificate(&wcf->ssl_conf.ssl,
                                        trusted_crt, 1)
        != NGX_OK)
    {
        return NGX_ERROR;
    }

    return NGX_OK;
}
#endif
