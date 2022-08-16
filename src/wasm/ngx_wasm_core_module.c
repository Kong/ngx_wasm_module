#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_wavm.h>


static void *ngx_wasm_core_create_conf(ngx_cycle_t *cycle);
static char *ngx_wasm_core_module_directive(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
static char *ngx_wasm_core_init_conf(ngx_cycle_t *cycle, void *conf);
static ngx_int_t ngx_wasm_core_init(ngx_cycle_t *cycle);
static ngx_int_t ngx_wasm_core_init_process(ngx_cycle_t *cycle);
#if (NGX_SSL)
static ngx_int_t ngx_wasm_core_init_ssl(ngx_cycle_t *cycle);
#endif

extern ngx_wavm_host_def_t  ngx_wasm_core_interface;


typedef struct {
    ngx_wavm_t                        *vm;
    ngx_wavm_conf_t                    vm_conf;

#if (NGX_SSL)
    ngx_wasm_ssl_conf_t                ssl_conf;
#endif
} ngx_wasm_core_conf_t;


static ngx_command_t  ngx_wasm_core_commands[] = {

    { ngx_string("compiler"),
      NGX_WASM_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_str_slot,
      0,
      offsetof(ngx_wasm_core_conf_t, vm_conf)
      + offsetof(ngx_wavm_conf_t, compiler),
      NULL },

    { ngx_string("module"),
      NGX_WASM_CONF|NGX_CONF_TAKE2,
      ngx_wasm_core_module_directive,
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

    { ngx_string("tls_skip_verify"),
      NGX_WASM_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_flag_slot,
      0,
      offsetof(ngx_wasm_core_conf_t, ssl_conf)
      + offsetof(ngx_wasm_ssl_conf_t, skip_verify),
      NULL },

    { ngx_string("tls_skip_host_check"),
      NGX_WASM_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_flag_slot,
      0,
      offsetof(ngx_wasm_core_conf_t, ssl_conf)
      + offsetof(ngx_wasm_ssl_conf_t, skip_host_check),
      NULL },
#endif

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


static void
ngx_wasm_core_cleanup_pool(void *data)
{
    ngx_cycle_t  *cycle = data;
    ngx_wavm_t   *vm = ngx_wasm_main_vm(cycle);
#if (NGX_SSL)
    ngx_ssl_t    *ssl = &ngx_wasm_core_ssl_conf(cycle)->ssl;
#endif

    ngx_wavm_destroy(vm);

#if (NGX_SSL)
    if (ssl->ctx) {
        ngx_ssl_cleanup_ctx(ssl);
    }
#endif
}


static void *
ngx_wasm_core_create_conf(ngx_cycle_t *cycle)
{
    static const ngx_str_t   vm_name = ngx_string("main");
    ngx_wasm_core_conf_t    *wcf;
    ngx_pool_cleanup_t      *cln;

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

#if (NGX_SSL)
    wcf->ssl_conf.skip_verify = NGX_CONF_UNSET;
    wcf->ssl_conf.skip_host_check = NGX_CONF_UNSET;
#endif

    return wcf;
}


static char *
ngx_wasm_core_module_directive(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_int_t              rc;
    ngx_str_t             *value, *name, *path;
    ngx_wasm_core_conf_t  *wcf = conf;

    value = cf->args->elts;
    name = &value[1];
    path = &value[2];

    if (name->len == 0) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "[wasm] invalid module name \"%V\"", name);
        return NGX_CONF_ERROR;
    }

    if (path->len == 0) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "[wasm] invalid module path \"%V\"", path);
        return NGX_CONF_ERROR;
    }

    rc = ngx_wavm_module_add(wcf->vm, name, path);
    if (rc != NGX_OK) {
        if (rc == NGX_DECLINED) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "[wasm] \"%V\" module already defined", name);
        }

        /* NGX_ERROR, NGX_ABORT */

        return NGX_CONF_ERROR;
    }

    return NGX_CONF_OK;
}


static char *
ngx_wasm_core_init_conf(ngx_cycle_t *cycle, void *conf)
{
    return NGX_CONF_OK;
}


static ngx_int_t
ngx_wasm_core_init(ngx_cycle_t *cycle)
{
    ngx_wavm_t  *vm;

    vm = ngx_wasm_main_vm(cycle);
    if (vm == NULL) {
        return NGX_OK;
    }

    if (ngx_wavm_init(vm) != NGX_OK) {
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

    ngx_wavm_load(vm);

    return NGX_OK;
}


#if (NGX_SSL)
ngx_inline ngx_wasm_ssl_conf_t *
ngx_wasm_core_ssl_conf(ngx_cycle_t *cycle)
{
    ngx_wasm_core_conf_t  *wcf;

    wcf = ngx_wasm_core_cycle_get_conf(cycle);
    ngx_wasm_assert(wcf);

    return &wcf->ssl_conf;
}


static ngx_int_t
ngx_wasm_core_init_ssl(ngx_cycle_t *cycle)
{
    ngx_wasm_core_conf_t  *wcf;
    static ngx_str_t       default_crt = ngx_string(
        "/etc/ssl/certs/ca-certificates.crt");

    wcf = ngx_wasm_core_cycle_get_conf(cycle);
    ngx_wasm_assert(wcf);

    wcf->ssl_conf.ssl.log = cycle->log;

    if (wcf->ssl_conf.trusted_certificate.data == NULL) {
        wcf->ssl_conf.trusted_certificate = default_crt;
    }

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

    if (ngx_wasm_core_load_ssl_trusted_certificate(&wcf->ssl_conf.ssl,
        &wcf->ssl_conf.trusted_certificate, 1)
        != NGX_OK)
    {
        return NGX_ERROR;
    }

    ngx_log_debug0(NGX_LOG_DEBUG_WASM, cycle->log, 0, "tls initialized");

    return NGX_OK;
}
#endif
