/*
 * Copyright (C) Thibault Charbonnier
 */

#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_config.h>
#include <ngx_wasm.h>
#include <ngx_wasm_util.h>

#include <wasm.h>
#include <wasi.h>
#include <wasmtime.h>


#define ngx_wasmtime_cycle_get_conf(cycle)                                   \
    (*(ngx_get_conf(cycle->conf_ctx, ngx_wasm_module)))                      \
    [ngx_wasmtime_module.ctx_index]


static void *ngx_wasmtime_create_conf(ngx_cycle_t *cycle);
static ngx_int_t ngx_wasmtime_init(ngx_cycle_t *cycle,
    ngx_wasm_vm_actions_t **vm_actions);
static void ngx_wasmtime_shutdown(ngx_cycle_t *cycle);
static ngx_int_t ngx_wasmtime_load_module(ngx_wasm_wmodule_t *module,
    ngx_cycle_t *cycle);
static void ngx_wasmtime_unload_module(ngx_wasm_wmodule_t *module);
static ngx_wasm_winstance_t *ngx_wasmtime_new_instance(
    ngx_wasm_wmodule_t *module);
static void ngx_wasmtime_free_instance(ngx_wasm_winstance_t *instance);
static ngx_int_t ngx_wasmtime_call_helper(ngx_wasm_winstance_t *instance,
    const u_char *fname, ngx_uint_t maybe);
static ngx_int_t ngx_wasmtime_call(ngx_wasm_winstance_t *instance,
    const u_char *fname);
static ngx_int_t ngx_wasmtime_call_maybe(ngx_wasm_winstance_t *instance,
    const u_char *fname);
static void ngx_wasmtime_log_error(ngx_uint_t level, ngx_log_t *log,
    wasmtime_error_t *werror, wasm_trap_t *wtrap, const char *fmt, ...);


typedef struct {
    wasm_engine_t          *wasm_engine;
    wasm_store_t           *wasm_store;
    //wasi_config_t          *wasi_config;
} ngx_wasmtime_conf_t;


typedef struct {
    //wasi_instance_t        *wasi_inst;
    wasm_importtype_vec_t   wasm_imports;
    wasm_exporttype_vec_t   wasm_exports;
    wasm_store_t           *wasm_store;
    wasm_module_t          *wasm_module;
    ngx_wasm_wmodule_t     *mod;
} ngx_wasmtime_wmodule_t;


typedef struct {
    wasm_extern_vec_t       wasm_externs;
    wasm_exporttype_vec_t  *wasm_module_exports;
    wasm_instance_t        *wasm_instance;
    ngx_wasm_winstance_t   *inst;
} ngx_wasmtime_winstance_t;


static ngx_str_t  module_name = ngx_string("wasmtime");


static ngx_wasm_module_t  ngx_wasmtime_module_ctx = {
    &module_name,
    ngx_wasmtime_create_conf,            /* create configuration */
    NULL,                                /* init configuration */
    ngx_wasmtime_init,                   /* init module */
    {                                    /* VM actions */
        ngx_wasmtime_load_module,
        ngx_wasmtime_unload_module,
        ngx_wasmtime_new_instance,
        ngx_wasmtime_free_instance,
        ngx_wasmtime_call,
        ngx_wasmtime_call_maybe
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
        ngx_wasmtime_log_error(NGX_LOG_ALERT, cycle->log, NULL, NULL,
                               "failed to instantiate engine");
        return NGX_ERROR;
    }

    wtcf->wasm_store = wasm_store_new(wtcf->wasm_engine);
    if (wtcf->wasm_store == NULL) {
        ngx_wasmtime_log_error(NGX_LOG_ALERT, cycle->log, NULL, NULL,
                               "failed to instantiate store");
        return NGX_ERROR;
    }

    /*{{{
    wtcf->wasi_config = wasi_config_new();
    if (wtcf->wasi_config == NULL) {
        ngx_wasmtime_log_error(NGX_LOG_ALERT, cycle->log, NULL, NULL,
                               "failed to instantiate wasi config");
        goto failed;
    }

    wasi_config_inherit_stdout(wtcf->wasi_config);
    wasi_config_inherit_stderr(wtcf->wasi_config);
    *//*}}}*/

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

    /* TODO: wasi_config */
    /* TODO: modules and instances */
}


