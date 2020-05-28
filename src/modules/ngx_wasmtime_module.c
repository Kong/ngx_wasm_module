/*
 * Copyright (C) Thibault Charbonnier
 */

#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_config.h>
#include <ngx_wasm.h>

#include <wasm.h>
#include <wasi.h>
#include <wasmtime.h>


#define ngx_wasmtime_cycle_get_conf(cycle)                                   \
    (*(ngx_get_conf(cycle->conf_ctx, ngx_wasm_module)))                      \
    [ngx_wasmtime_module.ctx_index]


static void      *ngx_wasmtime_create_conf(ngx_cycle_t *cycle);
static ngx_int_t  ngx_wasmtime_init(ngx_cycle_t *cycle);
static ngx_int_t  ngx_wasmtime_init_engine(ngx_cycle_t *cycle);
static void       ngx_wasmtime_shutdown_engine(ngx_cycle_t *cycle);
static ngx_int_t  ngx_wasmtime_init_vm(ngx_wasm_wvm_t *vm, ngx_cycle_t *cycle);
static void       ngx_wasmtime_shutdown_vm(ngx_wasm_wvm_t *vm);
static ngx_int_t  ngx_wasmtime_load_wasm_module(ngx_wasm_wmodule_t *module,
                                                ngx_wasm_wvm_t *vm,
                                                const u_char *path);
static void       ngx_wasmtime_release_module(ngx_wasm_wmodule_t *module);
static ngx_int_t  ngx_wasmtime_init_instance(ngx_wasm_winstance_t *instance,
                                             ngx_wasm_wmodule_t *module);
static void       ngx_wasmtime_shutdown_instance(ngx_wasm_winstance_t *instance);
static ngx_int_t  ngx_wasmtime_run_entrypoint(ngx_wasm_winstance_t *instance,
                                              const u_char *name);
static void       ngx_wasmtime_log_error(ngx_uint_t level, ngx_log_t *log,
                                         wasmtime_error_t *werror,
                                         wasm_trap_t *wtrap, const char *fmt,
                                         ...);


typedef struct {
    wasm_engine_t          *wasm_engine;
    //wasi_config_t          *wasi_config;
} ngx_wasmtime_conf_t;


typedef struct {
    ngx_wasmtime_conf_t    *wtcf;
    wasm_store_t           *wasm_store;
} ngx_wasmtime_vm_t;


typedef struct {
    wasm_module_t          *wasm_module;
    //wasi_instance_t        *wasi_inst;
    wasm_importtype_vec_t   wasm_imports;
    wasm_exporttype_vec_t   wasm_exports;
} ngx_wasmtime_module_t;


typedef struct {
    wasm_instance_t        *wasm_instance;
    wasm_extern_vec_t       wasm_externs;
} ngx_wasmtime_instance_t;


static ngx_str_t  module_name = ngx_string("wasmtime");


static ngx_wasm_module_t  ngx_wasmtime_module_ctx = {
    &module_name,
    ngx_wasmtime_create_conf,
    NULL,
    ngx_wasmtime_init,

    {
        ngx_wasmtime_init_vm,
        ngx_wasmtime_shutdown_vm,
        ngx_wasmtime_load_wasm_module,
        ngx_wasmtime_release_module,
        ngx_wasmtime_init_instance,
        ngx_wasmtime_shutdown_instance,
        ngx_wasmtime_run_entrypoint,
    }
};


ngx_module_t  ngx_wasmtime_module = {
    NGX_MODULE_V1,
    &ngx_wasmtime_module_ctx,            /* module context */
    NULL,                                /* module directives */
    NGX_WASM_MODULE,                     /* module type */
    NULL,                                /* init master */
    NULL,                                /* init module */
    NULL,                                /* init process */
    NULL,                                /* init thread */
    NULL,                                /* exit thread */
    NULL,                                /* exit process */
    NULL,                                /* exit master */
    NGX_MODULE_V1_PADDING
};


