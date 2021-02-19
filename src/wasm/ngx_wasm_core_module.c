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
static void ngx_wasm_core_exit_process(ngx_cycle_t *cycle);
static void ngx_wasm_core_exit_master(ngx_cycle_t *cycle);

extern ngx_wavm_host_def_t  ngx_wasm_core_interface;


typedef struct {
    ngx_wavm_t                        *vm;
} ngx_wasm_core_conf_t;


static ngx_command_t  ngx_wasm_core_commands[] = {

    { ngx_string("module"),
      NGX_WASM_CONF|NGX_CONF_TAKE2,
      ngx_wasm_core_module_directive,
      0,
      0,
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
    ngx_wasm_core_exit_process,            /* exit process */
    ngx_wasm_core_exit_master,             /* exit master */
    NGX_MODULE_V1_PADDING
};


static void
ngx_wasm_core_cleanup_pool(void *data)
{
    ngx_cycle_t  *cycle = data;

    ngx_wasm_core_exit_process(cycle);
}


static void *
ngx_wasm_core_create_conf(ngx_cycle_t *cycle)
{
    static const ngx_str_t vm_name = ngx_string("main");
    ngx_wasm_core_conf_t  *wcf;
    ngx_pool_cleanup_t    *cln;

    wcf = ngx_palloc(cycle->pool, sizeof(ngx_wasm_core_conf_t));
    if (wcf == NULL) {
        return NULL;
    }

    wcf->vm = ngx_wavm_create(cycle, &vm_name, &ngx_wasm_core_interface);
    if (wcf->vm == NULL) {
        return NULL;
    }

    cln = ngx_pool_cleanup_add(cycle->pool, 0);
    if (cln == NULL) {
        return NULL;
    }

    cln->handler = ngx_wasm_core_cleanup_pool;
    cln->data = cycle;

    return wcf;
}


static char *
ngx_wasm_core_module_directive(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_int_t                rc;
    ngx_str_t               *value, *name, *path;
    ngx_wasm_core_conf_t    *wcf = conf;

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
                               "[wasm] module \"%V\" already defined", name);
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
    ngx_wavm_t       *vm;

    ngx_wasm_log_error(NGX_LOG_NOTICE, cycle->log, 0,
                       "wasm core INIT");

    vm = ngx_wasm_main_vm(cycle);
    if (vm == NULL) {
        return NGX_OK;
    }

    if (ngx_wavm_init(vm) != NGX_OK) {
        return NGX_ERROR;
    }

    return NGX_OK;
}


static ngx_int_t
ngx_wasm_core_init_process(ngx_cycle_t *cycle)
{
    ngx_wavm_t  *vm;

    ngx_wasm_log_error(NGX_LOG_NOTICE, ngx_cycle->log, 0,
                       "wasm core INIT PROCESS");

    vm = ngx_wasm_main_vm(cycle);
    if (vm == NULL) {
        return NGX_OK;
    }

    ngx_wavm_load(vm);

    return NGX_OK;
}


static void
ngx_wasm_core_exit_process(ngx_cycle_t *cycle)
{
    ngx_wasm_core_conf_t  *wcf;

    wcf = ngx_wasm_core_cycle_get_conf(cycle);
    if (wcf && wcf->vm) {
        ngx_wavm_destroy(wcf->vm);
        wcf->vm = NULL;
    }
}


static void
ngx_wasm_core_exit_master(ngx_cycle_t *cycle)
{
    ngx_wasm_core_exit_process(cycle);
}


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
