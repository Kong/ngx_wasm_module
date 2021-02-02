#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_wavm.h>


static void ngx_wavm_module_free(ngx_wavm_module_t *module);
static ngx_int_t ngx_wavm_module_load(ngx_wavm_module_t *module);
static u_char *ngx_wavm_log_error_handler(ngx_log_t *log, u_char *buf,
    size_t len);


static ngx_inline ngx_uint_t
ngx_wavm_ready(ngx_wavm_t *vm)
{
    if (vm == NULL) {
        ngx_wavm_log_error(NGX_LOG_EMERG, ngx_cycle->log, NULL, NULL,
                           "null pointer to vm");
        ngx_wasm_assert(0);
        return 0;

    } else if (vm->engine == NULL) {
        ngx_wavm_log_error(NGX_LOG_EMERG, vm->log, NULL, NULL,
                           "vm not initialized");
        ngx_wasm_assert(0);
        return 0;
    }

    return 1;
}


ngx_wavm_t *
ngx_wavm_new(ngx_cycle_t *cycle, const ngx_str_t *name)
{
    //u_char      *err = (u_char *) "no memory";
    ngx_wavm_t  *vm;

    vm = ngx_pcalloc(cycle->pool, sizeof(ngx_wavm_t));
    if (vm == NULL) {
        goto error;
    }

    //ngx_log_debug2(NGX_LOG_DEBUG_WASM, ngx_cycle->log, 0,
    //               "wasm creating \"%V\" vm (vm: %p)",
    //               vm->name, vm);

    vm->name = name;
    vm->pool = cycle->pool;

    vm->log = ngx_pcalloc(vm->pool, sizeof(ngx_log_t));
    if (vm->log == NULL) {
        goto error;
    }

    vm->log->file = cycle->new_log.file;
    vm->log->next = cycle->new_log.next;
    vm->log->writer = cycle->new_log.writer;
    vm->log->wdata = cycle->new_log.wdata;
    vm->log->log_level = cycle->new_log.log_level;
    vm->log->handler = ngx_wavm_log_error_handler;
    vm->log->data = &vm->log_ctx;

    vm->log_ctx.vm = vm;
    vm->log_ctx.instance = NULL;
    vm->log_ctx.orig_log = &cycle->new_log;

    vm->modules_max = 0;

    ngx_rbtree_init(&vm->modules_tree, &vm->modules_sentinel,
                    ngx_str_rbtree_insert_value);

    vm->config = wasm_config_new();

    return vm;

error:

    //ngx_wasm_log_error(NGX_LOG_EMERG, ngx_cycle->log, 0,
    //                   "failed to create \"%V\" vm: %s", name, err);

    if (vm) {
        ngx_wavm_free(vm);
    }

    return NULL;
}


void
ngx_wavm_free(ngx_wavm_t *vm)
{
    ngx_rbtree_node_t  **root, **sentinel, *node;
    ngx_str_node_t      *sn;
    ngx_wavm_module_t   *module;

    //ngx_log_debug2(NGX_LOG_DEBUG_WASM, vm->pool->log, 0,
    //               "wasm free \"%V\" vm (vm: %p)",
    //               vm->name, vm);

    root = &vm->modules_tree.root;
    sentinel = &vm->modules_tree.sentinel;

    while (*root != *sentinel) {
        node = ngx_rbtree_min(*root, *sentinel);
        sn = ngx_wasm_sn_n2sn(node);
        module = ngx_wasm_sn_sn2data(sn, ngx_wavm_module_t, sn);

        ngx_rbtree_delete(&vm->modules_tree, node);

        ngx_wavm_module_free(module);
    }

    if (vm->store) {
        wasm_store_delete(vm->store);
    }

    if (vm->engine) {
        wasm_engine_delete(vm->engine);
    }

    if (vm->log) {
        ngx_pfree(vm->pool, vm->log);
    }

    ngx_pfree(vm->pool, vm);
}


ngx_wavm_module_t *
ngx_wavm_module_lookup(ngx_wavm_t *vm, ngx_str_t *name)
{
    ngx_str_node_t  *sn;

    sn = ngx_wasm_sn_rbtree_lookup(&vm->modules_tree, name);
    if (sn == NULL) {
        return NULL;
    }

    return ngx_wasm_sn_sn2data(sn, ngx_wavm_module_t, sn);
}


