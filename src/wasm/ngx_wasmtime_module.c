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


typedef struct {
    wasm_engine_t          *wasm_engine;
    wasm_store_t           *wasm_store;
} ngx_wasmtime_conf_t;


typedef struct {
    wasm_importtype_vec_t   wasm_imports;
    wasm_exporttype_vec_t   wasm_exports;
    wasm_store_t           *wasm_store;
    wasm_module_t          *wasm_module;
    ngx_pool_t             *pool;
    ngx_log_t              *log;
} ngx_wasmtime_wmodule_t;


typedef struct {
    wasm_extern_vec_t       wasm_externs;
    wasm_exporttype_vec_t  *wasm_module_exports;
    wasm_instance_t        *wasm_instance;
    ngx_pool_t             *pool;
    ngx_log_t              *log;
} ngx_wasmtime_winstance_t;


static void *ngx_wasmtime_create_conf(ngx_cycle_t *cycle);
static ngx_int_t ngx_wasmtime_init(ngx_cycle_t *cycle,
    ngx_wasm_vm_actions_t **vm_actions);
static void ngx_wasmtime_shutdown(ngx_cycle_t *cycle);
static void *ngx_wasmtime_new_module(ngx_str_t *path,
    ngx_pool_t *pool, ngx_log_t *log, ngx_cycle_t *cycle, ngx_uint_t wat);
static void ngx_wasmtime_free_module(void *data);
static void *ngx_wasmtime_new_instance(void *data, wasm_trap_t *wtrap);
static void ngx_wasmtime_free_instance(void *data);
static ngx_int_t ngx_wasmtime_call_instance(void *data,
    const char *fname, const ngx_wasm_wval_t *args, size_t nargs,
    ngx_wasm_wval_t *rets, size_t nrets, wasm_trap_t *trap);
static void ngx_wasmtime_log_error(ngx_uint_t level, ngx_log_t *log,
    wasmtime_error_t *werror,
#if (NGX_HAVE_VARIADIC_MACROS)
    const char *fmt, ...);

#else
    const char *fmt, va_list args);
#endif


static ngx_str_t  module_name = ngx_string("wasmtime");


