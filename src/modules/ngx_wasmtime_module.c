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


static void       *ngx_wasmtime_create_conf(ngx_cycle_t *cycle);
static ngx_int_t   ngx_wasmtime_init(ngx_cycle_t *cycle);
static ngx_int_t   ngx_wasmtime_init_engine(ngx_cycle_t *cycle);
static void        ngx_wasmtime_shutdown_engine(ngx_cycle_t *cycle);
static ngx_int_t   ngx_wasmtime_init_store(ngx_cycle_t *cycle);
static void        ngx_wasmtime_shutdown_store(ngx_cycle_t *cycle);
static ngx_int_t   ngx_wasmtime_load_wasm_module(ngx_cycle_t *cycle,
                                               const u_char *path);
static ngx_int_t   ngx_wasmtime_init_instance(ngx_cycle_t *cycle);
static ngx_int_t   ngx_wasmtime_run_entrypoint(ngx_cycle_t *cycle,
                                             const u_char *entrypoint);
static void        ngx_wasmtime_log_error(ngx_uint_t level, ngx_log_t *log,
                                          wasmtime_error_t *werror,
                                          wasm_trap_t *wtrap, const char *fmt,
                                          ...);


static ngx_str_t          module_name = ngx_string("wasmtime");


typedef struct {
    wasm_engine_t        *engine;
    wasm_store_t         *store;
    wasm_module_t        *module;
    wasm_instance_t      *instance;

    wasi_config_t        *wasi_config;
    wasi_instance_t      *wasi_inst;
} ngx_wasmtime_conf_t;


