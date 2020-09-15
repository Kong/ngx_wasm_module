/*
 * Copyright (C) Thibault Charbonnier
 */

#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_core.h>
#include <ngx_wasm.h>

#include <wasm.h>
#include <wasmtime.h>


typedef struct ngx_wasmtime_trampoline_ctx_s  ngx_wasmtime_trampoline_ctx_t;
typedef struct ngx_wasmtime_instance_s  ngx_wasmtime_instance_t;


typedef struct ngx_wasmtime_engine_s {
    ngx_pool_t                      *pool;
    ngx_log_t                       *log;
    wasm_config_t                   *config;
    wasm_engine_t                   *engine;
    wasm_store_t                    *store;
} ngx_wasmtime_engine_t;


typedef struct ngx_wasmtime_module_s {
    ngx_wasmtime_engine_t           *engine;
    ngx_str_t                       *name;
    ngx_array_t                     *imports_hfuncs;
    wasm_module_t                   *module;
    wasm_importtype_vec_t            imports;
    wasm_exporttype_vec_t            exports;
} ngx_wasmtime_module_t;


struct ngx_wasmtime_instance_s {
    ngx_wasmtime_module_t           *module;
    ngx_wasmtime_trampoline_ctx_t   *ctxs;
    wasm_store_t                    *store;
    wasm_instance_t                 *instance;
    wasm_memory_t                   *memory;
    wasm_extern_t                  **imports;
    wasm_extern_vec_t                externs;
};


struct ngx_wasmtime_trampoline_ctx_s {
    ngx_wasm_hctx_t                 *hctx;
    ngx_wasm_hfunc_t                *hfunc;
    ngx_wasmtime_instance_t         *wrt;
};


/* utils */


static ngx_inline wasm_valkind_t
ngx_wasmtime_valkind_lower(ngx_wasm_val_kind kind)
{
    switch (kind) {

    case NGX_WASM_I32:
        return WASM_I32;

    case NGX_WASM_I64:
        return WASM_I64;

    case NGX_WASM_F32:
        return WASM_F32;

    case NGX_WASM_F64:
        return WASM_F64;

    default:
        ngx_wasm_assert(0);
        return 0;

    }
}


#if 0
static ngx_inline ngx_wasm_val_kind
ngx_wasmtime_valkind_lift(wasm_valkind_t kind)
{
    switch (kind) {

    case WASM_I32:
        return NGX_WASM_I32;

    case WASM_I64:
        return NGX_WASM_I64;

    case WASM_F32:
        return NGX_WASM_F32;

    case WASM_F64:
        return NGX_WASM_F64;

    default:
        ngx_wasm_assert(0);
        return 0;

    }
}
#endif


static ngx_inline void
ngx_wasmtime_vals_lift(ngx_wasm_val_t ngx_vals[], wasm_val_t wasm_vals[],
    ngx_uint_t nvals)
{
    size_t  i;

    for (i = 0; i < nvals; i++) {

        switch (wasm_vals[i].kind) {

        case WASM_I32:
            ngx_vals[i].kind = NGX_WASM_I32;
            ngx_vals[i].value.I32 = wasm_vals[i].of.i32;
            break;

        case WASM_I64:
            ngx_vals[i].kind = NGX_WASM_I64;
            ngx_vals[i].value.I64 = wasm_vals[i].of.i64;
            break;

        case WASM_F32:
            ngx_vals[i].kind = NGX_WASM_F32;
            ngx_vals[i].value.F32 = wasm_vals[i].of.f32;
            break;

        case WASM_F64:
            ngx_vals[i].kind = NGX_WASM_F64;
            ngx_vals[i].value.F64 = wasm_vals[i].of.f64;
            break;

        default:
            ngx_wasm_assert(0);
            break;

        }
    }
}


static ngx_inline void
ngx_wasmtime_vals_lower(wasm_val_t wasm_vals[], ngx_wasm_val_t ngx_vals[],
    ngx_uint_t nvals)
{
    size_t  i;

    for (i = 0; i < nvals; i++) {

        switch (ngx_vals[i].kind) {

        case NGX_WASM_I32:
            wasm_vals[i].kind = WASM_I32;
            wasm_vals[i].of.i32 = ngx_vals[i].value.I32;
            break;

        case NGX_WASM_I64:
            wasm_vals[i].kind = WASM_I64;
            wasm_vals[i].of.i64 = ngx_vals[i].value.I64;
            break;

        case NGX_WASM_F32:
            wasm_vals[i].kind = WASM_F32;
            wasm_vals[i].of.f32 = ngx_vals[i].value.F32;
            break;

        case NGX_WASM_F64:
            wasm_vals[i].kind = WASM_F64;
            wasm_vals[i].of.f64 = ngx_vals[i].value.F64;
            break;

        default:
            ngx_wasm_assert(0);
            break;

        }
    }
}


