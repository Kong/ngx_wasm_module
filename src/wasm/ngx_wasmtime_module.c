/*
 * Copyright (C) Thibault Charbonnier
 */

#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_config.h>
#include <ngx_wasm_module.h>
#include <ngx_wasm_hostfuncs.h>

#include <wasm.h>
#include <wasmtime.h>


static ngx_int_t ngx_wasmtime_init(ngx_cycle_t *cycle,
    ngx_wasm_vm_actions_t **vm_actions);


typedef struct {
    wasm_engine_t          *engine;
    wasm_store_t           *store;
    ngx_pool_t             *pool;
} ngx_wasmtime_engine_t;


typedef struct {
    wasm_importtype_vec_t   imports;
    wasm_exporttype_vec_t   exports;
    wasm_store_t           *store;
    wasm_module_t          *module;
    ngx_pool_t             *pool;
} ngx_wasmtime_module_t;


typedef struct {
    wasm_extern_vec_t       wasm_externs;
    wasm_exporttype_vec_t  *wasm_module_exports;
    wasm_instance_t        *wasm_instance;
    wasm_memory_t          *wasm_memory;
    ngx_pool_t             *pool;
    ngx_log_t              *log;
} ngx_wasmtime_instance_t;


static ngx_str_t  module_name = ngx_string("wasmtime");


/* vm actions */


static void
ngx_wasmtime_free_engine(ngx_wasm_vm_engine_pt vm_engine)
{
    ngx_wasmtime_engine_t       *engine = vm_engine;

    if (engine->store) {
        wasm_store_delete(engine->store);
    }

    if (engine->engine) {
        wasm_engine_delete(engine->engine);
    }

    ngx_pfree(engine->pool, engine);
}


static ngx_wasm_vm_engine_pt
ngx_wasmtime_new_engine(ngx_pool_t *pool)
{
    ngx_wasmtime_engine_t       *engine;

    engine = ngx_palloc(pool, sizeof(ngx_wasmtime_engine_t));
    if (engine == NULL) {
        return NULL;
    }

    engine->pool = pool;

    engine->engine = wasm_engine_new();
    if (engine->engine == NULL) {
        goto failed;
    }

    engine->store = wasm_store_new(engine->engine);
    if (engine->store == NULL) {
        goto failed;
    }

    return engine;

failed:

    ngx_wasmtime_free_engine(engine);

    return NULL;
}


static void
ngx_wasmtime_free_module(ngx_wasm_vm_module_pt vm_module)
{
    ngx_wasmtime_module_t       *module = vm_module;

    if (module->module != NULL) {
        wasm_importtype_vec_delete(&module->imports);
        wasm_exporttype_vec_delete(&module->exports);
        wasm_module_delete(module->module);
    }

    ngx_pfree(module->pool, module);
}


static ngx_wasm_vm_module_pt
ngx_wasmtime_new_module(ngx_wasm_vm_engine_pt vm_engine,
    ngx_wasm_vm_bytes_t *bytes, ngx_uint_t flags,
    ngx_wasm_vm_error_pt *vm_error)
{
    ngx_wasmtime_engine_t       *engine = vm_engine;
    ngx_wasmtime_module_t       *module;
    wasm_byte_vec_t              wasm_bytes, wat_bytes;
    wasmtime_error_t           **error = (wasmtime_error_t **) vm_error;

    if (flags & NGX_WASM_VM_MODULE_ISWAT) {
        wat_bytes.size = bytes->len;
        wat_bytes.data = (char * ) bytes->data;

        *error = wasmtime_wat2wasm(&wat_bytes, &wasm_bytes);
        if (*error) {
            return NULL;
        }

    } else {
        wasm_bytes.size = bytes->len;
        wasm_bytes.data = (char *) bytes->data;
    }

    module = ngx_palloc(engine->pool, sizeof(ngx_wasmtime_module_t));
    if (module == NULL) {
        return NULL;
    }

    module->pool = engine->pool;
    module->store = engine->store;

    *error = wasmtime_module_new(module->store, &wasm_bytes, &module->module);
    if (*error) {
        goto failed;
    }

    wasm_module_imports(module->module, &module->imports);
    wasm_module_exports(module->module, &module->exports);

#if 0
    const wasm_name_t           *wname;
    size_t                       i;

    ngx_log_error(NGX_LOG_NOTICE, ngx_cycle->log, 0,
                  "wasm number of imports: %d", module->imports.size);

    for (i = 0; i < module->imports.size; i++) {
        wname = wasm_importtype_name(module->imports.data[i]);
        //wasm_externtype_t* wasm_importtype_type(const wasm_importtype_t*)

        if (wname->data) {
            ngx_log_error(NGX_LOG_NOTICE, ngx_cycle->log, 0,
                          "wasm WNAME: \"%V\"", wname);

            ngx_wasm_hostfunc_pt hfunc = ngx_wasm_hostfuncs_find("http", 4, wname->data, wname->size);

            ngx_log_error(NGX_LOG_NOTICE, ngx_cycle->log, 0,
                          "wasm has a host func?: \"%p\"", hfunc);

        } else {
            ngx_log_error(NGX_LOG_NOTICE, ngx_cycle->log, 0,
                          "wasm WNAME: nothing");
        }

        wname = wasm_importtype_module(module->imports.data[i]);
            ngx_log_error(NGX_LOG_NOTICE, ngx_cycle->log, 0,
                          "wasm module name: \"%V\"", wname);
    }
#endif

    return module;

failed:

    ngx_wasmtime_free_module(module);

    return NULL;
}


