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


typedef struct ngx_wasmtime_trampoline_ctx_s  ngx_wasmtime_trampoline_ctx_t;
typedef struct ngx_wasmtime_instance_s  ngx_wasmtime_instance_t;


typedef struct ngx_wasmtime_engine_s {
    ngx_pool_t                     *pool;
    ngx_log_t                      *log;
    ngx_wasm_hfuncs_resolver_t     *hf_resolver;
    wasm_config_t                  *config;
    wasm_engine_t                  *engine;
    wasm_store_t                   *store;
} ngx_wasmtime_engine_t;


typedef struct ngx_wasmtime_module_s {
    ngx_wasmtime_engine_t          *engine;
    wasm_module_t                  *module;
    ngx_str_t                      *name;
    ngx_array_t                    *imports_hfuncs;
    wasm_importtype_vec_t           imports;
    wasm_exporttype_vec_t           exports;
} ngx_wasmtime_module_t;


struct ngx_wasmtime_instance_s {
    ngx_wasmtime_module_t           *module;
    wasm_store_t                    *store;
    wasm_instance_t                 *instance;
    ngx_wasmtime_trampoline_ctx_t   *ctxs;
    wasm_memory_t                   *memory;
    const wasm_extern_t            **imports;
    wasm_extern_vec_t                externs;
};


