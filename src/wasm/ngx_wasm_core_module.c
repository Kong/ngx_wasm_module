#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_wavm.h>
#include <ngx_wasm_shm.h>


static void *ngx_wasm_core_create_conf(ngx_cycle_t *cycle);
static char *ngx_wasm_core_module_directive(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
static char *ngx_wasm_core_shm_kv_directive(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
static char *ngx_wasm_core_shm_queue_directive(ngx_conf_t *cf, ngx_command_t *cmd,
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

    /* element: ngx_wasm_shm_mapping_t */
    ngx_array_t                        shms;

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

    { ngx_string("shm_kv"),
      NGX_WASM_CONF|NGX_CONF_TAKE23,
      ngx_wasm_core_shm_kv_directive,
      0,
      0,
      NULL },

    { ngx_string("shm_queue"),
      NGX_WASM_CONF|NGX_CONF_TAKE23,
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
    ngx_wasm_assert(wcf);

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
    ngx_wasm_core_conf_t    *wcf;
    ngx_pool_cleanup_t      *cln;
    static const ngx_str_t   vm_name = ngx_string("main");

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

    /*
     * Passing zero to ngx_array_init prevents future `ngx_array_push` calls
     * from allocating memory and causes silent pool memory corruption
     */

    if (ngx_array_init(&wcf->shms, cycle->pool,
                       1, sizeof(ngx_wasm_shm_mapping_t))
        != NGX_OK)
    {
        return NULL;
    }

#if (NGX_SSL)
    wcf->ssl_conf.verify_cert = NGX_CONF_UNSET;
    wcf->ssl_conf.verify_host = NGX_CONF_UNSET;
    wcf->ssl_conf.no_verify_warn = NGX_CONF_UNSET;
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
ngx_wasm_core_shm_generic_directive(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf, ngx_wasm_shm_type_e type)
{
    ngx_uint_t               i;
    ngx_int_t                size;
    ngx_str_t               *value, *name;
    ngx_wasm_core_conf_t    *wcf = conf;
    ngx_wasm_shm_mapping_t  *mapping;
    ngx_wasm_shm_t          *shm;
    ngx_int_t                min_size = 3 * ngx_pagesize;

    value = cf->args->elts;
    name = &value[1];
    size = ngx_parse_size(&value[2]);

    if (name->len == 0) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "[wasm] invalid shm name \"%V\"", name);
        return NGX_CONF_ERROR;
    }

    if (size == NGX_ERROR) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "[wasm] invalid shm size \"%V\"", &value[2]);
        return NGX_CONF_ERROR;
    }

    if (size < min_size) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "[wasm] shm size of %d bytes is too small, minimum required is %d bytes",
                           size, min_size);
        return NGX_CONF_ERROR;
    }

    if ((size & (ngx_pagesize - 1)) != 0) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "[wasm] shm size of %d bytes is not page-aligned, must be a multiple of %d",
                           size, ngx_pagesize);
        return NGX_CONF_ERROR;
    }

    shm = ngx_pcalloc(cf->pool, sizeof(ngx_wasm_shm_t));
    if (!shm) {
        return NGX_CONF_ERROR;
    }

    shm->type = type;
    shm->name = *name;
    shm->log = cf->cycle->log;

    mapping = wcf->shms.elts;

    for (i = 0; i < wcf->shms.nelts; i++) {
        if (ngx_str_eq(mapping[i].name.data, mapping[i].name.len,
                       name->data, name->len))
        {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "[wasm] \"%V\" shm already defined", name);
            return NGX_CONF_ERROR;
        }
    }

    mapping = ngx_array_push(&wcf->shms);
    if (!mapping) {
        return NGX_CONF_ERROR;
    }

    mapping->name = *name;
    mapping->zone = ngx_shared_memory_add(cf, name, size, &ngx_wasm_module);
    if (!mapping->zone) {
        return NGX_CONF_ERROR;
    }

    mapping->zone->init = ngx_wasm_shm_init_zone;
    mapping->zone->data = shm;

    return NGX_CONF_OK;
}


static char *
ngx_wasm_core_shm_kv_directive(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    return ngx_wasm_core_shm_generic_directive(cf, cmd,
                                               conf, NGX_WASM_SHM_TYPE_KV);
}


static char *
ngx_wasm_core_shm_queue_directive(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf)
{
    return ngx_wasm_core_shm_generic_directive(cf, cmd,
                                               conf, NGX_WASM_SHM_TYPE_QUEUE);
}


static char *
ngx_wasm_core_init_conf(ngx_cycle_t *cycle, void *conf)
{
#if (NGX_SSL)
    ngx_wasm_core_conf_t  *wcf = conf;

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

    return NGX_CONF_OK;
}


static ngx_int_t
ngx_wasm_core_init(ngx_cycle_t *cycle)
{
    ngx_wavm_t            *vm;
    ngx_wasm_core_conf_t  *wcf;

    wcf = ngx_wasm_core_cycle_get_conf(cycle);
    if (wcf == NULL) {
        return NGX_OK;
    }

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
    ngx_wasm_assert(wcf);

    return &wcf->ssl_conf;
}


static ngx_int_t
ngx_wasm_core_init_ssl(ngx_cycle_t *cycle)
{
    ngx_wasm_core_conf_t  *wcf;
    ngx_str_t             *trusted_crt;

    wcf = ngx_wasm_core_cycle_get_conf(cycle);
    ngx_wasm_assert(wcf);

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

    ngx_log_debug0(NGX_LOG_DEBUG_WASM, cycle->log, 0, "tls initialized");

    return NGX_OK;
}
#endif