ngx_int_t
ngx_wavm_module_add(ngx_wavm_t *vm, ngx_str_t *name, ngx_str_t *path)
{
    u_char             *p, *err = (u_char *) "no memory";
    ngx_wavm_module_t  *module;

    module = ngx_wavm_module_lookup(vm, name);
    if (module) {
        return NGX_DECLINED;
    }

    ngx_log_debug4(NGX_LOG_DEBUG_WASM, vm->log, 0,
                   "wasm adding \"%V\" module in \"%V\" vm"
                   " (vm: %p, module: %p)",
                   name, vm->name, vm, module);

    /* module == NULL */

    module = ngx_pcalloc(vm->pool, sizeof(ngx_wavm_module_t));
    if (module == NULL) {
        goto error;
    }

    module->vm = vm;
    module->memory_idx = -1;
    module->name.len = name->len;
    module->name.data = ngx_pnalloc(vm->pool, module->name.len + 1);
    if (module->name.data == NULL) {
        goto error;
    }

    p = ngx_copy(module->name.data, name->data, module->name.len);
    *p = '\0';

    module->path.len = path->len;
    module->path.data = ngx_pnalloc(vm->pool, module->path.len + 1);
    if (module->path.data == NULL) {
        goto error;
    }

    p = ngx_copy(module->path.data, path->data, module->path.len);
    *p = '\0';

    if (ngx_strncmp(&module->path.data[module->path.len - 4], ".wat", 4) == 0) {
        module->state |= NGX_WAVM_MODULE_ISWAT;
    }

    ngx_wasm_sn_init(&module->sn, &module->name);
    ngx_wasm_sn_rbtree_insert(&vm->modules_tree, &module->sn);

    module->ctx_idx = vm->modules_max++;

    return NGX_OK;

error:

    ngx_wavm_log_error(NGX_LOG_EMERG, vm->log, NULL, NULL,
                       "failed to add \"%V\" module from \"%V\": %s",
                       name, path, err);

    if (module) {
        ngx_wavm_module_free(module);
    }

    return NGX_ERROR;
}


static void
ngx_wavm_module_free(ngx_wavm_module_t *module)
{
    ngx_rbtree_node_t    **root, **sentinel, *node;
    ngx_str_node_t        *sn;
    ngx_wavm_t            *vm = module->vm;
    ngx_wavm_func_t       *func;

    ngx_log_debug4(NGX_LOG_DEBUG_WASM, vm->log, 0,
                   "wasm free \"%V\" module in \"%V\" vm"
                   " (vm: %p, module: %p)",
                   &module->name, vm->name, vm, module);

    root = &module->funcs_tree.root;
    sentinel = &module->funcs_tree.sentinel;

    while (*root != *sentinel) {
        node = ngx_rbtree_min(*root, *sentinel);
        sn = ngx_wasm_sn_n2sn(node);
        func = ngx_wasm_sn_sn2data(sn, ngx_wavm_func_t, sn);

        ngx_rbtree_delete(&module->funcs_tree, node);

        ngx_pfree(vm->pool, func);
    }

    if (module->module) {
        wasm_module_delete(module->module);
        wasm_importtype_vec_delete(&module->imports);
        wasm_exporttype_vec_delete(&module->exports);
    }

    if (module->hfuncs) {
       ngx_pfree(vm->pool, module->hfuncs);
    }

    if (module->name.data) {
        ngx_pfree(vm->pool, module->name.data);
    }

    if (module->path.data) {
        ngx_pfree(vm->pool, module->path.data);
    }

    ngx_pfree(vm->pool, module);
}