static wasm_trap_t *
ngx_wasmtime_trampoline(void *env, const wasm_val_t args[], wasm_val_t rets[])
{
    ngx_int_t                       rc;
    ngx_wasm_val_t                  ngx_args[NGX_WASM_ARGS_MAX];
    ngx_wasm_val_t                  ngx_rets[NGX_WASM_RETS_MAX];
    ngx_wasmtime_trampoline_ctx_t  *ctx = env;
    wasm_message_t                  trap_msg;

    ctx->hctx->mem_off = wasm_memory_data(ctx->wrt->memory);

    ngx_log_debug2(NGX_LOG_DEBUG_WASM, ctx->hctx->log, 0,
                   "wasmtime host function trampoline (hfunc: %V, ctx: %p)",
                   &ctx->hfunc->name, ctx);

    ngx_wasmtime_vals_lift(ngx_args, (wasm_val_t *) args, ctx->hfunc->nargs);

    rc = ctx->hfunc->ptr(ctx->hctx, ngx_args, ngx_rets);
    if (rc == NGX_WASM_ERROR) {
        wasm_name_new_from_string(&trap_msg, "nginx error in host function");
        return wasm_trap_new(ctx->wrt->store, &trap_msg);

    } else if (rc == NGX_WASM_BAD_CTX) {
        wasm_name_new_from_string(&trap_msg, "bad context");
        return wasm_trap_new(ctx->wrt->store, &trap_msg);
    }

    /* NGX_WASM_OK, NGX_WASM_AGAIN */

    ngx_wasmtime_vals_lower(rets, ngx_rets, ctx->hfunc->nrets);

    return NULL;
}


/* vm actions */


static void
ngx_wasmtime_engine_free(ngx_wrt_engine_pt wrt)
{
    ngx_wasmtime_engine_t  *engine = wrt;

    if (engine->store) {
        wasm_store_delete(engine->store);
    }

    if (engine->engine) {
        wasm_engine_delete(engine->engine);
    }

    ngx_pfree(engine->pool, engine);
}


static ngx_wrt_engine_pt
ngx_wasmtime_engine_new(ngx_pool_t *pool)
{
    ngx_wasmtime_engine_t  *engine;

    engine = ngx_pcalloc(pool, sizeof(ngx_wasmtime_engine_t));
    if (engine == NULL) {
        return NULL;
    }

    engine->pool = pool;
    engine->log = pool->log;

#if 1
    engine->config = wasm_config_new();

    //wasmtime_config_max_wasm_stack_set(config, (size_t) 125000 * 5);
    //wasmtime_config_static_memory_maximum_size_set(engine->config, 0);
    //wasmtime_config_profiler_set(engine->config, WASMTIME_PROFILING_STRATEGY_JITDUMP);
    //wasmtime_config_debug_info_set(engine->config, 1);

    engine->engine = wasm_engine_new_with_config(engine->config);
#else
    engine->engine = wasm_engine_new();
#endif
    if (engine->engine == NULL) {
        goto failed;
    }

    engine->store = wasm_store_new(engine->engine);
    if (engine->store == NULL) {
        goto failed;
    }

    return engine;

failed:

    ngx_wasmtime_engine_free(engine);

    return NULL;
}


static void
ngx_wasmtime_module_free(ngx_wrt_module_pt wrt)
{
    ngx_wasmtime_module_t  *module = wrt;

    if (module->module) {
        wasm_importtype_vec_delete(&module->imports);
        wasm_exporttype_vec_delete(&module->exports);
        wasm_module_delete(module->module);
    }

    if (module->imports_hfuncs) {
        ngx_array_destroy(module->imports_hfuncs);
    }

    ngx_pfree(module->engine->pool, module);
}