static ngx_wasm_module_t  ngx_wasmtime_module_ctx = {
    &module_name,
    ngx_wasmtime_create_conf,            /* create configuration */
    NULL,                                /* init configuration */
    ngx_wasmtime_init,                   /* init module */
    {                                    /* VM actions */
        ngx_wasmtime_new_module,
        ngx_wasmtime_free_module,
        ngx_wasmtime_new_instance,
        ngx_wasmtime_free_instance,
        ngx_wasmtime_call_instance
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
ngx_wasmtime_init(ngx_cycle_t *cycle, ngx_wasm_vm_actions_t **vm_actions)
{
    ngx_wasmtime_conf_t         *wtcf;

    wtcf = ngx_wasmtime_cycle_get_conf(cycle);

    wtcf->wasm_engine = wasm_engine_new();
    if (wtcf->wasm_engine == NULL) {
        ngx_log_error(NGX_LOG_EMERG, cycle->log, 0,
                      "failed to create wasmtime engine");
        return NGX_ERROR;
    }

    wtcf->wasm_store = wasm_store_new(wtcf->wasm_engine);
    if (wtcf->wasm_store == NULL) {
        ngx_log_error(NGX_LOG_EMERG, cycle->log, 0,
                      "failed to create wasmtime store");
        return NGX_ERROR;
    }

    *vm_actions = &ngx_wasmtime_module_ctx.vm_actions;

    return NGX_OK;
}


static void
ngx_wasmtime_shutdown(ngx_cycle_t *cycle)
{
    ngx_wasmtime_conf_t         *wtcf;

    wtcf = ngx_wasmtime_cycle_get_conf(cycle);

    if (wtcf->wasm_engine != NULL) {
        wasm_engine_delete(wtcf->wasm_engine);
    }

    if (wtcf->wasm_store != NULL) {
        wasm_store_delete(wtcf->wasm_store);
    }
}


static void *
ngx_wasmtime_new_module(ngx_str_t *path, ngx_pool_t *pool, ngx_log_t *log,
    ngx_cycle_t *cycle, ngx_uint_t wat)
{
    ngx_wasmtime_conf_t         *wtcf;
    ngx_wasmtime_wmodule_t      *wtmodule;
    wasmtime_error_t            *werror;
    wasm_byte_vec_t              wat_bytes, wasm_bytes;

    wtcf = ngx_wasmtime_cycle_get_conf(cycle);

    ngx_wasm_assert(wtcf->wasm_store);

    if (wat) {
        if (ngx_wasm_read_file(&wat_bytes, path->data, log) != NGX_OK) {
            return NULL;
        }

        werror = wasmtime_wat2wasm(&wat_bytes, &wasm_bytes);
        if (werror) {
            ngx_wasmtime_log_error(NGX_LOG_ERR, log, werror,
                                   "failed to compile .wat module at \"%V\"",
                                   path);
            return NULL;
        }

    } else if (ngx_wasm_read_file(&wasm_bytes, path->data, log) != NGX_OK) {
        return NULL;
    }

    wtmodule = ngx_palloc(pool, sizeof(ngx_wasmtime_wmodule_t));
    if (wtmodule == NULL) {
        return NULL;
    }

    wtmodule->wasm_store = wtcf->wasm_store;
    wtmodule->pool = pool;
    wtmodule->log = log;

    werror = wasmtime_module_new(wtmodule->wasm_store, &wasm_bytes,
                                 &wtmodule->wasm_module);
    if (wtmodule->wasm_module == NULL) {
        ngx_wasmtime_log_error(NGX_LOG_ERR, log, werror,
                               "failed to compile module at \"%V\"", path);
        goto failed;
    }

    wasm_module_imports(wtmodule->wasm_module, &wtmodule->wasm_imports);
    wasm_module_exports(wtmodule->wasm_module, &wtmodule->wasm_exports);

    return wtmodule;

failed:

    ngx_wasmtime_free_module(wtmodule);

    return NULL;
}


static void
ngx_wasmtime_free_module(void *data)
{
    ngx_wasmtime_wmodule_t      *wtmodule = data;

    if (wtmodule->wasm_module != NULL) {
        wasm_importtype_vec_delete(&wtmodule->wasm_imports);
        wasm_exporttype_vec_delete(&wtmodule->wasm_exports);
        wasm_module_delete(wtmodule->wasm_module);
    }

    ngx_pfree(wtmodule->pool, wtmodule);
}


static void *
ngx_wasmtime_new_instance(void *data, wasm_trap_t *wtrap)
{
    ngx_wasmtime_wmodule_t      *wtmodule = data;
    ngx_wasmtime_winstance_t    *wtinstance;
    wasmtime_error_t            *werror;

    ngx_wasm_assert(wtmodule->wasm_module != NULL);

    wtinstance = ngx_palloc(wtmodule->pool, sizeof(ngx_wasmtime_winstance_t));
    if (wtinstance == NULL) {
        return NULL;
    }

    wtinstance->pool = wtmodule->pool;
    wtinstance->log = wtmodule->log;

    werror = wasmtime_instance_new(wtmodule->wasm_module, NULL, 0,
                                   &wtinstance->wasm_instance, &wtrap);
    if (werror != NULL || wtrap != NULL) {
        if (werror != NULL) {
            ngx_wasmtime_log_error(NGX_LOG_ERR, wtmodule->log, werror,
                                   "failed to create instance");
        }

        goto failed;
    }

    ngx_wasm_assert(wtinstance->wasm_instance != NULL);

    wtinstance->wasm_module_exports = &wtmodule->wasm_exports;
    wasm_instance_exports(wtinstance->wasm_instance,
                          &wtinstance->wasm_externs);

    return wtinstance;

failed:

    ngx_wasmtime_free_instance(wtinstance);

    return NULL;
}


static void
ngx_wasmtime_free_instance(void *data)
{
    ngx_wasmtime_winstance_t       *wtinstance = data;

    if (wtinstance->wasm_instance != NULL) {
        wasm_instance_delete(wtinstance->wasm_instance);
    }

    ngx_pfree(wtinstance->pool, wtinstance);
}


static wasm_val_t
ngx_wasmtime_ngxwval_to_wval(ngx_wasm_wval_t val)
{
    wasm_val_t      wval;

    switch (val.kind) {

    case NGX_WASM_I32:
        wval.kind = WASM_I32;
        wval.of.i32 = val.value.I32;
        break;

    case NGX_WASM_I64:
        wval.kind = WASM_I64;
        wval.of.i64 = val.value.I64;
        break;

    case NGX_WASM_F32:
        wval.kind = WASM_F32;
        wval.of.f32 = val.value.F32;
        break;

    case NGX_WASM_F64:
        wval.kind = WASM_F64;
        wval.of.f64 = val.value.F64;
        break;

    default:
        ngx_wasm_assert(NULL);
        break;

    }

    return wval;
}


static ngx_wasm_wval_t
ngx_wasmtime_wval_to_ngxwval(wasm_val_t wval)
{
    ngx_wasm_wval_t     val;

    switch (wval.kind) {

    case WASM_I32:
        val.kind = NGX_WASM_I32;
        val.value.I32 = wval.of.i32;
        break;

    case WASM_I64:
        val.kind = NGX_WASM_I64;
        val.value.I64 = wval.of.i64;
        break;

    case WASM_F32:
        val.kind = NGX_WASM_F32;
        val.value.F32 = wval.of.f32;
        break;

    case WASM_F64:
        val.kind = NGX_WASM_F64;
        val.value.F64 = wval.of.f64;
        break;

    default:
        ngx_wasm_assert(NULL);
        break;

    }

    return val;
}


static void
ngx_wasmtime_ngxwvals_to_wvals(wasm_val_t *wvals, const ngx_wasm_wval_t *vals,
    size_t nvals)
{
    size_t          i;

    for (i = 0; i < nvals; i++) {
        wvals[i] = ngx_wasmtime_ngxwval_to_wval(vals[i]);
    }
}


static void
ngx_wasmtime_wvals_to_ngxwvals(ngx_wasm_wval_t *vals, wasm_val_t *wvals,
    size_t nwvals)
{
    size_t          i;

    for (i = 0; i < nwvals; i++) {
        vals[i] = ngx_wasmtime_wval_to_ngxwval(wvals[i]);
    }
}


static ngx_int_t
ngx_wasmtime_call_helper(ngx_wasmtime_winstance_t *wtinstance,
    const char *fname, const ngx_wasm_wval_t *args, size_t nargs,
    ngx_wasm_wval_t *rets, size_t nrets, wasm_trap_t *wtrap)
{
    const wasm_name_t           *wname;
    wasm_val_t                   wargs[nargs], wrets[nrets];
    wasm_extern_t               *wfextern = NULL;
    wasm_func_t                 *wfunc;
    wasmtime_error_t            *werror;
    size_t                       i;
    ngx_int_t                    rc = NGX_ERROR;

    for (i = 0; i < wtinstance->wasm_externs.size; i++) {
        wname = wasm_exporttype_name(wtinstance->wasm_module_exports->data[i]);

        if (ngx_strncmp(wname->data, fname, ngx_strlen(fname)) == 0) {
            wfextern = wtinstance->wasm_externs.data[i];
        }
    }

    if (wfextern == NULL) {
        return NGX_DECLINED;
    }

    wfunc = wasm_extern_as_func(wfextern);
    if (wfunc == NULL) {
        goto failed;
    }

    ngx_wasmtime_ngxwvals_to_wvals(wargs, args, nargs);

    werror = wasmtime_func_call(wfunc, wargs, nargs, wrets, nrets, &wtrap);
    if (werror != NULL || wtrap != NULL) {
        if (werror != NULL) {
            ngx_wasmtime_log_error(NGX_LOG_ERR, wtinstance->log, werror,
                                   "failed to call \"%s\" function", fname);
            rc = NGX_ABORT;
        }

        goto failed;
    }

    ngx_wasmtime_wvals_to_ngxwvals(rets, wrets, nrets);

    rc = NGX_OK;

failed:

    if (wfunc != NULL) {
        wasm_func_delete(wfunc);
    }

    return rc;
}


static ngx_int_t
ngx_wasmtime_call_instance(void *data, const char *fname,
    const ngx_wasm_wval_t *args, size_t nargs, ngx_wasm_wval_t *rets,
    size_t nrets, wasm_trap_t *wtrap)
{
    ngx_wasmtime_winstance_t     *wtinstance = data;

    return ngx_wasmtime_call_helper(wtinstance, fname, args, nargs, rets,
                                    nrets, wtrap);
}


static void
ngx_wasmtime_log_error(ngx_uint_t level, ngx_log_t *log,
    wasmtime_error_t *werror,
#if (NGX_HAVE_VARIADIC_MACROS)
    const char *fmt, ...)

#else
    const char *fmt, va_list args)
#endif
{
#if (NGX_HAVE_VARIADIC_MACROS)
    va_list                      args;
#endif
    wasm_byte_vec_t              werror_msg;
    u_char                      *p, *last;
    u_char                       errstr[NGX_MAX_ERROR_STR];

    ngx_memzero(&werror_msg, sizeof(wasm_byte_vec_t));

    if (werror != NULL) {
        wasmtime_error_message(werror, &werror_msg);
        wasmtime_error_delete(werror);
    }

    last = errstr + NGX_MAX_ERROR_STR;
    p = &errstr[0];

#if (NGX_HAVE_VARIADIC_MACROS)
    va_start(args, fmt);
    p = ngx_vslprintf(p, last, fmt, args);
    va_end(args);

#else
    p = ngx_vslprintf(p, last, fmt, args);
#endif

    if (werror_msg.size) {
        ngx_wasm_log_error(level, log, 0,
                           "%*s (wasmtime error: %*s)",
                           p - errstr, errstr,
                           werror_msg.size, werror_msg.data);

        wasm_byte_vec_delete(&werror_msg);

    } else {
        ngx_wasm_log_error(level, log, 0,
                           "%*s (wasmtime error: ?)",
                           p - errstr, errstr);
    }
}


/* vi:set ft=c ts=4 sw=4 et fdm=marker: */