static ngx_int_t
ngx_wavm_module_load(ngx_wavm_module_t *module)
{
    size_t              i;
    u_char             *err = (u_char *) "no memory";
    ngx_uint_t          rc;
    ngx_str_t           s;
    ngx_wavm_t         *vm;
    ngx_wavm_hfunc_t   *hfunc;
    ngx_wavm_func_t    *func;
    ngx_wrt_res_t      *res;
    wasm_byte_vec_t     file_bytes, wasm_bytes, *bytes = NULL;
    wasm_importtype_t  *importtype;
    wasm_exporttype_t  *exporttype;
    const wasm_name_t  *importmodule, *importname, *exportname;

    if (ngx_wavm_module_loaded(module)) {
        return NGX_OK;
    }

    vm = module->vm;

    if (!ngx_wavm_ready(vm)) {
        goto unreachable;
    }

    if (!module->path.len) {
        ngx_wavm_log_error(NGX_LOG_ALERT, vm->log, NULL, NULL,
                           "NYI: module loading only supported via path");
        goto unreachable;
    }

    /* load_from_path */

    ngx_wavm_log_error(NGX_LOG_INFO, vm->log, NULL, NULL,
                       "loading \"%V\" module from \"%V\"",
                       &module->name, &module->path);

    if (ngx_wasm_bytes_from_path(&file_bytes, module->path.data, vm->log)
        != NGX_OK)
    {
        goto failed;
    }

    if (ngx_wavm_module_is_wat(module)) {
        ngx_log_debug1(NGX_LOG_DEBUG_WASM, vm->log, 0,
                       "wasm compiling wat at \"%V\"", &module->path);

        if (ngx_wrt_wat2wasm(&file_bytes, &wasm_bytes, &res) != NGX_OK) {
            err = (u_char *) "";
            goto error;
        }

        wasm_byte_vec_delete(&file_bytes);

        bytes = &wasm_bytes;

    } else {
        bytes = &file_bytes;
    }

    /* load wasm */

    if (ngx_wrt_module_new(
#if (NGX_WASM_HAVE_WASMTIME)
            vm->engine,
#else
            vm->store,
#endif
            bytes, &module->module, &res)
        != NGX_OK)
    {
        err = (u_char *) "";
        goto error;
    }


    wasm_module_imports(module->module, &module->imports);
    wasm_module_exports(module->module, &module->exports);

    module->state |= NGX_WAVM_MODULE_LOADED;

    module->nhfuncs = 0;

    /* hfuncs imports mapping */

    if (module->imports.size) {

        if (vm->hfuncs == NULL) {
            err = (u_char *) "no host functions in vm";
            goto error;
        }

        module->hfuncs = ngx_palloc(vm->pool, module->imports.size
                                              * sizeof(ngx_wavm_hfunc_t *));
        if (module->hfuncs == NULL) {
           goto error;
        }

        for (i = 0; i < module->imports.size; i++) {
            importtype = ((wasm_importtype_t **) module->imports.data)[i];
            importmodule = wasm_importtype_module(importtype);
            importname = wasm_importtype_name(importtype);

            ngx_log_debug7(NGX_LOG_DEBUG_WASM, vm->log, 0,
                           "wasm loading \"%V\" module import \"%*s.\"%*s\" (%ui/%ui)",
                           &module->name,
                           importmodule->size, importmodule->data,
                           importname->size, importname->data,
                           i + 1, module->imports.size);

            if (ngx_strncmp(importmodule->data, "env", 3) != 0) {
                continue;
            }

            switch (wasm_externtype_kind(
                    wasm_importtype_type(importtype))) {

            case WASM_EXTERN_FUNC:
                s.len = importname->size;
                s.data = (u_char *) importname->data;

                hfunc = ngx_wavm_hfuncs_lookup(vm->hfuncs, &s, NULL);
                if (hfunc == NULL) {
                    ngx_wavm_log_error(NGX_LOG_ERR, vm->log, NULL, NULL,
                                       "failed importing \"env.\"%V\": "
                                       "missing host function",
                                       importname->size, importname->data);

                    err = (u_char *) "incompatible host interface";
                    goto error;
                }

                module->hfuncs[module->nhfuncs] = hfunc;
                module->nhfuncs++;
                break;

            default:
                ngx_wavm_log_error(NGX_LOG_ALERT, vm->log, NULL, NULL,
                                   "NYI: module import type not supported");
                ngx_wasm_assert(0);
                goto failed;

            }
        }
    }

    ngx_wasm_assert(module->nhfuncs == module->imports.size);

    /* build exports lookups */

    ngx_rbtree_init(&module->funcs_tree, &module->funcs_sentinel,
                    ngx_str_rbtree_insert_value);

    for (i = 0; i < module->exports.size; i++) {
        exporttype = ((wasm_exporttype_t **) module->exports.data)[i];
        exportname = wasm_exporttype_name(exporttype);


        ngx_log_debug(NGX_LOG_DEBUG_WASM, vm->log, 0,
                      "wasm loading \"%V.%*s\" export",
                      &module->name, exportname->size, exportname->data);

        switch (wasm_externtype_kind(
                wasm_exporttype_type(exporttype))) {

        case WASM_EXTERN_FUNC:
            func = ngx_palloc(vm->pool, sizeof(ngx_wavm_func_t));
            if (func == NULL) {
                goto error;
            }

            func->module = module;
            func->name.len = exportname->size;
            func->name.data = (u_char *) exportname->data;
            func->exports_idx = i;

            ngx_wasm_sn_init(&func->sn, &func->name);
            ngx_wasm_sn_rbtree_insert(&module->funcs_tree, &func->sn);
            break;

        case WASM_EXTERN_MEMORY:
            module->memory_idx = i;
            break;

        default:
            break;

        }
    }

    rc = NGX_OK;
    goto done;

error:

    ngx_wavm_log_error(NGX_LOG_EMERG, vm->log, res, NULL,
                       "failed to load \"%V\" module: %s",
                       &module->name, err);

failed:

    if (ngx_wavm_module_loaded(module)) {
        ngx_wavm_module_free(module);
    }

    rc = NGX_ERROR;

done:

    if (bytes) {
        wasm_byte_vec_delete(bytes);
    }

    return rc;

unreachable:

    ngx_wasm_assert(0);
    rc = NGX_ABORT;
    goto done;
}