//static wasm_memory_t *current_memory;



/*
static ngx_wasm_vm_instance_t
ngx_wasmtime_new_instance(ngx_wasm_vm_module_t vm_module,
    ngx_wasm_whostfunc_t *hostfuncs, size_t nhostfuncs,
    ngx_wasm_vm_error_t *vm_error, ngx_wasm_vm_trap_t *vm_trap)
{
    ngx_wasmtime_winstance_t    *wtinstance;
    ngx_wasmtime_wmodule_t      *wtmodule = vm_module;
    ngx_wasm_whostfunc_t        *hostfunc;
    wasm_func_t                 *wfunc;
    intptr_t                    *wimports;
    const wasm_name_t           *wname;
    wasm_extern_t               *wmextern = NULL;
    wasmtime_error_t           **wterror = (wasmtime_error_t **) vm_error;
    wasm_trap_t                **wtrap = (wasm_trap_t **) vm_trap;
    size_t                       i;

    ngx_wasm_assert(wtmodule->wasm_module != NULL);

    wtinstance = ngx_palloc(wtmodule->pool, sizeof(ngx_wasmtime_winstance_t));
    if (wtinstance == NULL) {
        return NULL;
    }

    wtinstance->pool = wtmodule->pool;

    wasm_functype_t* log_type = wasm_functype_new_3_0(
            wasm_valtype_new_i32(),
            wasm_valtype_new_i32(),
            wasm_valtype_new_i32());

    wasm_functype_t* void_type = wasm_functype_new_0_0();

    //wasm_func_t* wlog = wasm_func_new(wtmodule->wasm_store, log_type, ngx_wasm_log);
    //wasm_func_t* wlog = wasm_func_new_with_env(wtmodule->wasm_store, log_type, ngx_wasm_log, wtinstance, finalizer);
    wasm_func_t* wlog = wasm_func_new_with_env(wtmodule->wasm_store, log_type, ngx_wasmtime_bridge_callback, wtinstance, finalizer);

    wasm_functype_delete(log_type);
    wasm_functype_delete(void_type);
    ngx_wasm_assert(wlog != NULL);

    const wasm_extern_t* imports[] = {wasm_func_as_extern(wlog)};

    ngx_log_error(NGX_LOG_EMERG, ngx_cycle->log, 0, "wasm before imports");

    if (nhostfuncs > 0) {
        wimports = ngx_palloc(wtmodule->pool, nhostfuncs * sizeof(intptr_t));
        if (wimports == NULL) {
            goto failed;
        }

        for (i = 0; i < nhostfuncs; i++) {
            hostfunc = &hostfuncs[i];

            wfunc = wasm_func_new_with_env(wtmodule->wasm_store,
                                           hostfunc->vm_functype,
                                           ngx_wasmtime_callback,
                                           wtinstance, NULL);

            wimports[i] = wasm_func_as_extern(wfunc);
        }
    }

    ngx_log_error(NGX_LOG_EMERG, ngx_cycle->log, 0, "wasm after imports");

    *wterror = wasmtime_instance_new(wtmodule->wasm_module,
                                     (const wasm_extern_t * const*) wimports,
                                     nhostfuncs, &wtinstance->wasm_instance,
                                     wtrap);
    if (*wterror || *wtrap) {
        goto failed;
    }

    wtinstance->wasm_module_exports = &wtmodule->wasm_exports;
    wasm_instance_exports(wtinstance->wasm_instance,
                          &wtinstance->wasm_externs);

    for (i = 0; i < wtinstance->wasm_externs.size; i++) {
        wname = wasm_exporttype_name(wtinstance->wasm_module_exports->data[i]);

        ngx_wasm_log_error(NGX_LOG_NOTICE, ngx_cycle->log, 0, NULL,
                           "WNAME: \"%V\"",
                           wname);

        if (ngx_strncmp(wname->data, "memory", 6) == 0) {
            ngx_wasm_log_error(NGX_LOG_NOTICE, ngx_cycle->log, 0, NULL,
                               "GOT MEMORY");

            wmextern = wtinstance->wasm_externs.data[i];

            wtinstance->wasm_memory = wasm_extern_as_memory(wmextern);

            ngx_wasm_assert(wtinstance->wasm_memory != NULL);

            current_memory = wtinstance->wasm_memory;
        }
    }

    return wtinstance;

failed:

    ngx_wasmtime_free_instance(wtinstance);

    return NULL;
}


static void
ngx_wasmtime_free_instance(ngx_wasm_vm_instance_t vm_instance)
{
    ngx_wasmtime_winstance_t       *wtinstance = vm_instance;

    if (wtinstance->wasm_instance != NULL) {
        wasm_instance_delete(wtinstance->wasm_instance);
    }

    ngx_pfree(wtinstance->pool, wtinstance);
}


static ngx_int_t
ngx_wasmtime_call_instance(ngx_wasm_vm_instance_t vm_instance,
    const u_char *fname, const ngx_wasm_wval_t *args, size_t nargs,
    ngx_wasm_wval_t *rets, size_t nrets, ngx_wasm_vm_error_t *vm_error,
    ngx_wasm_vm_trap_t *vm_trap)
{
    ngx_wasmtime_winstance_t    *wtinstance = vm_instance;
    const wasm_name_t           *wname;
    wasm_extern_t               *wfextern = NULL;
    wasm_func_t                 *wfunc;
    wasm_val_t                   wargs[nargs], wrets[nrets];
    wasmtime_error_t           **wterror = (wasmtime_error_t **) vm_error;
    wasm_trap_t                **wtrap = (wasm_trap_t **) vm_trap;
    size_t                       i;
    ngx_int_t                    rc = NGX_ERROR;

    for (i = 0; i < wtinstance->wasm_externs.size; i++) {
        wname = wasm_exporttype_name(wtinstance->wasm_module_exports->data[i]);

        ngx_wasm_log_error(NGX_LOG_NOTICE, ngx_cycle->log, 0, NULL,
                           "WNAME: \"%V\"",
                           wname);

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

    //ngx_wasmtime_wvals_to_vmvals(wargs, args, nargs);

    *wterror = wasmtime_func_call(wfunc, wargs, nargs, wrets, nrets, wtrap);
    if (*wterror || *wtrap) {
        goto failed;
    }

    //ngx_wasmtime_vmvals_to_wvals(rets, wrets, nrets);

    rc = NGX_OK;

failed:

    if (wfunc != NULL) {
        wasm_func_delete(wfunc);
    }

    return rc;
}
*/


