/*
 * Copyright (C) Thibault Charbonnier
 */

#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_config.h>
#include <ngx_wasm.h>


#define ngx_wasm_core_cycle_get_conf(cycle)                             \
    (*(ngx_get_conf(cycle->conf_ctx, ngx_wasm_module)))                 \
    [ngx_wasm_core_module.ctx_index]


static char *ngx_wasm_block(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static char *ngx_wasm_init_conf(ngx_cycle_t *cycle, void *conf);
static void *ngx_wasm_core_create_conf(ngx_cycle_t *cycle);
static char *ngx_wasm_core_use_directive(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
static char *ngx_wasm_core_module_directive(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
static char *ngx_wasm_core_init_conf(ngx_cycle_t *cycle, void *conf);
static ngx_int_t ngx_wasm_core_init(ngx_cycle_t *cycle);
static ngx_int_t ngx_wasm_core_add_module(ngx_wasm_core_conf_t *wcf,
    ngx_wasm_wmodule_t *wmodule);


/* ngx_wasm_module */


static ngx_uint_t         ngx_wasm_max_module;


static ngx_command_t      ngx_wasm_cmds[] = {

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


ngx_module_t              ngx_wasm_module = {
    NGX_MODULE_V1,
    &ngx_wasm_module_ctx,              /* module context */
    ngx_wasm_cmds,                     /* module directives */
    NGX_CORE_MODULE,                   /* module type */
    NULL,                              /* init master */
    NULL,                              /* init module */
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
    char                        *rv;
    void                      ***ctx;
    ngx_uint_t                   i;
    ngx_conf_t                   pcf;
    ngx_wasm_module_t           *m;

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
    if (ngx_get_conf(cycle->conf_ctx, ngx_wasm_module) == NULL) {
        ngx_log_error(NGX_LOG_EMERG, cycle->log, 0,
                      "no \"wasm\" section in configuration");
        return NGX_CONF_ERROR;
    }

    return NGX_CONF_OK;
}


/* ngx_wasm_core_module */


static ngx_str_t                wasm_core_name = ngx_string("wasm_core");
static ngx_wasm_vm_actions_t    ngx_wasm_vm_actions;


static ngx_command_t      ngx_wasm_core_commands[] = {

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
    &wasm_core_name,
    ngx_wasm_core_create_conf,                      /* create configuration */
    ngx_wasm_core_init_conf,                        /* init configuration */
    NULL,                                           /* init module */
    NGX_WASM_NO_VM_ACTIONS                          /* vm actions */
};


ngx_module_t              ngx_wasm_core_module = {
    NGX_MODULE_V1,
    &ngx_wasm_core_module_ctx,             /* module context */
    ngx_wasm_core_commands,                /* module directives */
    NGX_WASM_MODULE,                       /* module type */
    NULL,                                  /* init master */
    ngx_wasm_core_init,                    /* init module */
    NULL,                                  /* init process */
    NULL,                                  /* init thread */
    NULL,                                  /* exit thread */
    NULL,                                  /* exit process */
    NULL,                                  /* exit master */
    NGX_MODULE_V1_PADDING
};


static void *
ngx_wasm_core_create_conf(ngx_cycle_t *cycle)
{
    ngx_wasm_core_conf_t    *wcf;

    wcf = ngx_pcalloc(cycle->pool, sizeof(ngx_wasm_core_conf_t));
    if (wcf == NULL) {
        return NULL;
    }

    wcf->vm = NGX_CONF_UNSET_UINT;
    wcf->vm_name = NGX_CONF_UNSET_PTR;
    wcf->wmodules = ngx_array_create(cycle->pool, 2,
                                     sizeof(ngx_wasm_wmodule_t *));
    if (wcf->wmodules == NULL) {
        return NULL;
    }

    return wcf;
}


static char *
ngx_wasm_core_use_directive(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_wasm_core_conf_t    *wcf = conf;
    ngx_wasm_module_t       *m;
    ngx_str_t               *value;
    ngx_uint_t               i;

    if (wcf->vm != NGX_CONF_UNSET_UINT) {
        return "is duplicate";
    }

    value = cf->args->elts;

    for (i = 0; cf->cycle->modules[i]; i++) {
        if (cf->cycle->modules[i]->type != NGX_WASM_MODULE) {
            continue;
        }

        m = cf->cycle->modules[i]->ctx;

        if (ngx_strcmp(m->name->data, value[1].data) == 0) {
            wcf->vm = i;
            wcf->vm_name = m->name->data;

            return NGX_CONF_OK;
        }
    }

    ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                       "invalid wasm vm \"%V\"", &value[1]);

    return NGX_CONF_ERROR;
}


static char *
ngx_wasm_core_module_directive(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_wasm_core_conf_t    *wcf = conf;
    ngx_str_t               *value, *name, *path;
    ngx_wasm_wmodule_t      *wmodule;

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

    wmodule = ngx_wasm_get_module(cf->cycle, (char *) name->data);
    if (wmodule != NULL) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "module \"%V\" already defined", name);
        return NGX_CONF_ERROR;
    }

    wmodule = ngx_wasm_new_module((char *) name->data, (char *) path->data,
                                  cf->pool, cf->log);
    if (wmodule == NULL) {
        return NGX_CONF_ERROR;
    }

    if (ngx_wasm_core_add_module(wcf, wmodule) != NGX_OK) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "failed to register module");
        return NGX_CONF_ERROR;
    }

    return NGX_CONF_OK;
}