ngx_int_t
ngx_wavm_init(ngx_wavm_t *vm, ngx_wavm_hfuncs_t *hfuncs)
{
    u_char             *err = NULL;
    ngx_rbtree_node_t  *root, *sentinel, *node;
    ngx_str_node_t     *sn;
    ngx_wavm_module_t  *module;

    if (vm->engine) {
        err = (u_char *) "already initialized";
        goto error;
    }

    ngx_wavm_log_error(NGX_LOG_INFO, vm->log, NULL, NULL,
                       "initializing \"%V\" wasm VM", vm->name);

    ngx_wrt_config_init(vm->config);

    vm->hfuncs = hfuncs;
    vm->engine = wasm_engine_new_with_config(vm->config);
    if (vm->engine == NULL) {
        err = (u_char *) "engine init failure";
        goto error;
    }

    vm->store = wasm_store_new(vm->engine);
    if (vm->store == NULL) {
        err = (u_char *) "store init failure";
        goto error;
    }

    root = vm->modules_tree.root;
    sentinel = vm->modules_tree.sentinel;

    if (root == sentinel) {
        goto done;
    }

    for (node = ngx_rbtree_min(root, sentinel);
         node;
         node = ngx_rbtree_next(&vm->modules_tree, node))
    {
        sn = ngx_wasm_sn_n2sn(node);
        module = ngx_wasm_sn_sn2data(sn, ngx_wavm_module_t, sn);

        if (ngx_wavm_module_load(module) != NGX_OK) {
            /* TODO */
            goto failed;
        }
    }

done:

    ngx_wavm_log_error(NGX_LOG_INFO, vm->log, NULL, NULL,
                       "\"%V\" wasm VM initialized (modules: %ui)",
                       vm->name, vm->modules_max);

    return NGX_OK;

error:

    ngx_wavm_log_error(NGX_LOG_EMERG, vm->log, NULL, NULL,
                       "failed to initialize wasm VM: %s", err);

failed:

    return NGX_ERROR;
}


ngx_wavm_func_t *
ngx_wavm_module_function_lookup(ngx_wavm_module_t *module, ngx_str_t *name)
{
    ngx_str_node_t  *sn;

    sn = ngx_wasm_sn_rbtree_lookup(&module->funcs_tree, name);
    if (sn == NULL) {
        return NULL;
    }

    return ngx_wasm_sn_sn2data(sn, ngx_wavm_func_t, sn);
}


ngx_int_t
ngx_wavm_ctx_init(ngx_wavm_t *vm, ngx_wavm_ctx_t *ctx)
{
    ctx->vm = vm;

    ctx->store = wasm_store_new(vm->engine);
    if (ctx->store == NULL) {
        return NGX_ERROR;
    }

    ctx->instances = ngx_pcalloc(ctx->pool, vm->modules_max *
                                            sizeof(ngx_wavm_instance_t *));
    if (ctx->instances == NULL) {
        return NGX_ERROR;
    }

    return NGX_OK;
}


void
ngx_wavm_ctx_destroy(ngx_wavm_ctx_t *ctx)
{
    size_t                i;
    ngx_wavm_t           *vm = ctx->vm;
    ngx_wavm_instance_t  *instance;

    if (ctx->instances) {
        for (i = 0; i < vm->modules_max; i++) {
            instance = ctx->instances[i];
            if (instance) {
                ngx_wavm_instance_free(instance);
            }
        }

        ngx_pfree(ctx->pool, ctx->instances);
    }

    if (ctx->store) {
        wasm_store_delete(ctx->store);
    }
}


