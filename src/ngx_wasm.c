/*
 * Copyright (C) Thibault Charbonnier
 */

#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_config.h>
#include <ngx_wasm.h>


/* ngx_wasm_module */


static char *ngx_wasm_block(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static char *ngx_wasm_init_conf(ngx_cycle_t *cycle, void *conf);


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


#define NGX_WASM_DEFAULT_VM  "wasmtime"

#define ngx_wasm_core_cycle_get_conf(cycle)                                  \
    (*(ngx_get_conf(cycle->conf_ctx, ngx_wasm_module)))                      \
    [ngx_wasm_core_module.ctx_index]

#define ngx_wasm_clear_werror(werror)                                        \
    ngx_memzero(werror, sizeof(ngx_wasm_werror_t))


typedef struct {
    ngx_uint_t                vm;
    u_char                   *vm_name;
    ngx_array_t              *wmodules; /* TODO: R/B tree */
} ngx_wasm_core_conf_t;


typedef struct {
    ngx_log_t                *orig_log;
    ngx_wasm_winstance_t     *winstance;
} ngx_wasm_vm_log_ctx_t;


static void *ngx_wasm_core_create_conf(ngx_cycle_t *cycle);
static char *ngx_wasm_core_use_directive(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
static char *ngx_wasm_core_module_directive(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
static char *ngx_wasm_core_init_conf(ngx_cycle_t *cycle, void *conf);
static ngx_int_t ngx_wasm_core_init(ngx_cycle_t *cycle);
static ngx_int_t ngx_wasm_core_get_bytes_path(ngx_wasm_wmodule_t *wmodule,
    ngx_log_t *log);
static u_char *ngx_wasm_core_log_error_handler(ngx_log_t *log, u_char *buf,
    size_t len);
static ngx_int_t ngx_wasm_core_add_module(ngx_wasm_core_conf_t *wcf,
    ngx_wasm_wmodule_t *wmodule);


static ngx_str_t  wasm_core_name = ngx_string("wasm_core");
static ngx_wasm_vm_actions_t  ngx_wasm_vm_actions;


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
    &wasm_core_name,
    ngx_wasm_core_create_conf,             /* create configuration */
    ngx_wasm_core_init_conf,               /* init configuration */
    NULL,                                  /* init module */
    NGX_WASM_NO_VM_ACTIONS                 /* vm actions */
};


ngx_module_t  ngx_wasm_core_module = {
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

    wmodule = ngx_wasm_get_module(name->data, cf->cycle);
    if (wmodule != NULL) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "[wasm] module \"%V\" already defined", name);
        return NGX_CONF_ERROR;
    }

    wmodule = ngx_wasm_new_module(name->data, path->data, cf->log);
    if (wmodule == NULL) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, ngx_errno,
                           "[wasm] failed to load module \"%V\" from \"%V\"",
                           name, path);
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
        ngx_log_error(NGX_LOG_EMERG, cycle->log, 0, "no NGX_WASM_MODULE found");
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
    ngx_wasm_werror_t        werror;

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
        if (ngx_wasm_load_module(pwmodule[i], cycle, &werror) != NGX_OK) {
            ngx_wasm_log_error(NGX_LOG_EMERG, cycle->log, 0, &werror,
                               "failed to load module \"%V\" from \"%V\"",
                               &pwmodule[i]->name, &pwmodule[i]->path);
            return NGX_ERROR;
        }
    }

    return NGX_OK;
}