static void *
ngx_wasmtime_create_conf(ngx_cycle_t *cycle)
{
    ngx_wasmtime_conf_t    *wtcf;

    wtcf = ngx_pcalloc(cycle->pool, sizeof(ngx_wasmtime_conf_t));
    if (wtcf == NULL) {
        return NULL;
    }

    return wtcf;
}


static ngx_int_t
ngx_wasmtime_init(ngx_cycle_t *cycle)
{
    if (ngx_wasmtime_init_engine(cycle) != NGX_OK) {
        return NGX_ERROR;
    }

    ngx_wasm_vm_actions = ngx_wasmtime_module_ctx.vm_actions;

    return NGX_OK;
}


static ngx_int_t
ngx_wasmtime_init_engine(ngx_cycle_t *cycle)
{
    ngx_wasmtime_conf_t         *wtcf;

    wtcf = ngx_wasmtime_cycle_get_conf(cycle);

    wtcf->wasm_engine = wasm_engine_new();
    if (wtcf->wasm_engine == NULL) {
        ngx_wasmtime_log_error(NGX_LOG_ALERT, cycle->log, NULL, NULL,
                               "failed to instantiate engine");
        goto failed;
    }

    /*
    wtcf->wasi_config = wasi_config_new();
    if (wtcf->wasi_config == NULL) {
        ngx_wasmtime_log_error(NGX_LOG_ALERT, cycle->log, NULL, NULL,
                               "failed to instantiate wasi config");
        goto failed;
    }

    wasi_config_inherit_stdout(wtcf->wasi_config);
    wasi_config_inherit_stderr(wtcf->wasi_config);
    */

    return NGX_OK;

failed:

    ngx_wasmtime_shutdown_engine(cycle);

    return NGX_ERROR;
}


static void
ngx_wasmtime_shutdown_engine(ngx_cycle_t *cycle)
{
    ngx_wasmtime_conf_t         *wtcf;

    wtcf = ngx_wasmtime_cycle_get_conf(cycle);

    /* TODO: wasi_config */

    if (wtcf->wasm_engine != NULL) {
        wasm_engine_delete(wtcf->wasm_engine);
    }
}


static ngx_int_t
ngx_wasmtime_init_vm(ngx_wasm_wvm_t *vm, ngx_cycle_t *cycle)
{
    ngx_wasmtime_conf_t         *wtcf;
    ngx_wasmtime_vm_t           *wtvm;

    wtcf = ngx_wasmtime_cycle_get_conf(cycle);

    wtvm = ngx_palloc(cycle->pool, sizeof(ngx_wasmtime_vm_t));
    if (wtvm == NULL) {
        ngx_log_error(NGX_LOG_ERR, cycle->log, 0,
                      "failed to allocate wasmtime vm");
        goto failed;
    }

    vm->wvm = wtvm;
    vm->pool = cycle->pool;
    vm->log = cycle->log;

    wtvm->wasm_store = wasm_store_new(wtcf->wasm_engine);
    if (wtvm->wasm_store == NULL) {
        ngx_wasmtime_log_error(NGX_LOG_ERR, cycle->log, NULL, NULL,
                               "failed to instantiate store");
        goto failed;
    }

    wtvm->wtcf = wtcf;

    return NGX_OK;

failed:

    ngx_wasmtime_shutdown_vm(vm);

    return NGX_ERROR;
}


static void
ngx_wasmtime_shutdown_vm(ngx_wasm_wvm_t *vm)
{
    ngx_wasmtime_vm_t  *wvm = vm->wvm;

    if (wvm != NULL) {
        if (wvm->wasm_store != NULL) {
            wasm_store_delete(wvm->wasm_store);
        }

        ngx_pfree(vm->pool, wvm);
    }

    vm->pool = NULL;
    vm->log = NULL;
}