ngx_wavm_instance_t *
ngx_wavm_module_instantiate(ngx_wavm_module_t *module, ngx_wavm_ctx_t *ctx,
    ngx_module_t *m)
{
    size_t                  i;
    u_char                 *err = (u_char *) "no memory";
    ngx_wavm_t             *vm;
    ngx_wavm_instance_t    *instance;
    ngx_wavm_hfunc_t       *hfunc, *hf;
    ngx_wavm_hfunc_tctx_t  *tctx;
    wasm_func_t            *func;
    wasm_extern_t          *export;
    wasm_trap_t            *trap = NULL;
    ngx_wrt_res_t          *res;

    vm = module->vm;

    instance = ctx->instances[module->ctx_idx];
    if (instance) {
        ngx_log_debug3(NGX_LOG_DEBUG_WASM, vm->log, 0,
                       "wasm reusing instance of \"%V\" module "
                       "in \"%V\" vm (ctx: %p)",
                       &module->name, vm->name, ctx);
        return instance;
    }

    ngx_log_debug3(NGX_LOG_DEBUG_WASM, vm->log, 0,
                   "wasm creating instance of \"%V\" module in \"%V\" vm "
                   "(ctx: %p)", &module->name, vm->name, ctx);

    instance = ngx_palloc(vm->pool, sizeof(ngx_wavm_instance_t));
    if (instance == NULL) {
        goto error;
    }

    instance->pool = vm->pool;
    instance->module = module;
    instance->tctxs = NULL;

    instance->log = ngx_pcalloc(instance->pool, sizeof(ngx_log_t));
    if (instance->log == NULL) {
        goto error;
    }

    instance->log->file = vm->log->file;
    instance->log->next = vm->log->next;
    instance->log->wdata = vm->log->wdata;
    instance->log->writer = vm->log->writer;
    instance->log->log_level = vm->log->log_level;
    instance->log->handler = ngx_wavm_log_error_handler;
    instance->log->data = &instance->log_ctx;

    instance->log_ctx.vm = vm;
    instance->log_ctx.instance = instance;
    instance->log_ctx.orig_log = NULL;

    /* link hfuncs */

    wasm_extern_vec_new_uninitialized(&instance->env, module->nhfuncs);

    if (module->nhfuncs) {
        instance->tctxs = ngx_palloc(instance->pool,
                                     module->nhfuncs *
                                     sizeof(ngx_wavm_hfunc_tctx_t));
        if (instance->tctxs == NULL) {
            goto error;
        }

        for (i = 0; i < module->nhfuncs; i++) {
            hfunc = ((ngx_wavm_hfunc_t **) module->hfuncs)[i];
            hf = ngx_wavm_hfuncs_lookup(vm->hfuncs, &hfunc->def->name, m);

            if (hf != hfunc) {
                ngx_wavm_log_error(NGX_LOG_EMERG, vm->log, NULL, NULL,
                                   "missing \"%V\" host function in \"%s\""
                                   " ngx_wasm_module",
                                   &hfunc->def->name, m->name);
                goto error;
            }

            tctx = &instance->tctxs[i];

            tctx->hfunc = hfunc;
            tctx->instance = instance;

            func = wasm_func_new_with_env(ctx->store, hfunc->functype,
                                          &ngx_wavm_hfuncs_trampoline,
                                          tctx, NULL);

            instance->env.data[i] = wasm_func_as_extern(func);
        }
    }

    /* instantiate */

    ngx_wasm_assert(instance->env.size == module->nhfuncs);

    if (ngx_wrt_instance_new(ctx->store, module->module, &instance->env,
                             &instance->instance, &trap, &res) != NGX_OK)
    {
        goto error;
    }

    /* get exports */

    wasm_instance_exports(instance->instance, &instance->exports);

    if (module->memory_idx >= 0) {
        export = ((wasm_extern_t **) instance->exports.data)[module->memory_idx];
        ngx_wasm_assert(wasm_extern_kind(export) == WASM_EXTERN_MEMORY);
        instance->memory = wasm_extern_as_memory(export);
    }

    if (ctx) {
        instance->ctx = ctx;
        instance->log_ctx.orig_log = ctx->log;

        ctx->instances[module->ctx_idx] = instance;
    }

    return instance;

error:

    ngx_wavm_log_error(NGX_LOG_ERR, vm->log, NULL, trap,
                       "failed to instantiate \"%V\" module: %s",
                       &module->name, err);

    if (instance) {
        ngx_wavm_instance_free(instance);
    }

    return NULL;
}