static ngx_int_t
ngx_wasmtime_load_module(ngx_wasm_wmodule_t *module, ngx_cycle_t *cycle)
{
    ngx_wasmtime_conf_t         *wtcf;
    ngx_wasmtime_wmodule_t      *wtmodule;
    wasmtime_error_t            *werror;
    wasm_byte_vec_t              wat_bytes, wasm_bytes;
    ngx_int_t                    rc = NGX_ERROR;

    wtcf = ngx_wasmtime_cycle_get_conf(cycle);

    ngx_wasm_assert(module->path.data != NULL);

    if (module->path.len == 0) {
        return NGX_DECLINED;
    }

    if (module->wat) {
        if (ngx_wasm_read_file(&wat_bytes, module->path.data, module->log)
            != NGX_OK)
        {
            return NGX_ERROR;
        }

        werror = wasmtime_wat2wasm(&wat_bytes, &wasm_bytes);
        if (werror) {
            ngx_wasmtime_log_error(NGX_LOG_ERR, module->log, werror, NULL,
                                   "failed to compile .wat module at \"%V\"",
                                   &module->path);
            return NGX_ERROR;
        }

    } else if (ngx_wasm_read_file(&wasm_bytes, module->path.data, module->log)
               != NGX_OK)
    {
        return NGX_ERROR;
    }

    wtmodule = ngx_palloc(module->pool, sizeof(ngx_wasmtime_wmodule_t));
    if (wtmodule == NULL) {
        ngx_log_error(NGX_LOG_ERR, module->log, 0,
                      "failed to allocate wasmtime module");
        return NGX_ERROR;
    }

    module->data = wtmodule;
    wtmodule->mod = module;
    wtmodule->wasm_store = wtcf->wasm_store;

    werror = wasmtime_module_new(wtmodule->wasm_store, &wasm_bytes,
                                 &wtmodule->wasm_module);
    if (wtmodule->wasm_module == NULL) {
        ngx_wasmtime_log_error(NGX_LOG_ERR, module->log, werror, NULL,
                               "failed to compile module at \"%V\"",
                               &module->path);
        goto failed;
    }

    /*{{{
    wtmodule->wasi_inst = wasi_instance_new(vm->wvm->wasm_store,
                                            "wasi_snapshot_preview1",
                                            vm->wvm->wtcf->wasi_config,
                                            &wtrap);
    if (wtcf->wasi_inst == NULL) {
        ngx_wasmtime_log_error(NGX_LOG_ERR, vm->log, NULL, wtrap,
                               "failed to instantiate wasi instance");
        goto failed;
    }
    *//*}}}*/

    wasm_module_imports(wtmodule->wasm_module, &wtmodule->wasm_imports);
    wasm_module_exports(wtmodule->wasm_module, &wtmodule->wasm_exports);



    rc = NGX_OK;

failed:

    //wasm_byte_vec_delete(&wat_bytes);
    //wasm_byte_vec_delete(&wasm_bytes);

    if (rc != NGX_OK) {
        ngx_wasmtime_unload_module(module);
    }

    return rc;
}


static void
ngx_wasmtime_unload_module(ngx_wasm_wmodule_t *module)
{
    ngx_wasmtime_wmodule_t  *wtmodule = module->data;

    if (wtmodule != NULL) {
        /* TODO: wasi_inst */

        if (wtmodule->wasm_module != NULL) {
            wasm_importtype_vec_delete(&wtmodule->wasm_imports);
            wasm_exporttype_vec_delete(&wtmodule->wasm_exports);
            wasm_module_delete(wtmodule->wasm_module);
        }

        ngx_pfree(module->pool, wtmodule);
        module->data = NULL;
    }
}