static ngx_int_t
ngx_wasmtime_load_wasm_module(ngx_wasm_wmodule_t *module, ngx_wasm_wvm_t *vm,
    const u_char *path)
{
    ngx_wasmtime_vm_t           *wtvm;
    ngx_wasmtime_module_t       *wtmodule;
    ssize_t                      fsize, n;
    ngx_fd_t                     fd;
    ngx_file_t                   file;
    wasm_byte_vec_t              wbytes;
    wasmtime_error_t            *werror;
    ngx_int_t                    rc = NGX_ERROR;

    /* wasm file */

    fd = ngx_open_file(path, NGX_FILE_RDONLY, NGX_FILE_OPEN, 0);
    if (fd == NGX_INVALID_FILE) {
        ngx_log_error(NGX_LOG_ERR, vm->log, ngx_errno,
                      ngx_open_file_n " \"%s\" failed", path);
        return NGX_ERROR;
    }

    ngx_memzero(&file, sizeof(ngx_file_t));

    file.fd = fd;
    file.log = vm->log;
    file.name.len = ngx_strlen(path);
    file.name.data = (u_char *) path;

    if (ngx_fd_info(fd, &file.info) == NGX_FILE_ERROR) {
        ngx_log_error(NGX_LOG_ERR, vm->log, ngx_errno,
                      ngx_fd_info_n " \"%s\" failed", &file.name);
        goto failed;
    }

    fsize = ngx_file_size(&file.info);
    wasm_byte_vec_new_uninitialized(&wbytes, fsize);

    n = ngx_read_file(&file, (u_char *) wbytes.data, fsize, 0);
    if (n == NGX_ERROR) {
        ngx_log_error(NGX_LOG_ERR, vm->log, ngx_errno,
                      ngx_read_file_n " \"%V\" failed", &file.name);
        goto failed;
    }

    if (n != fsize) {
        ngx_log_error(NGX_LOG_ERR, vm->log, 0,
                      ngx_read_file_n " \"%V\" returned only "
                      "%z bytes instead of %uz", &file.name, n, fsize);
        goto failed;
    }

    /* wasm module */

    wtmodule = ngx_palloc(vm->pool, sizeof(ngx_wasmtime_module_t));
    if (wtmodule == NULL) {
        ngx_log_error(NGX_LOG_ERR, vm->log, 0,
                      "failed to allocate wasmtime module");
        goto failed;
    }

    module->wmodule = wtmodule;
    module->vm = vm;
    module->path.len = file.name.len;
    module->path.data = ngx_palloc(vm->pool, file.name.len);
    if (module->path.data == NULL) {
        goto failed;
    }

    ngx_memcpy(module->path.data, path, module->path.len);

    wtvm = vm->wvm;

    werror = wasmtime_module_new(wtvm->wasm_store, &wbytes,
                                 &wtmodule->wasm_module);
    if (wtmodule->wasm_module == NULL) {
        ngx_wasmtime_log_error(NGX_LOG_ERR, vm->log, werror, NULL,
                               "failed to compile module \"%V\"",
                               &module->path);
        goto failed;
    }

    /*
    wtmodule->wasi_inst = wasi_instance_new(vm->wvm->wasm_store,
                                            "wasi_snapshot_preview1",
                                            vm->wvm->wtcf->wasi_config,
                                            &wtrap);
    if (wtcf->wasi_inst == NULL) {
        ngx_wasmtime_log_error(NGX_LOG_ERR, vm->log, NULL, wtrap,
                               "failed to instantiate wasi instance");
        goto failed;
    }
    */

    wasm_module_imports(wtmodule->wasm_module, &wtmodule->wasm_imports);
    wasm_module_exports(wtmodule->wasm_module, &wtmodule->wasm_exports);

    rc = NGX_OK;

failed:

    if (ngx_close_file(file.fd) == NGX_FILE_ERROR) {
        ngx_log_error(NGX_LOG_ERR, vm->log, ngx_errno,
                      ngx_close_file_n " \"%V\" failed", &file.name);
    }

    wasm_byte_vec_delete(&wbytes);

    if (rc != NGX_OK) {
        ngx_wasmtime_release_module(module);
    }

    return rc;
}