ngx_int_t
ngx_wavm_function_call(ngx_wavm_func_t *func,
    ngx_wavm_instance_t *instance)
{
    wasm_extern_t  *export;
    wasm_trap_t    *trap;
    ngx_wrt_res_t  *res = NULL;

    ngx_wasm_assert(func->module == instance->module);

    export = ((wasm_extern_t **) instance->exports.data)[func->exports_idx];

    ngx_wasm_assert(wasm_extern_kind(export) == WASM_EXTERN_FUNC);

    if (ngx_wrt_func_call(wasm_extern_as_func(export), NULL, NULL, &trap, &res)
         != NGX_OK)
    {
        ngx_wavm_log_error(NGX_LOG_ERR, instance->log, res, trap, NULL);
        return NGX_ERROR;
    }

    return NGX_OK;
}


void
ngx_wavm_instance_free(ngx_wavm_instance_t *instance)
{
    ngx_log_debug5(NGX_LOG_DEBUG_WASM, instance->pool->log, 0,
                   "wasm free instance of \"%V\" module in \"%V\" vm"
                   " (vm: %p, module: %p, instance: %p)",
                   &instance->module->name, instance->module->vm->name,
                   instance->module->vm, instance->module, instance);

    if (instance->instance) {
        wasm_instance_delete(instance->instance);
        wasm_extern_vec_delete(&instance->exports);
        wasm_extern_vec_delete(&instance->env);
    }

    if (instance->tctxs) {
       ngx_pfree(instance->pool, instance->tctxs);
    }

    if (instance->log) {
        ngx_pfree(instance->pool, instance->log);
    }

    ngx_pfree(instance->pool, instance);
}


void
ngx_wavm_log_error(ngx_uint_t level, ngx_log_t *log,
    ngx_wrt_res_t *res, wasm_trap_t *trap,
#if (NGX_HAVE_VARIADIC_MACROS)
    const char *fmt, ...)

#else
    const char *fmt, va_list args)
#endif
{
#if (NGX_HAVE_VARIADIC_MACROS)
    va_list              args;
#endif
    u_char              *p, *last, errstr[NGX_MAX_ERROR_STR];
    wasm_message_t       trapmsg;

    last = errstr + NGX_MAX_ERROR_STR;
    p = &errstr[0];

    if (fmt) {
#if (NGX_HAVE_VARIADIC_MACROS)
        va_start(args, fmt);
        p = ngx_vslprintf(p, last, fmt, args);
        va_end(args);

#else
        p = ngx_vslprintf(p, last, fmt, args);
#endif
    }

    if (trap) {
        wasm_trap_message(trap, &trapmsg);

        p = ngx_slprintf(p, last, "%*s", trapmsg.size, trapmsg.data);

        wasm_byte_vec_delete(&trapmsg);
        wasm_trap_delete(trap);
    }

    p = ngx_wrt_error_log_handler(res, p, last - p);

    ngx_wasm_log_error(level, log, 0, "%*s", p - errstr, errstr);
}


static u_char *
ngx_wavm_log_error_handler(ngx_log_t *log, u_char *buf, size_t len)
{
    u_char               *p;
    ngx_log_t            *orig_log;
    ngx_wavm_t           *vm;
    ngx_wavm_instance_t  *instance;
    ngx_wavm_log_ctx_t   *ctx;

    ctx = log->data;

    vm = ctx->vm;
    instance = ctx->instance;
    orig_log = ctx->orig_log;

    if (instance) {
        p = ngx_snprintf(buf, len, " <module: \"%V\", vm: \"%V\", runtime: \"%s\">",
                         &instance->module->name, vm->name, NGX_WASM_RUNTIME);

    } else {
        p = ngx_snprintf(buf, len, " <vm: \"%V\", runtime: \"%s\">",
                         vm->name, NGX_WASM_RUNTIME);
    }

    len -= p - buf;
    buf = p;

    if (orig_log->handler) {
        p = orig_log->handler(orig_log, buf, len);
    }

    return p;
}