static ngx_wasm_winstance_t *
ngx_wasmtime_new_instance(ngx_wasm_wmodule_t *module)
{
    ngx_wasm_winstance_t        *instance = NULL;
    ngx_wasmtime_wmodule_t      *wtmodule = module->data;
    ngx_wasmtime_winstance_t    *wtinstance;
    //const wasm_extern_t        **wasm_inst_externs;
    //wasm_importtype_vec_t        wasm_module_imports;
    //wasi_instance_t             *wasi_inst;
    wasmtime_error_t            *werror;
    wasm_trap_t                 *wtrap = NULL;

    ngx_wasm_assert(wtmodule->wasm_module != NULL);

    wtinstance = ngx_palloc(module->pool, sizeof(ngx_wasmtime_winstance_t));
    if (wtinstance == NULL) {
        ngx_log_error(NGX_LOG_ERR, module->log, 0,
                      "failed to allocate wasmtime instance");
        return NULL;
    }

    //wasm_module_imports = module->wmodule->wasm_imports;{{{
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
    *//*}}}*/

    werror = wasmtime_instance_new(wtmodule->wasm_module,
                                   NULL, //wasm_inst_externs,
                                   0, //wasm_module_imports.size,
                                   &wtinstance->wasm_instance, &wtrap);
    if (wtinstance->wasm_instance == NULL) {
        ngx_wasmtime_log_error(NGX_LOG_ERR, module->log, werror, wtrap,
                               "failed to instantiate instance");
        goto failed;
    }

    instance = ngx_palloc(module->pool, sizeof(ngx_wasm_winstance_t));
    if (instance == NULL) {
        ngx_log_error(NGX_LOG_ERR, module->log, 0,
                      "failed to allocate ngx_wasm instance");
        goto failed;
    }

    instance->log = module->log;
    instance->pool = module->pool;
    instance->data = wtinstance;

    wtinstance->inst = instance;
    wtinstance->wasm_module_exports = &wtmodule->wasm_exports;
    wasm_instance_exports(wtinstance->wasm_instance, &wtinstance->wasm_externs);

failed:

    /*{{{
    if (wasm_inst_externs != NULL) {
        ngx_free(wasm_inst_externs);
    }
    *//*}}}*/

    if (instance == NULL) {
        if (wtinstance->wasm_instance != NULL) {
            wasm_instance_delete(wtinstance->wasm_instance);
        }

        ngx_pfree(module->pool, wtinstance);
    }

    return instance;
}


static void
ngx_wasmtime_free_instance(ngx_wasm_winstance_t *instance)
{
    ngx_wasmtime_winstance_t  *wtinstance = instance->data;

    if (wtinstance != NULL) {
        if (wtinstance->wasm_instance != NULL) {
            wasm_instance_delete(wtinstance->wasm_instance);
        }

        ngx_pfree(instance->pool, instance);
    }
}


static ngx_int_t
ngx_wasmtime_call_helper(ngx_wasm_winstance_t *instance, const u_char *fname,
    ngx_uint_t maybe)
{
    ngx_wasmtime_winstance_t    *wtinstance = instance->data;
    const wasm_name_t           *wname;
    wasm_extern_t               *wfextern = NULL;
    wasm_func_t                 *wfunc = NULL;
    wasm_trap_t                 *wtrap = NULL;
    wasmtime_error_t            *werror;
    size_t                       i;
    ngx_int_t                    rc = NGX_ERROR;

    ngx_log_debug3(NGX_LOG_DEBUG_WASM, instance->log, 0,
                   "wasm instance %p calling function \"%s\" (maybe: %u)",
                   wtinstance->wasm_instance, fname, maybe);

    for (i = 0; i < wtinstance->wasm_externs.size; i++) {
        wname = wasm_exporttype_name(wtinstance->wasm_module_exports->data[i]);

        if (ngx_strncmp(wname->data, fname, ngx_strlen(fname)) == 0) {
            wfextern = wtinstance->wasm_externs.data[i];
        }
    }

    if (wfextern == NULL) {
        if (maybe) {
            return NGX_DONE;
        }

        ngx_wasmtime_log_error(NGX_LOG_ERR, instance->log, NULL, NULL,
                               "failed to find function \"%s\"", fname);
        return NGX_DECLINED;
    }

    wfunc = wasm_extern_as_func(wfextern);
    if (wfunc == NULL) {
        ngx_wasmtime_log_error(NGX_LOG_ERR, instance->log, NULL, NULL,
                               "failed to load function \"%s\"", fname);
        goto failed;
    }

    werror = wasmtime_func_call(wfunc, NULL, 0, NULL, 0, &wtrap);
    if (werror != NULL || wtrap != NULL) {
        ngx_wasmtime_log_error(NGX_LOG_ERR, instance->log, werror, wtrap,
                               "failed to call function \"%s\"", fname);
        goto failed;
    }

    rc = NGX_OK;

failed:

    if (wfunc != NULL) {
        wasm_func_delete(wfunc);
    }

    return rc;
}


static ngx_int_t
ngx_wasmtime_call(ngx_wasm_winstance_t *instance, const u_char *fname)
{
    return ngx_wasmtime_call_helper(instance, fname, 0);
}


static ngx_int_t
ngx_wasmtime_call_maybe(ngx_wasm_winstance_t *instance, const u_char *fname)
{
    return ngx_wasmtime_call_helper(instance, fname, 1);
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