static ngx_int_t
ngx_wasm_core_get_bytes_path(ngx_wasm_wmodule_t *wmodule, ngx_log_t *log)
{
    ssize_t                      n, fsize;
    ngx_fd_t                     fd;
    ngx_file_t                   file;
    ngx_int_t                    rc = NGX_ERROR;

    fd = ngx_open_file(wmodule->path.data, NGX_FILE_RDONLY, NGX_FILE_OPEN, 0);
    if (fd == NGX_INVALID_FILE) {
        return NGX_ERROR;
    }

    ngx_memzero(&file, sizeof(ngx_file_t));

    file.fd = fd;
    file.log = log;
    file.name.len = ngx_strlen(wmodule->path.data);
    file.name.data = (u_char *) wmodule->path.data;

    if (ngx_fd_info(fd, &file.info) == NGX_FILE_ERROR) {
        goto failed;
    }

    fsize = ngx_file_size(&file.info);
    wmodule->bytes.len = fsize;
    wmodule->bytes.data = ngx_palloc(wmodule->pool, fsize);
    if (wmodule->bytes.data == NULL) {
        goto failed;
    }

    //wasm_byte_vec_new_uninitialized(bytes, fsize);

    n = ngx_read_file(&file, (u_char *) wmodule->bytes.data, fsize, 0);
    if (n == NGX_ERROR) {
        goto failed;
    }

    if (n != fsize) {
        ngx_log_error(NGX_LOG_ERR, log, 0,
                      ngx_read_file_n " \"%V\" returned only "
                      "%z bytes instead of %uz", &file.name, n, fsize);
        goto failed;
    }

    rc = NGX_OK;

failed:

    if (ngx_close_file(file.fd) == NGX_FILE_ERROR) {
        ngx_log_error(NGX_LOG_ERR, log, ngx_errno,
                      ngx_close_file_n " \"%V\" failed", &file.name);
    }

    return rc;
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
ngx_wasm_get_module(const u_char *name, ngx_cycle_t *cycle)
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
ngx_wasm_new_module(const u_char *name, const u_char *path, ngx_log_t *log)
{
    ngx_pool_t                  *pool;
    ngx_wasm_wmodule_t          *wmodule;
    u_char                      *p;

    pool = ngx_create_pool(NGX_DEFAULT_POOL_SIZE, log);
    if (pool == NULL) {
        return NULL;
    }

    wmodule = ngx_pcalloc(pool, sizeof(ngx_wasm_wmodule_t));
    if (wmodule == NULL) {
        ngx_destroy_pool(pool);
        return NULL;
    }

    wmodule->pool = pool;

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
        wmodule->flags |= NGX_WASM_WMODULE_ISWAT;
    }

    if (ngx_wasm_core_get_bytes_path(wmodule, log) != NGX_OK) {
        goto failed;
    }

    ngx_wasm_assert(wmodule->bytes.data != NULL);

    wmodule->flags |= NGX_WASM_WMODULE_HASBYTES;

    return wmodule;

failed:

    ngx_wasm_free_module(wmodule);

    return NULL;
}


void
ngx_wasm_free_module(ngx_wasm_wmodule_t *wmodule)
{
    if (wmodule->vm_module != NULL) {
        ngx_wasm_vm_actions.free_module(wmodule->vm_module);
    }

    ngx_destroy_pool(wmodule->pool);
}


ngx_int_t
ngx_wasm_load_module(ngx_wasm_wmodule_t *wmodule, ngx_cycle_t *cycle,
    ngx_wasm_werror_t *werror)
{
    ngx_wasm_clear_werror(werror);

    if (wmodule->flags & NGX_WASM_WMODULE_LOADED) {
        return NGX_OK;
    }

    if (!(wmodule->flags & NGX_WASM_WMODULE_HASBYTES)) {
        ngx_wasm_log_error(NGX_LOG_ALERT, cycle->log, 0, NULL,
                           "module \"%V\" has no bytes to load",
                           &wmodule->name, &wmodule->path);
        return NGX_ABORT;
    }

    ngx_wasm_log_error(NGX_LOG_NOTICE, cycle->log, 0, NULL,
                       "loading module \"%V\" from \"%V\"",
                       &wmodule->name, &wmodule->path);

    wmodule->vm_module = ngx_wasm_vm_actions.new_module(cycle, wmodule->pool,
                                                        &wmodule->bytes,
                                                        wmodule->flags,
                                                        &werror->vm_error);
    if (wmodule->vm_module == NULL) {
        return NGX_ERROR;
    }

    wmodule->flags |= NGX_WASM_WMODULE_LOADED;

    return NGX_OK;
}