static void
ngx_wasmtime_release_module(ngx_wasm_wmodule_t *module)
{
    ngx_wasmtime_module_t  *wtmodule = module->wmodule;

    if (module->path.data != NULL) {
        ngx_pfree(module->vm->pool, module->path.data);
    }

    if (wtmodule != NULL) {
        /* TODO: wasi_inst */

        if (wtmodule->wasm_module != NULL) {
            wasm_importtype_vec_delete(&wtmodule->wasm_imports);
            wasm_exporttype_vec_delete(&wtmodule->wasm_exports);
            wasm_module_delete(wtmodule->wasm_module);
        }

        ngx_pfree(module->vm->pool, wtmodule);
    }

    module->vm = NULL;
}


static ngx_int_t
ngx_wasmtime_init_instance(ngx_wasm_winstance_t *instance,
    ngx_wasm_wmodule_t *module)
{
    ngx_wasmtime_module_t       *wtmodule;
    ngx_wasmtime_instance_t     *wtinstance;
    //const wasm_extern_t        **wasm_inst_externs;
    //wasm_importtype_vec_t        wasm_module_imports;
    //wasi_instance_t             *wasi_inst;
    wasmtime_error_t            *werror;
    wasm_trap_t                 *wtrap = NULL;
    ngx_int_t                    rc = NGX_ERROR;

    wtinstance = ngx_palloc(module->vm->pool, sizeof(ngx_wasmtime_instance_t));
    if (wtinstance == NULL) {
        ngx_log_error(NGX_LOG_ERR, module->vm->log, 0,
                      "failed to allocate wasmtime instance");
    }

    instance->winstance = wtinstance;
    instance->module = module;

    //wasm_module_imports = module->wmodule->wasm_imports;
    //wasi_inst = module->wmodule->wasi_inst;

    /*
    wasm_inst_externs = calloc(wasm_module_imports.size, sizeof(void *));
    if (wasm_inst_externs == NULL) {
        ngx_log_error(NGX_LOG_ERR, module->vm->log, 0,
                      "failed to allocate wasm module imports");
        goto failed;
    }

    for (size_t i = 0; i < wasm_module_imports.size; i++) {
        wasm_inst_externs[i] = wasi_instance_bind_import(wasi_inst,
                                                  wasm_module_imports.data[i]);
        if (wasm_inst_externs[i] == NULL) {
            ngx_wasmtime_log_error(NGX_LOG_ERR, module->vm->log, NULL, NULL,
                                   "failed to satisfy wasi instance imports");
            goto failed;
        }
    }
    */

    wtmodule = instance->module->wmodule;

    werror = wasmtime_instance_new(wtmodule->wasm_module,
                                   NULL, //wasm_inst_externs,
                                   0, //wasm_module_imports.size,
                                   &wtinstance->wasm_instance, &wtrap);
    if (wtinstance->wasm_instance == NULL) {
        ngx_wasmtime_log_error(NGX_LOG_ERR, module->vm->log, werror, wtrap,
                               "failed to instantiate instance");
        goto failed;
    }

    wasm_instance_exports(wtinstance->wasm_instance,
                          &wtinstance->wasm_externs);

    rc = NGX_OK;

failed:

    /*
    if (wasm_inst_externs != NULL) {
        ngx_free(wasm_inst_externs);
    }
    */

    if (rc != NGX_OK) {
        ngx_wasmtime_shutdown_instance(instance);
    }

    return rc;
}