/*
static wasm_val_t
ngx_wasmtime_wval_to_vmval(ngx_wasm_wval_t val)
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
ngx_wasmtime_vmval_to_wval(ngx_wasm_vm_val_t vm_val)
{
    wasm_val_t          wval = vm_val;
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
*/


static u_char *
ngx_wasmtime_log_error_handler(ngx_wasm_vm_error_pt vm_error,
    ngx_wasm_vm_trap_pt vm_trap, u_char *buf, size_t len)
{
    wasmtime_error_t            *error = vm_error;
    wasm_trap_t                 *trap = vm_trap;
    wasm_byte_vec_t              error_msg, trap_msg;//, trap_trace;
    u_char                      *p = buf;

    if (trap) {
        ngx_memzero(&trap_msg, sizeof(wasm_byte_vec_t));
        //ngx_memzero(&trap_trace, sizeof(wasm_byte_vec_t));

        wasm_trap_message(trap, &trap_msg);
        //wasm_trap_trace(trap, &trap_trace);

        p = ngx_snprintf(buf, len, " | %*s |",
                         trap_msg.size, trap_msg.data);
        len -= p - buf;
        buf = p;

#if 0
        if (trap_trace.size) {
            p = ngx_snprintf(buf, len, "\n%*s",
                             trap_trace.size, trap_trace.data);
            len -= p - buf;
            buf = p;
        }
#endif

        wasm_byte_vec_delete(&trap_msg);
        //wasm_byte_vec_delete(&trap_trace);
        wasm_trap_delete(trap);
    }

    if (vm_error) {
        ngx_memzero(&error_msg, sizeof(wasm_byte_vec_t));
        wasmtime_error_message(error, &error_msg);
        wasmtime_error_delete(error);

        p = ngx_snprintf(buf, len, " (%*s)",
                         error_msg.size, error_msg.data);
        len -= p - buf;
        buf = p;

        wasm_byte_vec_delete(&error_msg);
    }

    return p;
}


/* ngx_wasmtime_module */


static ngx_wasm_module_t  ngx_wasmtime_module_ctx = {
    &module_name,
    NULL,                                /* create configuration */
    NULL,                                /* init configuration */
    ngx_wasmtime_init,                   /* init module */
    {                                    /* VM actions */
        &module_name,
        ngx_wasmtime_new_engine,
        ngx_wasmtime_free_engine,
        ngx_wasmtime_new_module,
        ngx_wasmtime_free_module,
        ngx_wasmtime_log_error_handler
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


static ngx_int_t
ngx_wasmtime_init(ngx_cycle_t *cycle, ngx_wasm_vm_actions_t **vm_actions)
{
    *vm_actions = &ngx_wasmtime_module_ctx.vm_actions;

    return NGX_OK;
}


/* vi:set ft=c ts=4 sw=4 et fdm=marker: */