static ngx_wrt_error_pt
ngx_wasmtime_wat2wasm(ngx_wrt_engine_pt wrt, u_char *wat, size_t len,
    ngx_str_t *wasm)
{
    ngx_wasmtime_engine_t  *engine = wrt;
    wasm_byte_vec_t         wasm_bytes, wat_bytes;
    wasmtime_error_t       *err;

    wasm_byte_vec_new(&wat_bytes, len, (const char *) wat);

    err = wasmtime_wat2wasm(&wat_bytes, &wasm_bytes);

    wasm_byte_vec_delete(&wat_bytes);

    if (err) {
        return err;
    }

    wasm->len = wasm_bytes.size;
    wasm->data = ngx_alloc(wasm->len, engine->log);
    if (wasm->data == NULL) {
        return NULL;
    }

    ngx_memcpy(wasm->data, wasm_bytes.data, wasm_bytes.size);

    wasm_byte_vec_delete(&wasm_bytes);

    return NULL;
}


static ngx_wrt_module_pt
ngx_wasmtime_module_new(ngx_wrt_engine_pt wrt, ngx_wasm_hfuncs_t *hfuncs,
    ngx_str_t *mod_name, ngx_str_t *bytes, ngx_wrt_error_pt *err)
{
    size_t                    i;
    ngx_wasm_hfunc_t         *hfunc, **hfuncp;
    ngx_wasmtime_engine_t    *engine = wrt;
    ngx_wasmtime_module_t    *module;
    wasm_byte_vec_t           wasm_bytes;
    const wasm_externtype_t  *ext_type;
    wasm_externkind_t         ext_kind;
    const wasm_name_t        *name_module, *name_func;

    wasm_byte_vec_new(&wasm_bytes, bytes->len, (const char *) bytes->data);

    module = ngx_pcalloc(engine->pool, sizeof(ngx_wasmtime_module_t));
    if (module == NULL) {
        wasm_byte_vec_delete(&wasm_bytes);
        return NULL;
    }

    module->engine = engine;
    module->name = mod_name;

    *err = wasmtime_module_new(engine->engine, &wasm_bytes, &module->module);

    wasm_byte_vec_delete(&wasm_bytes);

    if (*err) {
        goto failed;
    }

    wasm_module_imports(module->module, &module->imports);
    wasm_module_exports(module->module, &module->exports);

    module->imports_hfuncs = ngx_array_create(engine->pool,
                                              module->imports.size,
                                              sizeof(ngx_wasm_hfunc_t *));
    if (module->imports_hfuncs == NULL) {
        goto failed;
    }

    for (i = 0; i < module->imports.size; i++) {
        ext_type = wasm_importtype_type(module->imports.data[i]);
        ext_kind = wasm_externtype_kind(ext_type);

        if (ext_kind != WASM_EXTERN_FUNC) {
            continue;
        }

        name_module = wasm_importtype_module(module->imports.data[i]);
        name_func = wasm_importtype_name(module->imports.data[i]);

        hfunc = ngx_wasm_hfuncs_lookup(hfuncs,
                               (u_char *) name_module->data, name_module->size,
                               (u_char *) name_func->data, name_func->size);
        if (hfunc == NULL) {
            /* let instantiation fail later on */
            continue;
        }

        hfuncp = ngx_array_push(module->imports_hfuncs);
        if (hfuncp == NULL) {
            goto failed;
        }

        *hfuncp = hfunc;
    }

    return module;

failed:

    ngx_wasmtime_module_free(module);

    return NULL;
}


void
ngx_wasmtime_instance_free(ngx_wrt_instance_pt wrt)
{
    size_t                    i;
    ngx_wasmtime_instance_t  *instance = wrt;
    ngx_wasmtime_engine_t    *engine;
    wasm_func_t              *func;

    engine = instance->module->engine;

    if (instance->instance) {
        wasm_extern_vec_delete(&instance->externs);
        wasm_instance_delete(instance->instance);
    }

    if (instance->ctxs) {
        ngx_pfree(engine->pool, instance->ctxs);
    }

    if (instance->imports) {
        for (i = 0; i < instance->module->imports_hfuncs->nelts; i++) {
            func = wasm_extern_as_func((wasm_extern_t *) instance->imports[i]);
            wasm_func_delete(func);
        }

        ngx_pfree(engine->pool, instance->imports);
    }

    if (instance->store) {
        wasm_store_delete(instance->store);
    }

    ngx_pfree(engine->pool, instance);
}