static void
ngx_wasmtime_shutdown_instance(ngx_wasm_winstance_t *instance)
{
    ngx_wasmtime_instance_t  *wtinstance = instance->winstance;

    if (wtinstance != NULL) {
        if (wtinstance->wasm_instance != NULL) {
            //wasm_extern_vec_delete(&wtinstance->wasm_externs);
            wasm_instance_delete(wtinstance->wasm_instance);
        }

        ngx_pfree(instance->module->vm->pool, wtinstance);
    }

    instance->module = NULL;
}


static ngx_int_t
ngx_wasmtime_run_entrypoint(ngx_wasm_winstance_t *instance, const u_char *name)
{
    ngx_wasmtime_module_t       *wtmodule;
    ngx_wasmtime_instance_t     *wtinstance;
    const wasm_name_t           *wasm_name;
    wasm_extern_t               *func_extern = NULL;
    wasm_func_t                 *wfunc = NULL;
    wasm_trap_t                 *wtrap = NULL;
    wasmtime_error_t            *werror;
    ngx_int_t                    rc = NGX_ERROR;

    wtmodule = instance->module->wmodule;
    wtinstance = instance->winstance;

    for (size_t i = 0; i < wtinstance->wasm_externs.size; i++) {
        wasm_name = wasm_exporttype_name(wtmodule->wasm_exports.data[i]);

        ngx_log_debug2(NGX_LOG_DEBUG_WASM, instance->module->vm->log, 0,
                       "wasm module %s export: %s",
                       instance->module->path.data, wasm_name->data);

        if (ngx_strncmp(wasm_name->data, name, ngx_strlen(name)) == 0) {
            func_extern = wtinstance->wasm_externs.data[i];
        }
    }

    if (func_extern == NULL) {
        ngx_wasmtime_log_error(NGX_LOG_ALERT, instance->module->vm->log,
                               NULL, NULL,
                               "failed to find \"%s\" function",
                               name);
        goto failed;
    }

    wfunc = wasm_extern_as_func(func_extern);
    if (wfunc == NULL) {
        ngx_wasmtime_log_error(NGX_LOG_ALERT, instance->module->vm->log,
                               NULL, NULL,
                               "failed to load \"%s\" function",
                               name);
        goto failed;
    }

    werror = wasmtime_func_call(wfunc, NULL, 0, NULL, 0, &wtrap);
    if (werror != NULL || wtrap != NULL) {
        ngx_wasmtime_log_error(NGX_LOG_ALERT, instance->module->vm->log,
                               werror, wtrap,
                               "failed to call \"%s\" function",
                               name);
        goto failed;
    }

    rc = NGX_OK;

failed:

    if (wfunc != NULL) {
        wasm_func_delete(wfunc);
    }

    return rc;
}


static void
ngx_wasmtime_log_error(ngx_uint_t level, ngx_log_t *log,
    wasmtime_error_t *werror, wasm_trap_t *wtrap, const char *fmt, ...)
{
    va_list                      args;
    wasm_byte_vec_t              werror_msg;
    u_char                      *p, *last = NULL;
    u_char                       errstr[NGX_MAX_ERROR_STR];

    werror_msg.size = 0;

    if (werror != NULL) {
        wasmtime_error_message(werror, &werror_msg);
        wasmtime_error_delete(werror);

    } else if (wtrap != NULL) {
        wasm_trap_message(wtrap, &werror_msg);
        wasm_trap_delete(wtrap);
    }

    last = errstr + NGX_MAX_ERROR_STR;
    p = &errstr[0];

    va_start(args, fmt);
    p = ngx_vslprintf(p, last, fmt, args);
    va_end(args);

    if (werror_msg.size) {
        ngx_log_error_core(level, log, 0,
                           "[wasmtime] %*s (%*s)",
                           p - errstr, errstr,
                           werror_msg.size, werror_msg.data);

        wasm_byte_vec_delete(&werror_msg);

    } else {
        ngx_log_error_core(level, log, 0,
                           "[wasmtime] %*s",
                           p - errstr, errstr);
    }
}


/* vi:set ft=c ts=4 sw=4 et fdm=marker: */
