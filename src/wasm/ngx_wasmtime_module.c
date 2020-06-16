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


#define ngx_wasm_bytes_set(name, str)                                         \
    (name)->size = (str)->len; (name)->data = (char *) (str)->data


typedef struct {
    wasm_engine_t          *engine;
    wasm_store_t           *store;
    wasmtime_linker_t      *linker;
    ngx_pool_t             *pool;
    ngx_log_t              *log;
} ngx_wasmtime_engine_t;


typedef struct {
    wasm_importtype_vec_t   imports;
    wasm_exporttype_vec_t   exports;
    ngx_wasmtime_engine_t  *engine;
    ngx_str_t              *name;
    wasm_module_t          *module;
} ngx_wasmtime_module_t;


typedef struct {
    wasm_extern_vec_t       externs;
    ngx_wasmtime_engine_t  *engine;
    wasm_memory_t          *memory;
    wasm_instance_t        *instance;
} ngx_wasmtime_instance_t;


static ngx_str_t  module_name = ngx_string("wasmtime");


/* utils */


ngx_inline static wasm_valkind_t
ngx_wasmtime_val_htow(ngx_wasm_vm_val_kind kind)
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


ngx_inline static ngx_wasm_vm_val_kind
ngx_wasmtime_val_wtoh(wasm_valkind_t kind)
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

    if (engine->linker) {
        wasmtime_linker_delete(engine->linker);
    }

    ngx_pfree(engine->pool, engine);
}


static ngx_wasm_vm_engine_pt
ngx_wasmtime_new_engine(ngx_pool_t *pool, ngx_wasm_vm_error_pt *vm_error)
{
    ngx_wasmtime_engine_t           *engine;
    ngx_wasm_hostfuncs_namespaces_t *namespaces;
    ngx_wasm_hostfuncs_namespace_t  *nn;
    ngx_rbtree_node_t               *node, *root, *sentinel;
    size_t                           i, nargs, nrets;
    ngx_wasm_hostfunc_t             *hfunc, **hfuncs;
    wasm_name_t                      module_name, hfunc_name;
    wasm_valtype_t                  *valtype;
    wasm_valtype_vec_t               args, rets;
    wasm_functype_t                 *functype;
    wasm_func_t                     *func;
    wasm_extern_t                   *func_extern;
    wasmtime_error_t               **error = (wasmtime_error_t **) vm_error;

    engine = ngx_palloc(pool, sizeof(ngx_wasmtime_engine_t));
    if (engine == NULL) {
        return NULL;
    }

    engine->pool = pool;
    engine->log = pool->log;

    engine->engine = wasm_engine_new();
    if (engine->engine == NULL) {
        goto failed;
    }

    engine->store = wasm_store_new(engine->engine);
    if (engine->store == NULL) {
        goto failed;
    }

    engine->linker = wasmtime_linker_new(engine->store);
    if (engine->linker == NULL) {
        goto failed;
    }

    namespaces = ngx_wasm_hostfuncs_namespaces();

    sentinel = namespaces->rbtree.sentinel;
    root = namespaces->rbtree.root;

    if (root == sentinel) {
        goto done;
    }

    for (node = ngx_rbtree_min(root, sentinel);
         node;
         node = ngx_rbtree_next(&namespaces->rbtree, node))
    {
        nn = (ngx_wasm_hostfuncs_namespace_t *) node;

        ngx_wasm_bytes_set(&module_name, &nn->name);

        hfuncs = nn->hfuncs->elts;

        for (i = 0; i < nn->hfuncs->nelts; i++) {
            hfunc = hfuncs[i];

            ngx_wasm_bytes_set(&hfunc_name, &hfunc->name);

            ngx_log_debug2(NGX_LOG_DEBUG_WASM, engine->log, 0,
                           "wasmtime defining \"%V.%V\" host function",
                           &nn->name, &hfunc->name);

            for (nargs = 0; hfunc->args[nargs] != 0; nargs++);
            for (nrets = 0; hfunc->rets[nrets] != 0; nrets++);

            wasm_valtype_vec_new_uninitialized(&args, nargs);
            wasm_valtype_vec_new_uninitialized(&rets, nrets);

            for (nargs = 0; hfunc->args[nargs] != 0; nargs++) {
                valtype = wasm_valtype_new(ngx_wasmtime_val_htow(
                                               hfunc->args[nargs]));
                args.data[nargs] = wasm_valtype_copy(valtype);
                wasm_valtype_delete(valtype);
            }

            for (nrets = 0; hfunc->rets[nrets] != 0; nrets++) {
                valtype = wasm_valtype_new(ngx_wasmtime_val_htow(
                                               hfunc->rets[nrets]));
                rets.data[nrets] = wasm_valtype_copy(valtype);
                wasm_valtype_delete(valtype);
            }

            functype = wasm_functype_new(&args, &rets);

            func = wasmtime_func_new(engine->store, functype,
                                     (wasmtime_func_callback_t) hfunc->ptr);

            func_extern = wasm_func_as_extern(func);

            *error = wasmtime_linker_define(engine->linker, &module_name,
                                            &hfunc_name, func_extern);

            wasm_func_delete(func);
            wasm_functype_delete(functype);
            wasm_valtype_vec_delete(&args);
            wasm_valtype_vec_delete(&rets);

            if (*error) {
                goto failed;
            }
        }
    }

done:

    return engine;

failed:

    ngx_wasmtime_free_engine(engine);

    return NULL;
}