ngx_wrt_instance_pt
ngx_wasmtime_instance_new(ngx_wrt_module_pt wrt,
    ngx_wasm_hctx_t *hctx, ngx_wrt_error_pt *err,
    ngx_wrt_trap_pt *trap)
{
    size_t                           i;
    ngx_wasm_hfunc_t               **hfuncs, *hfunc;
    ngx_wasmtime_module_t           *module = wrt;
    ngx_wasmtime_engine_t           *engine;
    ngx_wasmtime_instance_t         *instance;
    ngx_wasmtime_trampoline_ctx_t   *ctx;
    wasm_func_t                     *func;
    wasm_extern_t                   *ext;
    wasm_externkind_t                ext_kind;
#if (NGX_DEBUG)
    const wasm_exporttype_t         *exp_type;
    const wasm_name_t               *exp_name;
#endif

    engine = module->engine;

    instance = ngx_palloc(engine->pool, sizeof(ngx_wasmtime_instance_t));
    if (instance == NULL) {
        return NULL;
    }

    instance->module = module;

    instance->store = wasm_store_new(engine->engine);
    if (instance->store == NULL) {
        goto failed;
    }

    instance->imports = ngx_palloc(engine->pool, sizeof(wasm_extern_t *) *
                                   module->imports_hfuncs->nelts);
    if (instance->imports == NULL) {
        goto failed;
    }

    instance->ctxs = ngx_palloc(engine->pool,
                                sizeof(ngx_wasmtime_trampoline_ctx_t) *
                                module->imports_hfuncs->nelts);
    if (instance->ctxs == NULL) {
        goto failed;
    }

    for (hfuncs = module->imports_hfuncs->elts, i = 0;
         i < module->imports_hfuncs->nelts;
         i++)
    {
        hfunc = hfuncs[i];

        ctx = &instance->ctxs[i];

        ctx->hctx = hctx;
        ctx->hfunc = hfunc;
        ctx->wrt = instance;

        func = wasm_func_new_with_env(instance->store,
                                      (wasm_functype_t *) hfunc->wrt_functype,
                                      &ngx_wasmtime_trampoline,
                                      ctx, NULL);

        instance->imports[i] = wasm_func_as_extern(func);
    }

    ngx_log_debug2(NGX_LOG_DEBUG_WASM, engine->log, 0,
                   "wasmtime creating \"%V\" instance (imports: %d)",
                   module->name, i);

    *err = wasmtime_instance_new(instance->store, module->module,
                                   (const wasm_extern_t * const *) instance->imports,
                                   i, &instance->instance,
                                   (wasm_trap_t **) trap);
    if (*err || *trap) {
        goto failed;
    }

    wasm_instance_exports(instance->instance, &instance->externs);

    for (i = 0; i < instance->externs.size; i++) {
        ext = instance->externs.data[i];
        ext_kind = wasm_extern_kind(ext);

#if (NGX_DEBUG)
        exp_type = module->exports.data[i];
        exp_name = wasm_exporttype_name(exp_type);

        ngx_log_debug5(NGX_LOG_DEBUG_WASM, engine->log, 0,
                       "wasmtime new \"%V\" instance extern %d/%d: \"%*s\"",
                       module->name, i + 1, instance->externs.size,
                       exp_name->size, exp_name->data);
#endif

        switch (ext_kind) {

        case WASM_EXTERN_MEMORY:
            instance->memory = wasm_extern_as_memory(ext);
            break;

        default:
            break;

        }
    }

    goto done;

failed:

    ngx_wasmtime_instance_free(instance);

    instance = NULL;

done:

    return instance;
}