static ngx_wasm_module_t  ngx_wasmtime_module_ctx = {
    NGX_WASM_MODULE_VM,
    &module_name,
    ngx_wasmtime_create_conf,
    NULL,
    ngx_wasmtime_init,

    {
        ngx_wasmtime_init_engine,
        ngx_wasmtime_shutdown_engine,
        ngx_wasmtime_init_store,
        ngx_wasmtime_shutdown_store,
        ngx_wasmtime_load_wasm_module,
        ngx_wasmtime_init_instance,
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
    ngx_wasm_vm_actions = ngx_wasmtime_module_ctx.vm_actions;

    return NGX_OK;
}


static ngx_int_t
ngx_wasmtime_init_engine(ngx_cycle_t *cycle)
{
    ngx_wasmtime_conf_t         *wtcf;

    wtcf = ngx_wasmtime_cycle_get_conf(cycle);

    wtcf->engine = wasm_engine_new();
    if (wtcf->engine == NULL) {
        ngx_wasmtime_log_error(NGX_LOG_ALERT, cycle->log, NULL, NULL,
                               "failed to instantiate WASM engine");
        goto failed;
    }

    wtcf->wasi_config = wasi_config_new();
    if (wtcf->wasi_config == NULL) {
        ngx_wasmtime_log_error(NGX_LOG_ALERT, cycle->log, NULL, NULL,
                               "failed to instantiate WASI config");
        goto failed;
    }

    wasi_config_inherit_stdout(wtcf->wasi_config);
    wasi_config_inherit_stderr(wtcf->wasi_config);

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

    if (wtcf->engine != NULL) {
        wasm_engine_delete(wtcf->engine);
    }
}


static ngx_int_t
ngx_wasmtime_init_store(ngx_cycle_t *cycle)
{
    ngx_wasmtime_conf_t         *wtcf;
    wasm_trap_t                 *wtrap = NULL;

    wtcf = ngx_wasmtime_cycle_get_conf(cycle);

    wtcf->store = wasm_store_new(wtcf->engine);
    if (wtcf->store == NULL) {
        ngx_wasmtime_log_error(NGX_LOG_ALERT, cycle->log, NULL, NULL,
                               "failed to instantiate WASM store");
        goto failed;
    }

    wtcf->wasi_inst = wasi_instance_new(wtcf->store, "wasi_snapshot_preview1",
                                        wtcf->wasi_config, &wtrap);
    if (wtcf->wasi_inst == NULL) {
        ngx_wasmtime_log_error(NGX_LOG_ALERT, cycle->log, NULL, wtrap,
                               "failed to instantiate WASI instance");
        goto failed;
    }

    return NGX_OK;

failed:

    ngx_wasmtime_shutdown_store(cycle);

    return NGX_ERROR;
}


static void
ngx_wasmtime_shutdown_store(ngx_cycle_t *cycle)
{
    ngx_wasmtime_conf_t         *wtcf;

    wtcf = ngx_wasmtime_cycle_get_conf(cycle);

    if (wtcf->store != NULL) {
        wasm_store_delete(wtcf->store);
    }
}


static ngx_int_t
ngx_wasmtime_load_wasm_module(ngx_cycle_t *cycle, const u_char *path)
{
    ngx_wasmtime_conf_t         *wtcf;
    ssize_t                      fsize, n;
    ngx_fd_t                     fd;
    ngx_file_t                   file;
    wasm_byte_vec_t              wbytes;
    wasmtime_error_t            *werror;
    ngx_int_t                    rc = NGX_ERROR;

    wtcf = ngx_wasmtime_cycle_get_conf(cycle);

    fd = ngx_open_file(path, NGX_FILE_RDONLY, NGX_FILE_OPEN, 0);
    if (fd == NGX_INVALID_FILE) {
        ngx_log_error(NGX_LOG_ALERT, cycle->log, ngx_errno,
                      ngx_open_file_n " \"%s\" failed", path);
        return NGX_ERROR;
    }

    ngx_memzero(&file, sizeof(ngx_file_t));

    file.fd = fd;
    file.log = cycle->log;
    file.name.len = ngx_strlen(path);
    file.name.data = ngx_palloc(cycle->pool, file.name.len);
    if (file.name.data == NULL) {
        goto failed;
    }

    ngx_memcpy(&file.name.data, path, file.name.len);

    if (ngx_fd_info(fd, &file.info) == NGX_FILE_ERROR) {
        ngx_log_error(NGX_LOG_ALERT, cycle->log, ngx_errno,
                      ngx_fd_info_n " \"%s\" failed", &file.name);
        goto failed;
    }

    fsize = ngx_file_size(&file.info);
    wasm_byte_vec_new_uninitialized(&wbytes, fsize);

    n = ngx_read_file(&file, (u_char *) wbytes.data, fsize, 0);
    if (n == NGX_ERROR) {
        ngx_log_error(NGX_LOG_ALERT, cycle->log, ngx_errno,
                      ngx_read_file_n " \"%V\" failed", &file.name);
        goto failed;
    }

    if (n != fsize) {
        ngx_log_error(NGX_LOG_ALERT, cycle->log, 0,
                      ngx_read_file_n " \"%V\" returned only "
                      "%z bytes instead of %uz", &file.name, n, fsize);
        goto failed;
    }

    werror = wasmtime_module_new(wtcf->store, &wbytes, &wtcf->module);
    if (wtcf->module == NULL) {
        ngx_wasmtime_log_error(NGX_LOG_ALERT, cycle->log, werror, NULL,
                               "failed to compile module \"%V\"", &file.name);
        goto failed;
    }

    rc = NGX_OK;

failed:

    if (ngx_close_file(file.fd) == NGX_FILE_ERROR) {
        ngx_log_error(NGX_LOG_ALERT, cycle->log, ngx_errno,
                      ngx_close_file_n " \"%V\" failed", &file.name);
    }

    wasm_byte_vec_delete(&wbytes);

    return rc;
}


static ngx_int_t
ngx_wasmtime_init_instance(ngx_cycle_t *cycle)
{
    ngx_wasmtime_conf_t         *wtcf;
    const wasm_extern_t        **wexterns;
    wasm_importtype_vec_t        wimports;
    wasmtime_error_t            *werror;
    wasm_trap_t                 *wtrap = NULL;
    ngx_int_t                    rc = NGX_ERROR;

    wtcf = ngx_wasmtime_cycle_get_conf(cycle);

    wasm_module_imports(wtcf->module, &wimports);

    wexterns = calloc(wimports.size, sizeof(void*));
    if (wexterns == NULL) {
        ngx_wasmtime_log_error(NGX_LOG_ALERT, cycle->log, NULL, NULL,
                               "failed to instantiate WASM externs");
        goto failed;
    }

    for (size_t i = 0; i < wimports.size; i++) {
        wexterns[i] = wasi_instance_bind_import(wtcf->wasi_inst,
                                                wimports.data[i]);
        if (wexterns == NULL) {
            ngx_wasmtime_log_error(NGX_LOG_ALERT, cycle->log, NULL, NULL,
                                   "failed to satisfy WASM import");
            goto failed;
        }
    }

    werror = wasmtime_instance_new(wtcf->module, wexterns, wimports.size,
                                   &wtcf->instance, &wtrap);
    if (wtcf->instance == NULL) {
        ngx_wasmtime_log_error(NGX_LOG_ALERT, cycle->log, werror, wtrap,
                               "failed to instanciate WASM instance");
        goto failed;
    }

    rc = NGX_OK;

failed:

    wasm_importtype_vec_delete(&wimports);

    if (wexterns != NULL) {
        free(wexterns);
    }

    return rc;
}


static ngx_int_t
ngx_wasmtime_run_entrypoint(ngx_cycle_t *cycle, const u_char *entrypoint)
{

    ngx_wasmtime_conf_t         *wtcf;
    wasm_exporttype_vec_t        wexports;
    wasm_extern_vec_t            wexterns;
    wasm_extern_t               *func_extern = NULL;
    wasm_func_t                 *wfunc = NULL;
    wasm_trap_t                 *wtrap = NULL;
    wasmtime_error_t            *werror;
    ngx_int_t                    rc = NGX_ERROR;

    wtcf = ngx_wasmtime_cycle_get_conf(cycle);

    wasm_instance_exports(wtcf->instance, &wexterns);
    wasm_module_exports(wtcf->module, &wexports);

    for (size_t i = 0; i < wexports.size; i++) {
        const wasm_name_t *name = wasm_exporttype_name(wexports.data[i]);
        if (ngx_strncmp(name->data, entrypoint, ngx_strlen(entrypoint)) == 0) {
            func_extern = wexterns.data[i];
        }
    }

    if (func_extern == NULL) {
        ngx_wasmtime_log_error(NGX_LOG_ALERT, cycle->log, NULL, NULL,
                               "failed to find \"%s\" function",
                               entrypoint);
        goto failed;
    }

    wfunc = wasm_extern_as_func(func_extern);
    if (wfunc == NULL) {
        ngx_wasmtime_log_error(NGX_LOG_ALERT, cycle->log, NULL, NULL,
                               "failed to load \"%s\" function",
                               &entrypoint);
        goto failed;
    }

    werror = wasmtime_func_call(wfunc, NULL, 0, NULL, 0, &wtrap);
    if (werror != NULL || wtrap != NULL) {
        ngx_wasmtime_log_error(NGX_LOG_ALERT, cycle->log, werror, wtrap,
                               "failed to call \"%s\" function",
                               entrypoint);
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

    ngx_wasm_assert(werror != NULL || wtrap != NULL);

    if (werror != NULL) {
        wasmtime_error_message(werror, &werror_msg);
        wasmtime_error_delete(werror);

    } else {
        wasm_trap_message(wtrap, &werror_msg);
        wasm_trap_delete(wtrap);
    }

    last = errstr + NGX_MAX_ERROR_STR;
    p = &errstr[0];

    va_start(args, fmt);
    p = ngx_vslprintf(p, last, fmt, args);
    va_end(args);

    ngx_log_error_core(level, log, 0, "%*s (%*s)", p - errstr, errstr,
                       werror_msg.size, werror_msg.data);

    wasm_byte_vec_delete(&werror_msg);
}


/* vi:set ft=c ts=4 sw=4 et fdm=marker: */