ngx_wasm_winstance_t *
ngx_wasm_new_instance(ngx_wasm_wmodule_t *wmodule, ngx_log_t *log,
    ngx_wasm_werror_t *werror)
{
    ngx_wasm_winstance_t      *winstance;
    ngx_wasm_vm_log_ctx_t     *ctx;

    ngx_wasm_clear_werror(werror);

    if (!(wmodule->flags & NGX_WASM_WMODULE_LOADED)) {
        ngx_wasm_log_error(NGX_LOG_ERR, log, 0, NULL,
                           "module \"%V\" is not loaded", &wmodule->name);
        return NULL;
    }

    winstance = ngx_palloc(wmodule->pool, sizeof(ngx_wasm_winstance_t));
    if (winstance == NULL) {
        return NULL;
    }

    winstance->module = wmodule;
    winstance->pool = wmodule->pool;

    winstance->log = ngx_palloc(wmodule->pool, sizeof(ngx_log_t));
    if (winstance->log == NULL) {
        goto failed;
    }

    winstance->log->file = log->file;
    winstance->log->next = log->next;
    winstance->log->writer = log->writer;
    winstance->log->wdata = log->wdata;
    winstance->log->log_level = log->log_level;
    winstance->log->handler = ngx_wasm_core_log_error_handler;

    ctx = ngx_palloc(wmodule->pool, sizeof(ngx_wasm_vm_log_ctx_t));
    if (ctx == NULL) {
        goto failed;
    }

    ctx->winstance = winstance;
    ctx->orig_log = log;
    winstance->log->data = ctx;

    ngx_log_debug1(NGX_LOG_DEBUG_WASM, log, 0,
                   "creating wasm instance of \"%V\" module",
                   &winstance->module->name);

    winstance->vm_instance = ngx_wasm_vm_actions.new_instance(
                                 wmodule->vm_module, &werror->vm_error,
                                 &werror->trap);
    if (winstance->vm_instance == NULL) {
        goto failed;
    }

    return winstance;

failed:

    ngx_wasm_free_instance(winstance);

    return NULL;
}


void
ngx_wasm_free_instance(ngx_wasm_winstance_t *winstance)
{
    if (winstance->vm_instance != NULL) {
        ngx_wasm_vm_actions.free_instance(winstance->vm_instance);
    }

    if (winstance->log != NULL) {
        if (winstance->log->data != NULL) {
            ngx_pfree(winstance->pool, winstance->log->data);
        }

        ngx_pfree(winstance->pool, winstance->log);
    }

    ngx_pfree(winstance->pool, winstance);
}


ngx_int_t
ngx_wasm_call_instance(ngx_wasm_winstance_t *winstance, const u_char *fname,
    const ngx_wasm_wval_t *args, size_t nargs, ngx_wasm_wval_t *rets,
    size_t nrets, ngx_wasm_werror_t *werror)
{
    ngx_wasm_clear_werror(werror);

    ngx_log_debug3(NGX_LOG_DEBUG_WASM, winstance->log, 0,
                   "wasm instance of \"%V\" module "
                   "calling function \"%s\" (instance: %p)",
                   &winstance->module->name, fname, winstance);

    return ngx_wasm_vm_actions.call_instance(winstance->vm_instance, fname,
                                             args, nargs, rets, nrets,
                                             &werror->vm_error, &werror->trap);
}


void
ngx_wasm_log_error(ngx_uint_t level, ngx_log_t *log, ngx_err_t err,
    ngx_wasm_werror_t *werror,
#if (NGX_HAVE_VARIADIC_MACROS)
    const char *fmt, ...)

#else
    const char *fmt, va_list args)
#endif
{
#if (NGX_HAVE_VARIADIC_MACROS)
    va_list            args;
#endif
    u_char            *p, *last;
    u_char             errstr[NGX_MAX_ERROR_STR];

    last = errstr + NGX_MAX_ERROR_STR;
    p = &errstr[0];

#if (NGX_HAVE_VARIADIC_MACROS)
    va_start(args, fmt);
    p = ngx_vslprintf(p, last, fmt, args);
    va_end(args);

#else
    p = ngx_vslprintf(p, last, fmt, args);
#endif

    if (werror) {
        if (ngx_wasm_vm_actions.log_error_handler) {
            p = ngx_wasm_vm_actions.log_error_handler(werror->vm_error,
                                                      werror->trap,
                                                      p, last - p);
        } else {
            ngx_wasm_assert(NULL);
            p = ngx_slprintf(p, last, " (no error log handler in vm)");
        }
    }

    ngx_log_error_core(level, log, err, "[wasm] %*s", p - errstr, errstr);
}


static u_char *
ngx_wasm_core_log_error_handler(ngx_log_t *log, u_char *buf, size_t len)
{
    u_char                  *p = buf;
    ngx_log_t               *orig_log;
    ngx_wasm_vm_log_ctx_t   *ctx;
    ngx_wasm_winstance_t    *winstance;

    ctx = log->data;
    orig_log = ctx->orig_log;
    winstance = ctx->winstance;

    if (winstance) {
        p = ngx_snprintf(buf, len, " <module: \"%V\">",
                         &winstance->module->name);
    }

    if (orig_log->handler) {
        p = orig_log->handler(orig_log, p, len);
    }

    return p;
}


/* vi:set ft=c ts=4 sw=4 et fdm=marker: */