static ngx_int_t
ngx_wasmtime_instance_call(ngx_wrt_instance_pt wrt,
    ngx_str_t *func_name, ngx_wrt_error_pt *err,
    ngx_wrt_trap_pt *trap)
{
    size_t                    i;
    ngx_int_t                 rc = NGX_ERROR;
    ngx_wasmtime_instance_t  *instance = wrt;
    wasm_extern_t            *ext;
    wasm_externkind_t         ext_kind;
    const wasm_name_t        *exp_name;
    const wasm_exporttype_t  *exp_type;
    wasm_func_t              *func = NULL;

    for (i = 0; i < instance->externs.size; i++) {
        ext = instance->externs.data[i];
        ext_kind = wasm_extern_kind(ext);

        if (ext_kind != WASM_EXTERN_FUNC) {
            continue;
        }

        exp_type = instance->module->exports.data[i];
        exp_name = wasm_exporttype_name(exp_type);

        if (exp_name->size != func_name->len) {
            continue;
        }

        if (ngx_strncmp(exp_name->data, func_name->data, func_name->len) == 0) {
            func = wasm_extern_as_func(ext);
            break;
        }
    }

    if (func == NULL) {
        return NGX_DECLINED;
    }

    *err = wasmtime_func_call(func, NULL, 0, NULL, 0, (wasm_trap_t **) trap);
    if (*err || *trap) {
        goto failed;
    }

    rc = NGX_OK;

failed:

    return rc;
}


static void *
ngx_wasmtime_hfunctype_new(ngx_wasm_hfunc_t *hfunc)
{
    size_t               i;
    wasm_valtype_vec_t   args, rets;
    wasm_valkind_t       valkind;
    wasm_valtype_t      *valtype;
    wasm_functype_t     *functype;

    wasm_valtype_vec_new_uninitialized(&args, hfunc->nargs);
    wasm_valtype_vec_new_uninitialized(&rets, hfunc->nrets);

    for (i = 0; i < hfunc->nargs; i++) {
        valkind = ngx_wasmtime_valkind_lower(hfunc->args[i]);
        valtype = wasm_valtype_new(valkind);
        args.data[i] = wasm_valtype_copy(valtype);
        wasm_valtype_delete(valtype);
    }

    for (i = 0; i < hfunc->nrets; i++) {
        valkind = ngx_wasmtime_valkind_lower(hfunc->rets[i]);
        valtype = wasm_valtype_new(valkind);
        rets.data[i] = wasm_valtype_copy(valtype);
        wasm_valtype_delete(valtype);
    }

    functype = wasm_functype_new(&args, &rets);

    wasm_valtype_vec_delete(&args);
    wasm_valtype_vec_delete(&rets);

    return functype;
}


static void
ngx_wasmtime_hfunctype_free(void *vm_data)
{
    wasm_functype_delete((wasm_functype_t *) vm_data);
}


static u_char *
ngx_wasmtime_log_error_handler(ngx_wrt_error_pt err,
    ngx_wrt_trap_pt trap, u_char *buf, size_t len)
{
    u_char            *p = buf;
    wasm_byte_vec_t    error_msg, trap_msg;
    wasm_trap_t       *wtrap = trap;
    wasmtime_error_t  *werror = err;

    if (wtrap) {
        ngx_memzero(&trap_msg, sizeof(wasm_byte_vec_t));

        wasm_trap_message(wtrap, &trap_msg);

        p = ngx_snprintf(buf, len, " trap: %*s",
                         trap_msg.size, trap_msg.data);
        len -= p - buf;
        buf = p;

        wasm_byte_vec_delete(&trap_msg);
        wasm_trap_delete(wtrap);
    }

    if (werror) {
        ngx_memzero(&error_msg, sizeof(wasm_byte_vec_t));
        wasmtime_error_message(werror, &error_msg);
        wasmtime_error_delete(werror);

        p = ngx_snprintf(buf, len, " (%*s)",
                         error_msg.size, error_msg.data);

        wasm_byte_vec_delete(&error_msg);
    }

    return p;
}


/* ngx_wasmtime_module */


static ngx_wrt_t  ngx_wasmtime_runtime = {
    ngx_string("wasmtime"),

    ngx_wasmtime_engine_new,
    ngx_wasmtime_engine_free,

    ngx_wasmtime_wat2wasm,
    ngx_wasmtime_module_new,
    ngx_wasmtime_module_free,

    ngx_wasmtime_instance_new,
    ngx_wasmtime_instance_call,
    ngx_wasmtime_instance_free,

    ngx_wasmtime_hfunctype_new,
    ngx_wasmtime_hfunctype_free,

    ngx_wasmtime_log_error_handler
};


static ngx_wasm_module_t  ngx_wasmtime_module_ctx = {
    &ngx_wasmtime_runtime,               /* runtime */
    NULL,                                /* hfuncs */
    NULL,                                /* create configuration */
    NULL,                                /* init configuration */
    NULL,                                /* init module */
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