static void
ngx_wasmtime_free_module(ngx_wasm_vm_module_pt vm_module)
{
    ngx_wasmtime_module_t       *module = vm_module;

    if (module->module) {
        wasm_importtype_vec_delete(&module->imports);
        wasm_exporttype_vec_delete(&module->exports);
        wasm_module_delete(module->module);
    }

    ngx_pfree(module->engine->pool, module);
}


static ngx_wasm_vm_module_pt
ngx_wasmtime_new_module(ngx_wasm_vm_engine_pt vm_engine, ngx_str_t *name,
    ngx_str_t *bytes, ngx_uint_t flags, ngx_wasm_vm_error_pt *vm_error)
{
    ngx_wasmtime_engine_t       *engine = vm_engine;
    wasmtime_error_t           **error = (wasmtime_error_t **) vm_error;
    ngx_wasmtime_module_t       *module;
    wasm_byte_vec_t              wasm_bytes, wat_bytes;
    wasm_name_t                  module_name;

    if (flags & NGX_WASM_VM_MODULE_ISWAT) {
        ngx_wasm_bytes_set(&wat_bytes, bytes);

        *error = wasmtime_wat2wasm(&wat_bytes, &wasm_bytes);
        if (*error) {
            return NULL;
        }

    } else {
        ngx_wasm_bytes_set(&wasm_bytes, bytes);
    }

    module = ngx_palloc(engine->pool, sizeof(ngx_wasmtime_module_t));
    if (module == NULL) {
        return NULL;
    }

    module->engine = engine;
    module->name = name;

    *error = wasmtime_module_new(engine->store, &wasm_bytes, &module->module);
    if (*error) {
        goto failed;
    }

    ngx_wasm_bytes_set(&module_name, name);

    *error = wasmtime_linker_module(engine->linker, &module_name,
                                    (const wasm_module_t *) module->module);
    if (*error) {
        goto failed;
    }

    wasm_module_imports(module->module, &module->imports);
    wasm_module_exports(module->module, &module->exports);

    return module;

failed:

    ngx_wasmtime_free_module(module);

    return NULL;
}


void
ngx_wasmtime_free_instance(ngx_wasm_vm_instance_pt vm_instance)
{
    ngx_wasmtime_instance_t     *instance = vm_instance;

    if (instance->instance) {
        /* double free bug in wasmtime? */
        //wasm_extern_vec_delete(&instance->externs);
        wasm_instance_delete(instance->instance);
    }

    if (instance->memory) {
        wasm_memory_delete(instance->memory);
    }

    ngx_pfree(instance->engine->pool, instance);
}


ngx_wasm_vm_instance_pt
ngx_wasmtime_new_instance(ngx_wasm_vm_engine_pt vm_engine,
    ngx_wasm_vm_module_pt vm_module, ngx_wasm_vm_error_pt *vm_error,
    ngx_wasm_vm_trap_pt *vm_trap)
{
    ngx_wasmtime_engine_t       *engine = vm_engine;
    ngx_wasmtime_module_t       *module = vm_module;
    wasmtime_error_t           **error = (wasmtime_error_t **) vm_error;
    wasm_trap_t                **trap = (wasm_trap_t **) vm_trap;
    ngx_wasmtime_instance_t     *instance;
    size_t                       i;
    const wasm_name_t           *extern_name;
    const wasm_externtype_t     *extern_type;

    instance = ngx_palloc(engine->pool, sizeof(ngx_wasmtime_instance_t));
    if (instance == NULL) {
        return NULL;
    }

    instance->engine = engine;

    *error = wasmtime_linker_instantiate(engine->linker, module->module,
                                         &instance->instance, trap);
    if (*error || *trap) {
        goto failed;
    }

    wasm_instance_exports(instance->instance, &instance->externs);

    ngx_log_debug2(NGX_LOG_DEBUG_WASM, engine->log, 0,
                   "wasmtime new \"%V\" instance with %d externs",
                   module->name, instance->externs.size);

    for (i = 0; i < instance->externs.size; i++) {
        extern_type = wasm_extern_type(instance->externs.data[i]);
        extern_name = wasm_exporttype_name(module->exports.data[i]);

        ngx_log_debug5(NGX_LOG_DEBUG_WASM, engine->log, 0,
                       "wasmtime new \"%V\" instance extern %d/%d: \"%*s\"",
                       module->name, i + 1, instance->externs.size,
                       extern_name->size, extern_name->data);

        if (wasm_externtype_kind(extern_type) == WASM_EXTERN_MEMORY) {
            instance->memory = wasm_extern_as_memory(instance->externs.data[i]);
        }
    }

    return instance;

failed:

    ngx_wasmtime_free_instance(instance);

    return NULL;
}


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
    NULL,                                /* init module */
    {                                    /* VM actions */
        &module_name,
        ngx_wasmtime_new_engine,
        ngx_wasmtime_free_engine,
        ngx_wasmtime_new_module,
        ngx_wasmtime_free_module,
        ngx_wasmtime_new_instance,
        ngx_wasmtime_free_instance,
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


/* vi:set ft=c ts=4 sw=4 et fdm=marker: */
