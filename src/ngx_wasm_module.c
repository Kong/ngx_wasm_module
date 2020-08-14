/*
 * Copyright (C) Thibault Charbonnier
 */

#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_wasm_module.h>
#include <ngx_wasm_hfuncs.h>
#include <ngx_wasm_vm.h>
#include <ngx_wasm_util.h>


#define ngx_wasm_run()                                                       \
    (!ngx_test_config &&                                                     \
     (ngx_process == NGX_PROCESS_SINGLE || \
      ngx_process == NGX_PROCESS_MASTER || \
      ngx_process == NGX_PROCESS_WORKER))


/* ngx_wasm_module */


static char *ngx_wasm_block(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static char *ngx_wasm_init_conf(ngx_cycle_t *cycle, void *conf);
static ngx_int_t ngx_wasm_init(ngx_cycle_t *cycle);


static ngx_uint_t  ngx_wasm_max_module;


static ngx_command_t  ngx_wasm_cmds[] = {

    { ngx_string("wasm"),
      NGX_MAIN_CONF|NGX_CONF_BLOCK|NGX_CONF_NOARGS,
      ngx_wasm_block,
      0,
      0,
      NULL },

    ngx_null_command
};


static ngx_core_module_t  ngx_wasm_module_ctx = {
    ngx_string("wasm"),
    NULL,
    ngx_wasm_init_conf
};


ngx_module_t  ngx_wasm_module = {
    NGX_MODULE_V1,
    &ngx_wasm_module_ctx,              /* module context */
    ngx_wasm_cmds,                     /* module directives */
    NGX_CORE_MODULE,                   /* module type */
    NULL,                              /* init master */
    ngx_wasm_init,                     /* init module */
    NULL,                              /* init process */
    NULL,                              /* init thread */
    NULL,                              /* exit thread */
    NULL,                              /* exit process */
    NULL,                              /* exit master */
    NGX_MODULE_V1_PADDING
};


static char *
ngx_wasm_block(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    char                 *rv;
    void               ***ctx;
    ngx_uint_t            i;
    ngx_conf_t            pcf;
    ngx_wasm_module_t    *m;

    if (*(void **) conf) {
        return "is duplicate";
    }

    ngx_wasm_max_module = ngx_count_modules(cf->cycle, NGX_WASM_MODULE);

    ctx = ngx_pcalloc(cf->pool, sizeof(void *));
    if (ctx == NULL) {
        return NGX_CONF_ERROR;
    }

    *ctx = ngx_pcalloc(cf->pool, ngx_wasm_max_module * sizeof(void *));
    if (*ctx == NULL) {
        return NGX_CONF_ERROR;
    }

    *(void **) conf = ctx;

    /* NGX_WASM_MODULES create_conf */

    for (i = 0; cf->cycle->modules[i]; i++) {
        if (cf->cycle->modules[i]->type != NGX_WASM_MODULE) {
            continue;
        }

        m = cf->cycle->modules[i]->ctx;

        if (m->create_conf) {
            (*ctx)[cf->cycle->modules[i]->ctx_index] =
                                                     m->create_conf(cf->cycle);
            if ((*ctx)[cf->cycle->modules[i]->ctx_index] == NULL) {
                return NGX_CONF_ERROR;
            }
        }
    }

    /* parse the wasm{} block */

    pcf = *cf;

    cf->ctx = ctx;
    cf->module_type = NGX_WASM_MODULE;
    cf->cmd_type = NGX_WASM_CONF;

    rv = ngx_conf_parse(cf, NULL);

    *cf = pcf;

    if (rv != NGX_CONF_OK) {
        return NGX_CONF_ERROR;
    }

    /* NGX_WASM_MODULES init_conf */

    for (i = 0; cf->cycle->modules[i]; i++) {
        if (cf->cycle->modules[i]->type != NGX_WASM_MODULE) {
            continue;
        }

        m = cf->cycle->modules[i]->ctx;

        if (m->init_conf) {
            rv = m->init_conf(cf->cycle,
                              (*ctx)[cf->cycle->modules[i]->ctx_index]);
            if (rv != NGX_CONF_OK) {
                return rv;
            }
        }
    }

    return NGX_CONF_OK;
}


static char *
ngx_wasm_init_conf(ngx_cycle_t *cycle, void *conf)
{
    if (ngx_wasm_core_get_vm(cycle) == NULL) {
        return NGX_CONF_ERROR;
    }

    return NGX_CONF_OK;
}


static ngx_int_t
ngx_wasm_init(ngx_cycle_t *cycle)
{
    size_t              i;
    ngx_wasm_module_t  *m;
    ngx_int_t           rc;

    /* NGX_WASM_MODULES init */

    for (i = 0; cycle->modules[i]; i++) {
        if (cycle->modules[i]->type != NGX_WASM_MODULE) {
            continue;
        }

        m = cycle->modules[i]->ctx;

        if (m->init) {
            rc = m->init(cycle);
            if (rc != NGX_OK) {
                return rc;
            }
        }
    }

    return NGX_OK;
}


/* ngx_wasm_core_module */


#define ngx_wasm_core_cycle_get_conf(cycle)                                  \
    (ngx_get_conf(cycle->conf_ctx, ngx_wasm_module))                         \
    ? (*(ngx_get_conf(cycle->conf_ctx, ngx_wasm_module)))                    \
      [ngx_wasm_core_module.ctx_index]                                       \
    : NULL


typedef struct {
    ngx_wasm_hfuncs_store_t     *hfuncs_store;
    ngx_wasm_hfuncs_resolver_t  *hfuncs_resolver;
    ngx_wrt_t                   *runtime;
    ngx_wasm_vm_t               *vm;
} ngx_wasm_core_conf_t;


static void *ngx_wasm_core_create_conf(ngx_cycle_t *cycle);
static char *ngx_wasm_core_use_directive(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
static char *ngx_wasm_core_module_directive(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
static char *ngx_wasm_core_init_conf(ngx_cycle_t *cycle, void *conf);
static ngx_int_t ngx_wasm_core_init(ngx_cycle_t *cycle);
static void ngx_wasm_core_exit_process(ngx_cycle_t *cycle);
static void ngx_wasm_core_exit_master(ngx_cycle_t *cycle);


static ngx_command_t  ngx_wasm_core_commands[] = {

    { ngx_string("use"),
      NGX_WASM_CONF|NGX_CONF_TAKE1,
      ngx_wasm_core_use_directive,
      0,
      0,
      NULL },

    { ngx_string("module"),
      NGX_WASM_CONF|NGX_CONF_TAKE2,
      ngx_wasm_core_module_directive,
      0,
      0,
      NULL },

      ngx_null_command
};


static ngx_wasm_module_t  ngx_wasm_core_module_ctx = {
    NULL,                                  /* runtime */
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
    if (ngx_process == NGX_PROCESS_MASTER) {
        ngx_wasm_core_exit_process((ngx_cycle_t *) data);
    }
}
#endif


static void *
ngx_wasm_core_create_conf(ngx_cycle_t *cycle)
{
    static const ngx_str_t core_vm_name = ngx_string("core");
    ngx_wasm_core_conf_t  *wcf;
#ifdef NGX_WASM_NOPOOL
    ngx_pool_cleanup_t    *cln;
#endif

    wcf = ngx_palloc(cycle->pool, sizeof(ngx_wasm_core_conf_t));
    if (wcf == NULL) {
        return NULL;
    }

    wcf->runtime = NGX_CONF_UNSET_PTR;
    wcf->hfuncs_resolver = NULL;

    wcf->hfuncs_store = ngx_wasm_hfuncs_store_new(cycle);
    if (wcf->hfuncs_store == NULL) {
        return NULL;
    }

    wcf->vm = ngx_wasm_vm_new(cycle, (ngx_str_t *) &core_vm_name);
    if (wcf->vm == NULL) {
        return NULL;
    }

#ifdef NGX_WASM_NOPOOL
    /* ensure hfuncs and vm are freed in master HUP reloads for Valgrind */
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
ngx_wasm_core_use_directive(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_uint_t             i;
    ngx_str_t             *value;
    ngx_wasm_module_t     *m;
    ngx_wasm_core_conf_t  *wcf = conf;

    if (wcf->runtime != NGX_CONF_UNSET_PTR) {
        return "is duplicate";
    }

    value = cf->args->elts;

    for (i = 0; cf->cycle->modules[i]; i++) {
        if (cf->cycle->modules[i]->type != NGX_WASM_MODULE) {
            continue;
        }

        m = cf->cycle->modules[i]->ctx;

        if (m->runtime == NULL) {
            continue;
        }

        if (ngx_strncmp(m->runtime->name.data, value[1].data,
                        m->runtime->name.len) == 0)
        {
            wcf->runtime = m->runtime;

            return NGX_CONF_OK;
        }
    }

    ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                       "invalid wasm runtime \"%V\"", &value[1]);

    return NGX_CONF_ERROR;
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

    rc = ngx_wasm_vm_add_module(wcf->vm, name, path);
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
    ngx_uint_t             i;
    ngx_module_t          *m = NULL;
    ngx_wasm_module_t     *wm;
    ngx_wasm_core_conf_t  *wcf = conf;
    ngx_wrt_t             *default_runtime = NULL;

    for (i = 0; cycle->modules[i]; i++) {
        if (cycle->modules[i]->type != NGX_WASM_MODULE) {
            continue;
        }

        wm = cycle->modules[i]->ctx;

        if (wm->runtime == NULL) {
            continue;
        }

        if (ngx_strcmp(wm->runtime->name.data, NGX_WRT_DEFAULT) == 0) {
            default_runtime = wm->runtime;
        }

        m = cycle->modules[i];
        break;
    }

    if (m == NULL) {
        ngx_wasm_log_error(NGX_LOG_EMERG, cycle->log, 0,
                           "no NGX_WASM_MODULE found");
        return NGX_CONF_ERROR;
    }

    if (default_runtime == NULL) {
        ngx_wasm_log_error(NGX_LOG_EMERG, cycle->log, 0,
                           "missing default NGX_WASM_MODULE \"%s\"",
                           NGX_WRT_DEFAULT);
        return NGX_CONF_ERROR;
    }

    ngx_conf_init_ptr_value(wcf->runtime, default_runtime);

    if (ngx_wasm_run()) {
        ngx_log_error(NGX_LOG_NOTICE, cycle->log, 0,
                      "using the \"%V\" wasm runtime", &wcf->runtime->name);
    }

    return NGX_CONF_OK;
}


static ngx_int_t
ngx_wasm_core_init(ngx_cycle_t *cycle)
{
    ngx_wasm_core_conf_t    *wcf;

    wcf = ngx_wasm_core_cycle_get_conf(cycle);

    wcf->hfuncs_resolver = ngx_wasm_hfuncs_resolver_new(cycle,
                               wcf->hfuncs_store, wcf->runtime);
    if (wcf->hfuncs_resolver == NULL) {
        return NGX_ERROR;
    }

    ngx_wasm_hfuncs_store_free(wcf->hfuncs_store);
    wcf->hfuncs_store = NULL;

    if (ngx_wasm_vm_init(wcf->vm, wcf->hfuncs_resolver, wcf->runtime)
        != NGX_OK)
    {
        return NGX_ERROR;
    }

    if (ngx_wasm_vm_load_modules(wcf->vm) != NGX_OK) {
        return NGX_ERROR;
    }

    return NGX_OK;
}


static void
ngx_wasm_core_exit_process(ngx_cycle_t *cycle)
{
    ngx_wasm_core_conf_t  *wcf;

    wcf = ngx_wasm_core_cycle_get_conf(cycle);

    if (wcf->vm) {
        ngx_wasm_vm_free(wcf->vm);
        wcf->vm = NULL;
    }

    if (wcf->hfuncs_resolver) {
        ngx_wasm_hfuncs_resolver_free(wcf->hfuncs_resolver);
        wcf->hfuncs_resolver = NULL;
    }

    if (wcf->hfuncs_store) {
        ngx_wasm_hfuncs_store_free(wcf->hfuncs_store);
        wcf->hfuncs_store = NULL;
    }
}


static void
ngx_wasm_core_exit_master(ngx_cycle_t *cycle)
{
    ngx_wasm_core_exit_process(cycle);
}


ngx_inline ngx_wasm_vm_t *
ngx_wasm_core_get_vm(ngx_cycle_t *cycle)
{
    ngx_wasm_core_conf_t  *wcf;

    wcf = ngx_wasm_core_cycle_get_conf(cycle);
    if (wcf == NULL) {
        ngx_log_error(NGX_LOG_EMERG, cycle->log, 0,
                      "no \"wasm\" section in configuration");
        return NULL;
    }

    return wcf->vm;
}


void
ngx_wasm_core_hfuncs_add(ngx_cycle_t *cycle,
    const ngx_wasm_hfunc_decl_t decls[])
{
    ngx_wasm_core_conf_t  *wcf;

    wcf = ngx_wasm_core_cycle_get_conf(cycle);
    if (wcf == NULL) {
        ngx_log_error(NGX_LOG_EMERG, cycle->log, 0,
                      "no \"wasm\" section in configuration");
        return;
    }

    ngx_wasm_hfuncs_store_add(wcf->hfuncs_store, "env", decls);
}