struct ngx_wasmtime_trampoline_ctx_s {
    ngx_wasmtime_instance_t         *instance;
    ngx_wasm_hfunc_t                *hfunc;
    ngx_wasm_hctx_t                 *hctx;
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
    size_t   i;

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


static wasm_trap_t *
ngx_wasmtime_trampoline(void *env, const wasm_val_t args[], wasm_val_t rets[])
{
    ngx_wasmtime_trampoline_ctx_t  *ctx = env;
    ngx_int_t                       rc;
    ngx_wasm_val_t                  ngx_args[NGX_WASM_ARGS_MAX];
    /*ngx_wasm_val_t                  ngx_rets[NGX_WASM_RETS_MAX];*/

    ngx_log_debug2(NGX_LOG_DEBUG_WASM, ctx->hctx->log, 0,
                   "wasmtime host function trampoline (hfunc: %V, hctx: %p)",
                   ctx->hfunc->name, ctx);

    ngx_wasmtime_vals_lift(ngx_args, (wasm_val_t *) args, ctx->hfunc->nargs);

    ctx->hctx->memory_offset = wasm_memory_data(ctx->instance->memory);

    rc = ctx->hfunc->ptr(ctx->hctx, ngx_args, NULL);
    if (rc != NGX_OK) {
        return NULL;
    }

    return NULL;
}


/* vm actions */


static void
ngx_wasmtime_engine_free(ngx_wasm_vm_engine_pt vm_engine)
{
    ngx_wasmtime_engine_t  *engine = vm_engine;

    if (engine->store) {
        wasm_store_delete(engine->store);
    }

    if (engine->engine) {
        wasm_engine_delete(engine->engine);
        // double free if after wasm_engine_delete
        //wasm_config_delete(engine->config);
    }

    ngx_pfree(engine->pool, engine);
}


static ngx_wasm_vm_engine_pt
ngx_wasmtime_engine_new(ngx_pool_t *pool,
    ngx_wasm_hfuncs_resolver_t *hf_resolver)
{
    ngx_wasmtime_engine_t           *engine;

    engine = ngx_pcalloc(pool, sizeof(ngx_wasmtime_engine_t));
    if (engine == NULL) {
        return NULL;
    }

    engine->pool = pool;
    engine->log = pool->log;
    engine->hf_resolver = hf_resolver;

    engine->config = wasm_config_new();

    //wasmtime_config_max_wasm_stack_set(config, (size_t) 125000 * 5);
    wasmtime_config_static_memory_maximum_size_set(engine->config, 0);

    engine->engine = wasm_engine_new_with_config(engine->config);
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
ngx_wasmtime_module_free(ngx_wasm_vm_module_pt vm_module)
{
    ngx_wasmtime_module_t       *module = vm_module;

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


static ngx_wasm_vm_module_pt
ngx_wasmtime_module_new(ngx_wasm_vm_engine_pt vm_engine, ngx_str_t *mod_name,
    ngx_str_t *bytes, ngx_uint_t flags, ngx_wasm_vm_error_pt *error)
{
    size_t                    i;
    ngx_wasmtime_engine_t    *engine = vm_engine;
    ngx_wasmtime_module_t    *module;
    ngx_wasm_hfunc_t         *hfunc, **hfuncp;
    wasm_byte_vec_t           wasm_bytes, wat_bytes;
    const wasm_externtype_t  *ext_type;
    wasm_externkind_t         ext_kind;
    const wasm_importtype_t  *imp_type;
    const wasm_name_t        *name_module, *name_func;

    if (flags & NGX_WASM_VM_MODULE_ISWAT) {
        wasm_byte_vec_new(&wat_bytes, bytes->len, (const char *) bytes->data);

        *error = wasmtime_wat2wasm(&wat_bytes, &wasm_bytes);

        wasm_byte_vec_delete(&wat_bytes);

        if (*error) {
            return NULL;
        }

    } else {
        wasm_byte_vec_new(&wasm_bytes, bytes->len, (const char *) bytes->data);
    }

    module = ngx_pcalloc(engine->pool, sizeof(ngx_wasmtime_module_t));
    if (module == NULL) {
        return NULL;
    }

    module->engine = engine;
    module->name = mod_name;

    *error = wasmtime_module_new(engine->engine, &wasm_bytes, &module->module);

    wasm_byte_vec_delete(&wasm_bytes);

    if (*error) {
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
        imp_type = module->imports.data[i];
        ext_type = wasm_importtype_type(imp_type);
        ext_kind = wasm_externtype_kind(ext_type);

        if (ext_kind != WASM_EXTERN_FUNC) {
            continue;
        }

        name_module = wasm_importtype_module(module->imports.data[i]);
        name_func = wasm_importtype_name(module->imports.data[i]);

        hfunc = ngx_wasm_hfuncs_resolver_lookup(engine->hf_resolver,
                    name_module->data, name_module->size,
                    name_func->data, name_func->size);
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
ngx_wasmtime_instance_free(ngx_wasm_vm_instance_pt vm_instance)
{
    size_t                    i;
    ngx_wasmtime_engine_t    *engine;
    ngx_wasmtime_instance_t  *instance = vm_instance;
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


ngx_wasm_vm_instance_pt
ngx_wasmtime_instance_new(ngx_wasm_vm_module_pt vm_module,
    ngx_wasm_hctx_t *hctx, ngx_wasm_vm_error_pt *error,
    ngx_wasm_vm_trap_pt *trap)
{
    size_t                           i;
    ngx_wasmtime_engine_t           *engine;
    ngx_wasmtime_module_t           *module = vm_module;
    ngx_wasmtime_instance_t         *instance;
    ngx_wasmtime_trampoline_ctx_t   *ctx;
    ngx_wasm_hfunc_t               **hfuncs, *hfunc;
    wasm_extern_t                   *ext;
    wasm_externkind_t                ext_kind;
    wasm_func_t                     *func;
#if (NGX_DEBUG)
    const wasm_exporttype_t  *exp_type;
    const wasm_name_t        *exp_name;
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

        ctx->instance = instance;
        ctx->hfunc = hfunc;
        ctx->hctx = hctx;

        func = wasm_func_new_with_env(instance->store,
                                      (wasm_functype_t *) hfunc->vm_data,
                                      &ngx_wasmtime_trampoline,
                                      ctx, NULL);

        instance->imports[i] = wasm_func_as_extern(func);
    }

    *error = wasmtime_instance_new(instance->store, module->module,
                                   (const wasm_extern_t * const *) instance->imports,
                                   i, &instance->instance,
                                   (wasm_trap_t **) trap);
    if (*error || *trap) {
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
ngx_wasmtime_instance_call(ngx_wasm_vm_instance_pt vm_instance,
    ngx_str_t *func_name, ngx_wasm_vm_error_pt *error,
    ngx_wasm_vm_trap_pt *trap)
{
    size_t                    i;
    ngx_wasmtime_instance_t  *instance = vm_instance;
    wasm_extern_t            *ext;
    wasm_externkind_t         ext_kind;
    const wasm_name_t        *exp_name;
    const wasm_exporttype_t  *exp_type;
    wasm_func_t              *func = NULL;
    ngx_int_t                 rc = NGX_ERROR;

    for (i = 0; i < instance->externs.size; i++) {
        ext = instance->externs.data[i];
        ext_kind = wasm_extern_kind(ext);

        exp_type = instance->module->exports.data[i];
        exp_name = wasm_exporttype_name(exp_type);

        if (ext_kind != WASM_EXTERN_FUNC) {
            continue;
        }

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

    *error = wasmtime_func_call(func, NULL, 0, NULL, 0, (wasm_trap_t **) trap);
    if (*error || *trap) {
        goto failed;
    }

    rc = NGX_OK;

failed:

    return rc;
}


static u_char *
ngx_wasmtime_log_error_handler(ngx_wasm_vm_error_pt error,
    ngx_wasm_vm_trap_pt trap, u_char *buf, size_t len)
{
    wasmtime_error_t  *werror = error;
    wasm_trap_t       *wtrap = trap;
    wasm_byte_vec_t    error_msg, trap_msg;//, trap_trace;
    u_char            *p = buf;

    if (wtrap) {
        ngx_memzero(&trap_msg, sizeof(wasm_byte_vec_t));
        //ngx_memzero(&trap_trace, sizeof(wasm_byte_vec_t));

        wasm_trap_message(wtrap, &trap_msg);
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
        wasm_trap_delete(wtrap);
    }

    if (werror) {
        ngx_memzero(&error_msg, sizeof(wasm_byte_vec_t));
        wasmtime_error_message(werror, &error_msg);
        wasmtime_error_delete(werror);

        p = ngx_snprintf(buf, len, " (%*s)",
                         error_msg.size, error_msg.data);
        len -= p - buf;
        buf = p;

        wasm_byte_vec_delete(&error_msg);
    }

    return p;
}


static void *
ngx_wasmtime_hfunc_new(ngx_wasm_hfunc_t *hfunc)
{
    size_t                i;
    wasm_valtype_vec_t    args, rets;
    wasm_valkind_t        valkind;
    wasm_valtype_t       *valtype;
    wasm_functype_t      *functype;

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
ngx_wasmtime_hfunc_free(void *vm_data)
{
    wasm_functype_delete((wasm_functype_t *) vm_data);
}


/* ngx_wasmtime_module */


static ngx_wasm_module_t  ngx_wasmtime_module_ctx = {
    ngx_string("wasmtime"),
    NULL,                                /* create configuration */
    NULL,                                /* init configuration */
    NULL,                                /* init module */
    NULL,                                /* exit process */
    NULL,                                /* exit master */
    {                                    /* VM actions */
        ngx_wasmtime_engine_new,
        ngx_wasmtime_engine_free,
        ngx_wasmtime_module_new,
        ngx_wasmtime_module_free,
        ngx_wasmtime_instance_new,
        ngx_wasmtime_instance_call,
        ngx_wasmtime_instance_free,
        ngx_wasmtime_log_error_handler,
        ngx_wasmtime_hfunc_new,
        ngx_wasmtime_hfunc_free
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
