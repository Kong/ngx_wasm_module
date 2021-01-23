#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_wavm.h>


extern ngx_wavm_hfunc_def_t  ngx_wasm_core_hfuncs[];


#define ngx_wasm_core_check_process()                                        \
    (!ngx_test_config &&                                                     \
     (ngx_process == NGX_PROCESS_SINGLE ||                                   \
      ngx_process == NGX_PROCESS_MASTER ||                                   \
      ngx_process == NGX_PROCESS_WORKER))


typedef struct {
    ngx_wavm_t                        *vm;
    ngx_wavm_hfuncs_t                 *hfuncs;
} ngx_wasm_core_conf_t;


static void *ngx_wasm_core_create_conf(ngx_cycle_t *cycle);
static char *ngx_wasm_core_module_directive(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
static char *ngx_wasm_core_init_conf(ngx_cycle_t *cycle, void *conf);
static ngx_int_t ngx_wasm_core_init(ngx_cycle_t *cycle);
static void ngx_wasm_core_exit_process(ngx_cycle_t *cycle);
static void ngx_wasm_core_exit_master(ngx_cycle_t *cycle);


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
    NULL,                                  /* hfuncs */
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
    NULL,                                  /* init process */
    NULL,                                  /* init thread */
    NULL,                                  /* exit thread */
    ngx_wasm_core_exit_process,            /* exit process */
    ngx_wasm_core_exit_master,             /* exit master */
    NGX_MODULE_V1_PADDING
};


#ifdef NGX_WASM_NOPOOL
static void
ngx_wasm_core_cleanup_pool(void *data)
{
    ngx_cycle_t  *cycle = data;
    unsigned      run = (ngx_process == NGX_PROCESS_MASTER ||
                         ngx_process == NGX_PROCESS_SINGLE);

    ngx_log_debug1(NGX_LOG_DEBUG_WASM, cycle->log, 0,
                  "wasm core: nopool patch cycle->pool cleanup: %s",
                  run ? "yes" : "no");

    if (run) {
        ngx_wasm_core_exit_process(cycle);
    }
}
#endif


static void *
ngx_wasm_core_create_conf(ngx_cycle_t *cycle)
{
    static const ngx_str_t vm_name = ngx_string("main");
    ngx_wasm_core_conf_t  *wcf;
#ifdef NGX_WASM_NOPOOL
    ngx_pool_cleanup_t    *cln;
#endif

    wcf = ngx_palloc(cycle->pool, sizeof(ngx_wasm_core_conf_t));
    if (wcf == NULL) {
        return NULL;
    }

    wcf->vm = ngx_wavm_new(cycle, &vm_name);
    if (wcf->vm == NULL) {
        return NULL;
    }

    wcf->hfuncs = ngx_wavm_hfuncs_new(cycle);
    if (wcf->hfuncs == NULL) {
        return NULL;
    }

#ifdef NGX_WASM_NOPOOL
    /* ensure vm is freed in master HUP reloads for Valgrind */
    cln = ngx_pool_cleanup_add(cycle->pool, 0);
    if (cln == NULL) {
        return NULL;
    }

    cln->handler = ngx_wasm_core_cleanup_pool;
    cln->data = cycle;
#endif

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
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "invalid module name \"%V\"",
                           name);
        return NGX_CONF_ERROR;
    }

    if (path->len == 0) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "invalid module path \"%V\"",
                           path);
        return NGX_CONF_ERROR;
    }

    rc = ngx_wavm_module_add(wcf->vm, name, path);
    if (rc != NGX_OK) {
        if (rc == NGX_DECLINED) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "[wasm] module \"%V\" already defined", name);
        }

        /* rc == NGX_ERROR */

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
    ngx_uint_t             i;
    ngx_wasm_module_t     *wm;
    ngx_wasm_core_conf_t  *wcf;

    wcf = ngx_wasm_core_cycle_get_conf(cycle);
    if (wcf == NULL) {
        return NGX_OK;
    }

    ngx_wasm_log_error(NGX_LOG_INFO, cycle->log, 0,
                       "registering host functions", wcf->vm->name);

    for (i = 0; cycle->modules[i]; i++) {
        if (cycle->modules[i]->type != NGX_WASM_MODULE) {
            continue;
        }

        wm = cycle->modules[i]->ctx;

        if (wm->hfuncs) {
            ngx_wavm_hfuncs_add(wcf->hfuncs, wm->hfuncs, cycle->modules[i]);
        }
    }

    ngx_wavm_hfuncs_add(wcf->hfuncs, ngx_wasm_core_hfuncs, NULL);

    if (ngx_wavm_init(wcf->vm, wcf->hfuncs) != NGX_OK) {
        return NGX_ERROR;
    }

    return NGX_OK;
}


static void
ngx_wasm_core_exit_process(ngx_cycle_t *cycle)
{
    ngx_wasm_core_conf_t  *wcf;

    wcf = ngx_wasm_core_cycle_get_conf(cycle);
    if (wcf) {
        if (wcf->vm) {
            ngx_wavm_free(wcf->vm);
            wcf->vm = NULL;
        }

        if (wcf->hfuncs) {
            ngx_wavm_hfuncs_free(wcf->hfuncs);
            wcf->hfuncs = NULL;
        }
    }
}


static void
ngx_wasm_core_exit_master(ngx_cycle_t *cycle)
{
    ngx_wasm_core_exit_process(cycle);
}


ngx_inline ngx_wavm_t *
ngx_wasm_core_get_vm(ngx_cycle_t *cycle)
{
    ngx_wasm_core_conf_t  *wcf;

    wcf = ngx_wasm_core_cycle_get_conf(cycle);
    if (wcf == NULL) {
        //ngx_log_error(NGX_LOG_EMERG, cycle->log, 0,
        //              "no \"wasm\" section in configuration");
        return NULL;
    }

    return wcf->vm;
}