static char *
ngx_wasm_core_init_conf(ngx_cycle_t *cycle, void *conf)
{
    ngx_wasm_core_conf_t    *wcf = conf;
    ngx_uint_t               i, default_wmodule_index;
    ngx_module_t            *m;
    ngx_wasm_module_t       *wm;

    for (i = 0; cycle->modules[i]; i++) {
        if (cycle->modules[i]->type != NGX_WASM_MODULE) {
            continue;
        }

        wm = cycle->modules[i]->ctx;

        if (ngx_strcmp(wm->name->data, wasm_core_name.data) == 0) {
            continue;
        }

        if (ngx_strcmp(wm->name->data, NGX_WASM_DEFAULT_VM) == 0) {
            default_wmodule_index = i;
        }

        m = cycle->modules[i];
        break;
    }

    if (m == NULL) {
        ngx_log_error(NGX_LOG_EMERG, cycle->log, 0,
                      "no wasm nginx module found");
        return NGX_CONF_ERROR;
    }

    ngx_conf_init_uint_value(wcf->vm, default_wmodule_index);
    ngx_conf_init_ptr_value(wcf->vm_name, ((ngx_wasm_module_t *)
        cycle->modules[default_wmodule_index]->ctx)->name->data);

    ngx_log_error(NGX_LOG_NOTICE, cycle->log, 0,
                  "using the \"%s\" wasm vm", wcf->vm_name);

    return NGX_CONF_OK;
}


static ngx_int_t
ngx_wasm_core_init(ngx_cycle_t *cycle)
{
    ngx_uint_t               i;
    ngx_wasm_core_conf_t    *wcf;
    ngx_wasm_module_t       *m;
    ngx_wasm_vm_actions_t   *vma = NULL, **vmap;
    ngx_wasm_wmodule_t     **pwmodule;

    vmap = &vma;

    wcf = ngx_wasm_core_cycle_get_conf(cycle);

    /* NGX_WASM_MODULES init */

    for (i = 0; cycle->modules[i]; i++) {
        if (cycle->modules[i]->type != NGX_WASM_MODULE) {
            continue;
        }

        m = cycle->modules[i]->ctx;

        if (m->init) {
            if (m->init(cycle, vmap) != NGX_OK) {
                return NGX_ERROR;
            }

            if (i == wcf->vm) {
                ngx_wasm_vm_actions = *vma;
            }
        }
    }

    pwmodule = wcf->wmodules->elts;

    for (i = 0; i < wcf->wmodules->nelts; i++) {
        if (ngx_wasm_load_module(pwmodule[i], cycle) != NGX_OK) {
            return NGX_ERROR;
        }
    }

    return NGX_OK;
}


static ngx_int_t
ngx_wasm_core_add_module(ngx_wasm_core_conf_t *wcf, ngx_wasm_wmodule_t *wmodule)
{
    ngx_wasm_wmodule_t      **pwmodule;

    ngx_wasm_assert(wcf->wmodules != NULL);

    pwmodule = ngx_array_push(wcf->wmodules);
    if (pwmodule == NULL) {
        return NGX_ERROR;
    }

    *pwmodule = wmodule;

    return NGX_OK;
}


ngx_wasm_wmodule_t *
ngx_wasm_get_module(ngx_cycle_t *cycle, const char *name)
{
    ngx_wasm_core_conf_t    *wcf;
    ngx_wasm_wmodule_t      *wmodule = NULL, **pwmodule = NULL;
    ngx_uint_t               i;

    wcf = ngx_wasm_core_cycle_get_conf(cycle);

    ngx_wasm_assert(wcf->wmodules != NULL);

    pwmodule = wcf->wmodules->elts;

    for (i = 0; i < wcf->wmodules->nelts; i++) {
        if (ngx_strcmp(pwmodule[i]->name.data, name) == 0) {
            wmodule = pwmodule[i];
            break;
        }
    }

    return wmodule;
}


ngx_wasm_wmodule_t *
ngx_wasm_new_module(const char *name, const char *path, ngx_pool_t *pool,
    ngx_log_t *log)
{
    ngx_wasm_wmodule_t          *wmodule;
    u_char                      *p;

    ngx_wasm_assert(name != NULL && path != NULL);

    wmodule = ngx_palloc(pool, sizeof(ngx_wasm_wmodule_t));
    if (wmodule == NULL) {
        return NULL;
    }

    wmodule->log = log;
    wmodule->pool = pool;
    wmodule->data = NULL;
    wmodule->state = 0;

    wmodule->name.len = ngx_strlen(name);
    wmodule->name.data = ngx_palloc(pool, wmodule->name.len + 1);
    if (wmodule->name.data == NULL) {
        goto failed;
    }

    p = ngx_copy(wmodule->name.data, name, wmodule->name.len);
    *p = '\0';

    wmodule->path.len = ngx_strlen(path);
    wmodule->path.data = ngx_palloc(pool, wmodule->path.len + 1);
    if (wmodule->path.data == NULL) {
        goto failed;
    }

    p = ngx_copy(wmodule->path.data, path, wmodule->path.len);
    *p = '\0';

    if (ngx_strcmp(&wmodule->path.data[wmodule->path.len - 4], ".wat") == 0) {
        wmodule->state = NGX_WASM_WMODULE_ISWAT;
    }

    return wmodule;

failed:

    ngx_wasm_free_module(wmodule);

    return NULL;
}


ngx_int_t
ngx_wasm_load_module(ngx_wasm_wmodule_t *wmodule, ngx_cycle_t *cycle)
{
    ngx_uint_t      iswat = (wmodule->state & NGX_WASM_WMODULE_ISWAT) != 0;

    ngx_wasm_log_error(NGX_LOG_NOTICE, wmodule->log, 0,
                       "loading module \"%V\" at \"%V\"",
                       &wmodule->name, &wmodule->path);

    wmodule->data = ngx_wasm_vm_actions.new_module(&wmodule->path,
                                                   wmodule->pool,
                                                   wmodule->log,
                                                   cycle, iswat);
    if (wmodule->data == NULL) {
        return NGX_ERROR;
    }

    wmodule->state |= NGX_WASM_WMODULE_LOADED;

    return NGX_OK;
}


void
ngx_wasm_free_module(ngx_wasm_wmodule_t *wmodule)
{
    if (wmodule->name.data != NULL) {
        ngx_pfree(wmodule->pool, wmodule->name.data);
    }

    if (wmodule->path.data != NULL) {
        ngx_pfree(wmodule->pool, wmodule->path.data);
    }

    if (wmodule->data != NULL) {
        ngx_wasm_vm_actions.free_module(wmodule->data);
    }

    ngx_pfree(wmodule->pool, wmodule);
}


ngx_wasm_winstance_t *
ngx_wasm_new_instance(ngx_wasm_wmodule_t *wmodule)
{
    ngx_wasm_winstance_t      *winstance;
    wasm_trap_t               *wtrap = NULL;

    if (!(wmodule->state & NGX_WASM_WMODULE_LOADED)) {
        ngx_wasm_log_error(NGX_LOG_ERR, wmodule->log, 0,
                           "module \"%V\" is not loaded", wmodule->name);
        return NULL;
    }

    winstance = ngx_palloc(wmodule->pool, sizeof(ngx_wasm_winstance_t));
    if (winstance == NULL) {
        return NULL;
    }

    winstance->wmod_name = &wmodule->name;
    winstance->log = wmodule->log;
    winstance->pool = wmodule->pool;

    ngx_wasm_log_error(NGX_LOG_INFO, wmodule->log, 0,
                       "creating instance from \"%V\" module", wmodule->name);

    winstance->data = ngx_wasm_vm_actions.new_instance(wmodule->data, wtrap);
    if (winstance->data == NULL) {
        if (wtrap != NULL) {
            ngx_wasm_log_trap(NGX_LOG_ERR, wmodule->log, wtrap,
                              "failed to create instance of \"%V\" module",
                              wmodule->name);
        }

        goto failed;
    }

    return winstance;

failed:

    ngx_wasm_free_instance(winstance);

    return NULL;
}


ngx_int_t
ngx_wasm_call_instance(ngx_wasm_winstance_t *winstance, const char *fname,
    const ngx_wasm_wval_t *args, size_t nargs, ngx_wasm_wval_t *rets,
    size_t nrets)
{
    ngx_int_t           rc;
    wasm_trap_t        *wtrap = NULL;

    ngx_log_debug3(NGX_LOG_DEBUG_WASM, winstance->log, 0,
                   "instance of \"%V\" (%p) calling function \"%s\"",
                   winstance->wmod_name, winstance, fname);

    rc = ngx_wasm_vm_actions.call_instance(winstance->data, fname, args, nargs,
                                           rets, nrets, wtrap);
    if (rc != NGX_OK) {
        if (rc == NGX_DECLINED) {
            ngx_wasm_log_error(NGX_LOG_ERR, winstance->log, 0,
                               "module \"%V\" has no \"%s\" function",
                               winstance->wmod_name, fname);

        } else if (rc == NGX_ERROR) {
            ngx_wasm_log_trap(NGX_LOG_ERR, winstance->log, wtrap,
                               "module \"%V\" errored in \"%s\" function",
                               winstance->wmod_name, fname);
        }

        /* rc == NGX_ABORT */
    }

    return rc;
}


void
ngx_wasm_free_instance(ngx_wasm_winstance_t *winstance)
{
    if (winstance->data != NULL) {
        ngx_wasm_vm_actions.free_instance(winstance->data);
    }

    ngx_pfree(winstance->pool, winstance);
}


/* vi:set ft=c ts=4 sw=4 et fdm=marker: */
