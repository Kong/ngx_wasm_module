#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_wavm.h>


static ngx_int_t ngx_wavm_engine_init(ngx_wavm_t *vm, ngx_wavm_err_t *e);
static void ngx_wavm_engine_destroy(ngx_wavm_t *vm);
static void ngx_wavm_module_free(ngx_wavm_module_t *module);
static ngx_int_t ngx_wavm_module_load_bytes(ngx_wavm_module_t *module);
static ngx_int_t ngx_wavm_module_load(ngx_wavm_module_t *module);
static void ngx_wavm_linked_module_free(ngx_wavm_linked_module_t *lmodule);
static ngx_int_t ngx_wavm_func_call(ngx_wavm_func_t *f, wasm_val_vec_t *args,
    wasm_val_vec_t *rets, ngx_wavm_err_t *e);
static void ngx_wavm_val_vec_set(wasm_val_vec_t *out,
    const wasm_valtype_vec_t *valtypes, va_list args);
static ngx_int_t ngx_wavm_instance_call_func_va(ngx_wavm_instance_t *instance,
    ngx_wavm_func_t *f, wasm_val_vec_t **rets, va_list args);
static void ngx_wavm_log_error(ngx_uint_t level, ngx_log_t *log,
    ngx_wavm_err_t *e, const char *fmt, ...);
static u_char *ngx_wavm_log_error_handler(ngx_log_t *log, u_char *buf,
    size_t len);


static const char  NGX_WAVM_NOMEM_CHAR[] = "no memory";
static const char  NGX_WAVM_EMPTY_CHAR[] = "";


ngx_wavm_t *
ngx_wavm_create(ngx_cycle_t *cycle, const ngx_str_t *name,
    ngx_wavm_host_def_t *core_host)
{
    ngx_wavm_t  *vm;

    vm = ngx_pcalloc(cycle->pool, sizeof(ngx_wavm_t));
    if (vm == NULL) {
        goto error;
    }

    vm->name = name;
    vm->pool = cycle->pool;
    vm->core_host = core_host;
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

    vm->state = 0;
    vm->lmodules_max = 0;

    ngx_rbtree_init(&vm->modules_tree, &vm->modules_sentinel,
                    ngx_str_rbtree_insert_value);

    ngx_queue_init(&vm->instances);

    return vm;

error:

    ngx_wasm_log_error(NGX_LOG_EMERG, ngx_cycle->log, 0,
                      "failed creating \"%V\" vm: %s",
                      name, NGX_WAVM_NOMEM_CHAR);

    if (vm) {
        ngx_wavm_destroy(vm);
    }

    return NULL;
}


static ngx_int_t
ngx_wavm_engine_init(ngx_wavm_t *vm, ngx_wavm_err_t *e)
{
    wasm_config_t  *config;

    if (vm->engine || vm->store) {
        /* reentrant */
        return NGX_ABORT;
    }

    config = wasm_config_new();

    ngx_wrt_config_init(config);

    if (ngx_wrt_engine_new(config, &vm->engine, e) != NGX_OK) {
        goto error;
    }

    vm->store = wasm_store_new(vm->engine);
    if (vm->store == NULL) {
        goto error;
    }

    return NGX_OK;

error:

    ngx_wavm_engine_destroy(vm);

    return NGX_ERROR;
}


static void
ngx_wavm_engine_destroy(ngx_wavm_t *vm)
{
    if (vm->store) {
        wasm_store_delete(vm->store);
        vm->store = NULL;
    }

    if (vm->engine) {
        wasm_engine_delete(vm->engine);
        vm->engine = NULL;
    }
}


ngx_int_t
ngx_wavm_init(ngx_wavm_t *vm)
{
    ngx_uint_t          rc = NGX_ERROR;
    ngx_str_node_t     *sn;
    ngx_rbtree_node_t  *root, *sentinel, *node;
    ngx_wavm_module_t  *module;
    ngx_wavm_err_t      e;

    ngx_wavm_err_init(&e);

    ngx_wavm_log_error(NGX_LOG_INFO, vm->log, NULL,
                       "initializing \"%V\" wasm VM", vm->name);

    if (ngx_wavm_engine_init(vm, &e) != NGX_OK) {
        goto error;
    }

    root = vm->modules_tree.root;
    sentinel = vm->modules_tree.sentinel;

    if (root == sentinel) {
        goto empty;
    }

    for (node = ngx_rbtree_min(root, sentinel);
         node;
         node = ngx_rbtree_next(&vm->modules_tree, node))
    {
        sn = ngx_wasm_sn_n2sn(node);
        module = ngx_wasm_sn_sn2data(sn, ngx_wavm_module_t, sn);

        if (ngx_wavm_module_load_bytes(module) != NGX_OK) {
            goto done;
        }
    }

empty:

    ngx_wavm_log_error(NGX_LOG_INFO, vm->log, NULL,
                       "\"%V\" wasm VM initialized",
                       vm->name);

    rc = NGX_OK;
    goto done;

error:

    ngx_wavm_log_error(NGX_LOG_EMERG, vm->log, &e,
                       "failed initializing wasm VM: ");

done:

    ngx_wavm_engine_destroy(vm);

    return rc;
}


ngx_int_t
ngx_wavm_load(ngx_wavm_t *vm)
{
    ngx_str_node_t     *sn;
    ngx_rbtree_node_t  *root, *sentinel, *node;
    ngx_wavm_module_t  *module;
    ngx_wavm_err_t      e;

    ngx_wavm_err_init(&e);

    if (ngx_wavm_engine_init(vm, &e) != NGX_OK) {
        ngx_wavm_log_error(NGX_LOG_EMERG, vm->log, &e,
                           "failed initializing engine: ");
        return NGX_ERROR;
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
            return NGX_ERROR;
        }
    }

done:

    vm->state |= NGX_WAVM_READY;

    return NGX_OK;
}


static void
ngx_wavm_destroy_instances(ngx_wavm_t *vm)
{
    ngx_queue_t          *q;
    ngx_wavm_instance_t  *instance;

    while (!ngx_queue_empty(&vm->instances)) {
        q = ngx_queue_head(&vm->instances);
        instance = ngx_queue_data(q, ngx_wavm_instance_t, q);

        ngx_wavm_instance_destroy(instance);
    }
}


void
ngx_wavm_destroy(ngx_wavm_t *vm)
{
    ngx_rbtree_node_t    **root, **sentinel, *node;
    ngx_str_node_t        *sn;
    ngx_wavm_module_t     *module;

#if 1
    ngx_log_debug2(NGX_LOG_DEBUG_WASM, vm->pool->log, 0,
                  "wasm free \"%V\" vm (vm: %p)",
                   vm->name, vm);
#endif

    ngx_wavm_destroy_instances(vm);

    root = &vm->modules_tree.root;
    sentinel = &vm->modules_tree.sentinel;

    while (*root != *sentinel) {
        node = ngx_rbtree_min(*root, *sentinel);
        sn = ngx_wasm_sn_n2sn(node);
        module = ngx_wasm_sn_sn2data(sn, ngx_wavm_module_t, sn);

        ngx_rbtree_delete(&vm->modules_tree, node);

        ngx_wavm_module_free(module);
    }

    ngx_wavm_engine_destroy(vm);

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
    u_char             *p;
    const char         *err = NGX_WAVM_NOMEM_CHAR;
    ngx_wavm_module_t  *module;

    if (ngx_wavm_state(vm, NGX_WAVM_READY)) {
        /* frozen */
        return NGX_ABORT;
    }

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
    module->state = 0;
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

    if (ngx_strncmp(&module->path.data[module->path.len - 4],
                    ".wat", 4) == 0)
    {
        module->state |= NGX_WAVM_MODULE_ISWAT;
    }

    ngx_wasm_sn_init(&module->sn, &module->name);
    ngx_wasm_sn_rbtree_insert(&vm->modules_tree, &module->sn);

    ngx_queue_init(&module->lmodules);

    return NGX_OK;

error:

    ngx_wavm_log_error(NGX_LOG_EMERG, vm->log, NULL,
                       "failed adding \"%V\" module from \"%V\": %s",
                       name, path, err);

    if (module) {
        ngx_wavm_module_free(module);
    }

    return NGX_ERROR;
}


static ngx_int_t
ngx_wavm_module_load_bytes(ngx_wavm_module_t *module)
{
    const char       *err = NGX_WAVM_NOMEM_CHAR;
    ngx_uint_t        rc;
    ngx_wavm_t       *vm;
    ngx_wavm_err_t    e;
    wasm_byte_vec_t   file_bytes;

    vm = module->vm;

    ngx_log_debug4(NGX_LOG_DEBUG_WASM, vm->log, 0,
                   "wasm loading \"%V\" module bytes from \"%V\""
                   " (module: %p, engine: %p)",
                   &module->name, &module->path,
                   module->module, module->vm->engine);

    if (ngx_wasm_bytes_from_path(&file_bytes, module->path.data, vm->log)
        != NGX_OK)
    {
        goto failed;
    }

    ngx_wavm_err_init(&e);

    if (ngx_wavm_state(module, NGX_WAVM_MODULE_ISWAT)) {
        ngx_log_debug1(NGX_LOG_DEBUG_WASM, vm->log, 0,
                       "wasm compiling wat at \"%V\"", &module->path);

        rc = ngx_wrt_wat2wasm(&file_bytes, &module->bytes, &e);

        wasm_byte_vec_delete(&file_bytes);

        if (rc != NGX_OK) {
            err = NGX_WAVM_EMPTY_CHAR;
            goto error;
        }

    } else {
        module->bytes.size = file_bytes.size;
        module->bytes.data = file_bytes.data;
    }

    rc = ngx_wrt_module_validate(vm->store, &module->bytes, &e);
    if (rc != NGX_OK) {
        err = NGX_WAVM_EMPTY_CHAR;
        goto error;
    }

    module->state |= NGX_WAVM_MODULE_LOADED_BYTES;

    rc = NGX_OK;
    goto done;

error:

    ngx_wavm_log_error(NGX_LOG_EMERG, vm->log, &e,
                       "failed loading \"%V\" module bytes: %s",
                       &module->name, err);

failed:

    rc = NGX_ERROR;

done:

    return rc;
}


static ngx_int_t
ngx_wavm_module_load(ngx_wavm_module_t *module)
{
    size_t                    i;
    u_char                   *p;
    const char               *err = NGX_WAVM_NOMEM_CHAR;
    ngx_uint_t                rc;
    ngx_wavm_t               *vm;
    ngx_wavm_funcref_t       *funcref;
    ngx_wavm_err_t            e;
    wasm_exporttype_t        *exporttype;
    const wasm_externtype_t  *externtype;
    const wasm_importtype_t  *importtype;
    const wasm_name_t        *exportname, *importmodule;

    vm = module->vm;

    if (ngx_wavm_state(module, NGX_WAVM_MODULE_LOADED)) {
        return NGX_OK;
    }

    if (!ngx_wavm_state(module, NGX_WAVM_MODULE_LOADED_BYTES)) {
        goto unreachable;
    }

    ngx_wavm_log_error(NGX_LOG_INFO, vm->log, NULL,
                       "loading \"%V\" module", &module->name);

    ngx_wavm_err_init(&e);

    if (ngx_wrt_module_new(
#if (NGX_WASM_HAVE_WASMTIME)
            vm->engine,
#else
            vm->store,
#endif
            &module->bytes, &module->module, &e)
        != NGX_OK)
    {
        err = NGX_WAVM_EMPTY_CHAR;
        goto error;
    }

    wasm_module_imports(module->module, &module->imports);
    wasm_module_exports(module->module, &module->exports);

#if 1
    for (i = 0; i < module->imports.size; i++) {
        importtype = ((wasm_importtype_t **) module->imports.data)[i];
        importmodule = wasm_importtype_module(importtype);

        if (ngx_strncmp(importmodule->data, "env", 3) != 0) {
            continue;
        }

        if (wasm_externtype_kind(wasm_importtype_type(importtype))
            != WASM_EXTERN_FUNC)
        {
            ngx_wavm_log_error(NGX_LOG_WASM_NYI, vm->log, NULL,
                               "NYI: module import type not supported");
            goto failed;
        }
    }
#endif

    /* build exports lookups */

    ngx_rbtree_init(&module->funcs_tree, &module->funcs_sentinel,
                    ngx_str_rbtree_insert_value);

    for (i = 0; i < module->exports.size; i++) {
        exporttype = ((wasm_exporttype_t **) module->exports.data)[i];
        exportname = wasm_exporttype_name(exporttype);
        externtype = wasm_exporttype_type(exporttype);

        dd("wasm caching \"%.*s\" module export \"%.*s\" (%lu/%lu)",
           (int) module->name.len, module->name.data, (int) exportname->size,
           exportname->data, i + 1, module->exports.size);

        switch (wasm_externtype_kind(externtype)) {

        case WASM_EXTERN_FUNC:
            funcref = ngx_palloc(vm->pool, sizeof(ngx_wavm_funcref_t));
            if (funcref == NULL) {
                goto error;
            }

            funcref->exports_idx = i;
            funcref->module = module;
            funcref->name.len = exportname->size;
            funcref->name.data = ngx_pnalloc(vm->pool, funcref->name.len + 1);
            if (funcref->name.data == NULL) {
                goto error;
            }

            p = ngx_copy(funcref->name.data, exportname->data,
                         funcref->name.len);
            *p = '\0';

            ngx_wasm_sn_init(&funcref->sn, &funcref->name);
            ngx_wasm_sn_rbtree_insert(&module->funcs_tree, &funcref->sn);

            if (ngx_strncmp(exportname->data, "_start", 6) == 0) {
                module->f_start = funcref;
            }

            break;

        default:
            break;

        }
    }

    module->state |= NGX_WAVM_MODULE_LOADED;

    rc = NGX_OK;
    goto done;

error:

    ngx_wavm_log_error(NGX_LOG_EMERG, vm->log, &e,
                       "failed loading \"%V\" module: %s",
                       &module->name, err);

failed:

    module->state |= NGX_WAVM_MODULE_INVALID;

    rc = NGX_ERROR;

done:

    wasm_byte_vec_delete(&module->bytes);
    module->bytes.size = 0;

    return rc;

unreachable:

    ngx_wasm_assert(0);
    rc = NGX_ABORT;
    goto done;
}


ngx_wavm_linked_module_t *
ngx_wavm_module_link(ngx_wavm_module_t *module, ngx_wavm_host_def_t *host)
{
    size_t                     i;
    const char                *err = NGX_WAVM_NOMEM_CHAR;
    ngx_str_t                  s;
    ngx_queue_t               *q;
    ngx_wavm_t                *vm = NULL;
    ngx_wavm_hfunc_t          *hfunc, **hfuncp;
    ngx_wavm_linked_module_t  *lmodule = NULL;
    const wasm_importtype_t   *importtype;
    const wasm_name_t         *importmodule, *importname;

    vm = module->vm;

    if (ngx_wavm_state(module, NGX_WAVM_MODULE_INVALID)) {
        err = "invalid module";
        goto error;

    } else if (!ngx_wavm_state(module, NGX_WAVM_MODULE_LOADED)) {
        goto unreachable;
    }

    for (q = ngx_queue_head(&module->lmodules);
         q != ngx_queue_sentinel(&module->lmodules);
         q = ngx_queue_next(q))
    {
        lmodule = ngx_queue_data(q, ngx_wavm_linked_module_t, q);

        if (lmodule->host_def == host) {
            return lmodule;
        }
    }

    ngx_log_debug5(NGX_LOG_DEBUG_WASM, vm->log, 0,
                   "wasm linking \"%V\" module to \"%V\" host interface"
                   " (vm: %p, module: %p, host: %p)",
                   &module->name, &host->name, vm, module, host);

    lmodule = ngx_pcalloc(vm->pool, sizeof(ngx_wavm_linked_module_t));
    if (lmodule == NULL) {
        goto error;
    }

    lmodule->host_def = host;
    lmodule->module = module;
    lmodule->hfuncs_imports = ngx_array_create(vm->pool, 2,
                                               sizeof(ngx_wavm_hfunc_t *));
    if (lmodule->hfuncs_imports == NULL) {
        goto error;
    }

    if (host == NULL) {
        goto done;
    }

    for (i = 0; i < module->imports.size; i++) {
        importtype = ((wasm_importtype_t **) module->imports.data)[i];
        importmodule = wasm_importtype_module(importtype);
        importname = wasm_importtype_name(importtype);

        if (ngx_strncmp(importmodule->data, "env", 3) != 0) {
            continue;
        }

        dd("wasm loading \"%.*s\" module import \"env.%.*s\" (%lu/%lu)",
           (int) module->name.len, module->name.data,
           (int) importname->size, importname->data,
           i + 1, module->imports.size);

        ngx_wasm_assert(wasm_externtype_kind(wasm_importtype_type(importtype))
                        == WASM_EXTERN_FUNC);

        s.len = importname->size;
        s.data = (u_char *) importname->data;

        hfunc = ngx_wavm_host_hfunc_create(vm->pool, host, &s);
        if (hfunc == NULL && vm->core_host) {
            hfunc = ngx_wavm_host_hfunc_create(vm->pool, vm->core_host, &s);
        }

        if (hfunc == NULL) {
            ngx_wavm_log_error(NGX_LOG_ERR, vm->log, NULL,
                               "failed importing \"env.%*s\": "
                               "missing host function",
                               importname->size, importname->data);

            err = "incompatible host interface";
            goto error;
        }

        hfuncp = ngx_array_push(lmodule->hfuncs_imports);
        if (hfuncp == NULL) {
            goto error;
        }

        *hfuncp = hfunc;
    }

done:

    lmodule->idx = vm->lmodules_max++;

    ngx_queue_insert_tail(&module->lmodules, &lmodule->q);

    return lmodule;

error:

    ngx_wavm_log_error(NGX_LOG_EMERG, vm->log, NULL,
                       "failed linking \"%V\" module with \"%V\" "
                       "host interface: %s",
                       &module->name, &host->name, err);

    if (lmodule) {
        ngx_wavm_linked_module_free(lmodule);
    }

    return NULL;

unreachable:

    ngx_wasm_assert(0);
    return NULL;
}


static void
ngx_wavm_module_free(ngx_wavm_module_t *module)
{
    ngx_queue_t                *q;
    ngx_str_node_t             *sn;
    ngx_rbtree_node_t         **root, **sentinel, *node;
    ngx_wavm_t                 *vm = module->vm;
    ngx_wavm_linked_module_t   *lmodule;
    ngx_wavm_funcref_t         *funcref;

    ngx_log_debug4(NGX_LOG_DEBUG_WASM, vm->log, 0,
                   "wasm free \"%V\" module in \"%V\" vm"
                   " (vm: %p, module: %p)",
                   &module->name, vm->name, vm, module);

    if (module->module) {
        wasm_importtype_vec_delete(&module->imports);
        wasm_exporttype_vec_delete(&module->exports);
        wasm_module_delete(module->module);
    }

    root = &module->funcs_tree.root;
    sentinel = &module->funcs_tree.sentinel;

    while (*root != *sentinel) {
        node = ngx_rbtree_min(*root, *sentinel);
        sn = ngx_wasm_sn_n2sn(node);
        funcref = ngx_wasm_sn_sn2data(sn, ngx_wavm_funcref_t, sn);

        ngx_rbtree_delete(&module->funcs_tree, node);

        ngx_pfree(vm->pool, funcref);
    }

    if (module->bytes.size) {
        wasm_byte_vec_delete(&module->bytes);
    }

    while (!ngx_queue_empty(&module->lmodules)) {
        q = ngx_queue_head(&module->lmodules);
        lmodule = ngx_queue_data(q, ngx_wavm_linked_module_t, q);
        ngx_queue_remove(&lmodule->q);

        ngx_wavm_linked_module_free(lmodule);
    }

    if (module->name.data) {
        ngx_pfree(vm->pool, module->name.data);
    }

    if (module->path.data) {
        ngx_pfree(vm->pool, module->path.data);
    }

    ngx_pfree(vm->pool, module);
}


static void
ngx_wavm_linked_module_free(ngx_wavm_linked_module_t *lmodule)
{
    size_t             i;
    ngx_wavm_t        *vm = lmodule->module->vm;
    ngx_wavm_hfunc_t  *hfunc;

    if (lmodule->hfuncs_imports) {
        for (i = 0; i < lmodule->hfuncs_imports->nelts; i++) {
            hfunc = ((ngx_wavm_hfunc_t **) lmodule->hfuncs_imports->elts)[i];

            ngx_wavm_host_hfunc_destroy(hfunc);
        }

        ngx_array_destroy(lmodule->hfuncs_imports);
    }

    ngx_pfree(vm->pool, lmodule);
}


ngx_wavm_funcref_t *
ngx_wavm_module_func_lookup(ngx_wavm_module_t *module, ngx_str_t *name)
{
    ngx_str_node_t  *sn;

    if (!ngx_wavm_state(module, NGX_WAVM_MODULE_LOADED)) {
        return NULL;
    }

    sn = ngx_wasm_sn_rbtree_lookup(&module->funcs_tree, name);
    if (sn == NULL) {
        return NULL;
    }

    return ngx_wasm_sn_sn2data(sn, ngx_wavm_funcref_t, sn);
}


ngx_int_t
ngx_wavm_ctx_init(ngx_wavm_t *vm, ngx_wavm_ctx_t *ctx)
{
    ngx_wasm_assert(ctx->pool);
    ngx_wasm_assert(ctx->log);

    if (ctx->store == NULL) {
        ctx->vm = vm;
        ctx->store = wasm_store_new(vm->engine);
        if (ctx->store == NULL) {
            return NGX_ERROR;
        }

        ngx_queue_init(&ctx->instances);
    }

    return NGX_OK;
}


void
ngx_wavm_ctx_update(ngx_wavm_ctx_t *ctx, ngx_log_t *log, void *data)
{
    ngx_queue_t          *q;
    ngx_wavm_instance_t  *instance;

    ctx->log = log;
    ctx->data = data;

    for (q = ngx_queue_head(&ctx->instances);
         q != ngx_queue_sentinel(&ctx->instances);
         q = ngx_queue_next(q))
    {
        instance = ngx_queue_data(q, ngx_wavm_instance_t, ctx_q);

        instance->log_ctx.orig_log = log;
    }
}


void
ngx_wavm_ctx_destroy(ngx_wavm_ctx_t *ctx)
{
    ngx_queue_t          *q;
    ngx_wavm_instance_t  *instance;

    if (ctx->store) {

        while (!ngx_queue_empty(&ctx->instances)) {
            q = ngx_queue_head(&ctx->instances);
            instance = ngx_queue_data(q, ngx_wavm_instance_t, ctx_q);

            ngx_wavm_instance_destroy(instance);
        }

        wasm_store_delete(ctx->store);
        ctx->store = NULL;
    }
}


ngx_wavm_instance_t *
ngx_wavm_instance_create(ngx_wavm_linked_module_t *lmodule, ngx_wavm_ctx_t *ctx)
{
    size_t                     i;
    const char                *err = NGX_WAVM_NOMEM_CHAR;
    ngx_queue_t               *q;
    ngx_wavm_t                *vm;
    ngx_wavm_module_t         *module;
    ngx_wavm_instance_t       *instance = NULL;
    ngx_wavm_hfunc_tctx_t     *tctx;
    ngx_wavm_hfunc_t          *hfunc;
    ngx_wavm_func_t           *func;
    ngx_wavm_err_t             e;
    wasm_extern_t             *wextern;
    wasm_exporttype_t         *exporttype;
    wasm_func_t               *f;
    const wasm_externtype_t   *externtype;
    const wasm_functype_t     *functype;
    const wasm_valtype_vec_t  *valtypes;

    module = lmodule->module;
    vm = module->vm;

    for (q = ngx_queue_head(&ctx->instances);
         q != ngx_queue_sentinel(&ctx->instances);
         q = ngx_queue_next(q))
    {
        instance = ngx_queue_data(q, ngx_wavm_instance_t, ctx_q);

        if (instance->lmodule->idx == lmodule->idx) {
            ngx_log_debug3(NGX_LOG_DEBUG_WASM, ctx->log, 0,
                           "wasm reusing instance of \"%V\" module "
                           "in \"%V\" vm (ctx: %p)",
                           &module->name, vm->name, ctx);

            return instance;
        }
    }

    instance = NULL;

    ngx_log_debug3(NGX_LOG_DEBUG_WASM, ctx->log, 0,
                   "wasm creating instance of \"%V\" module in \"%V\" vm "
                   "(ctx: %p)", &module->name, vm->name, ctx);

    instance = ngx_palloc(vm->pool, sizeof(ngx_wavm_instance_t));
    if (instance == NULL) {
        goto error;
    }

    instance->ctx = ctx;
    instance->lmodule = lmodule;
    instance->pool = vm->pool;
    instance->tctxs = NULL;
    instance->funcs = NULL;
    instance->memory = NULL;

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

    instance->log_ctx.vm = module->vm;
    instance->log_ctx.instance = instance;
    instance->log_ctx.orig_log = ctx->log;

    /* link hfuncs */

    wasm_extern_vec_new_uninitialized(&instance->env,
                                      lmodule->hfuncs_imports->nelts);

    if (lmodule->hfuncs_imports->nelts) {
        instance->tctxs = ngx_palloc(instance->pool,
                                     lmodule->hfuncs_imports->nelts *
                                     sizeof(ngx_wavm_hfunc_tctx_t));
        if (instance->tctxs == NULL) {
            goto error;
        }

        for (i = 0; i < instance->env.size; i++) {
            hfunc = ((ngx_wavm_hfunc_t **) lmodule->hfuncs_imports->elts)[i];

            tctx = &instance->tctxs[i];

            tctx->hfunc = hfunc;
            tctx->instance = instance;

            f = wasm_func_new_with_env(ctx->store, hfunc->functype,
                                       &ngx_wavm_hfuncs_trampoline,
                                       tctx, NULL);

            instance->env.data[i] = wasm_func_as_extern(f);
        }
    }

    /* instantiate */

    ngx_wavm_err_init(&e);

    if (ngx_wrt_instance_new(ctx->store, module->module, &instance->env,
                             &instance->instance, &e) != NGX_OK)
    {
        err = NGX_WAVM_EMPTY_CHAR;
        goto error;
    }

    /* get exports */

    wasm_instance_exports(instance->instance, &instance->exports);

    instance->funcs = ngx_palloc(instance->pool,
                                 module->exports.size
                                 * sizeof(ngx_wavm_func_t *));
    if (instance->funcs == NULL) {
        goto error;
    }

    for (i = 0; i < module->exports.size; i++) {
        wextern = ((wasm_extern_t **) instance->exports.data)[i];
        exporttype = ((wasm_exporttype_t **) module->exports.data)[i];
        externtype = wasm_exporttype_type(exporttype);

        switch (wasm_externtype_kind(externtype)) {

        case WASM_EXTERN_FUNC:
            functype = wasm_externtype_as_functype_const(externtype);

            func = ngx_palloc(instance->pool, sizeof(ngx_wavm_func_t));
            if (func == NULL) {
                goto error;
            }

            func->instance = instance;
            func->func = wasm_extern_as_func(wextern);
            func->name = wasm_exporttype_name(exporttype);
            func->argstypes = wasm_functype_params(functype);

            if (func->argstypes->size) {
                wasm_val_vec_new_uninitialized(&func->args,
                                               func->argstypes->size);

            } else {
                wasm_val_vec_new_empty(&func->args);
            }

            valtypes = wasm_functype_results(functype);

            if (valtypes->size) {
                wasm_val_vec_new_uninitialized(&func->rets, valtypes->size);

            } else {
                wasm_val_vec_new_empty(&func->rets);
            }

            instance->funcs[i] = func;
            break;

        case WASM_EXTERN_MEMORY:
            if (instance->memory) {
                err = "memory already set";
                goto error;
            }

            instance->memory = wasm_extern_as_memory(wextern);
            /* fallthrough */

        default:
            instance->funcs[i] = NULL;
            break;

        }
    }

    /* _start */

    if (module->f_start) {
        if (ngx_wavm_instance_call_funcref(instance, module->f_start, NULL)
            != NGX_OK)
        {
            err = NGX_WAVM_EMPTY_CHAR;
            goto error;
        }
    }

    ngx_queue_insert_tail(&ctx->instances, &instance->ctx_q);
    ngx_queue_insert_tail(&vm->instances, &instance->q);

    return instance;

error:

    ngx_wavm_log_error(NGX_LOG_ERR, ctx->log, &e,
                       "failed to instantiate \"%V\" module: %s",
                       &module->name, err);

    if (instance) {
        ngx_wavm_instance_destroy(instance);
    }

    return NULL;
}


static ngx_inline ngx_int_t
ngx_wavm_func_call(ngx_wavm_func_t *f, wasm_val_vec_t *args,
    wasm_val_vec_t *rets, ngx_wavm_err_t *e)
{
    ngx_int_t     rc;
    wasm_func_t  *func = f->func;

    ngx_wasm_assert(args);
    ngx_wasm_assert(rets);

    ngx_wavm_err_init(e);

    dd("wasm instance calling \"%.*s\" func (nargs: %ld, nrets: %ld)",
       (int) f->name->size, f->name->data, f->args.size, f->rets.size);

    rc = ngx_wrt_func_call(func, args, rets, e);

    dd("wasm func call rc: %ld", rc);

    return rc;
}


static void
ngx_wavm_val_vec_set(wasm_val_vec_t *out, const wasm_valtype_vec_t *valtypes,
    va_list args)
{
    size_t           i;
    uint32_t         ui32;
    uint64_t         ui64;
    float            f;
    double           d;
    wasm_valkind_t   valkind;

    for (i = 0; i < valtypes->size; i++) {
        valkind = wasm_valtype_kind(valtypes->data[i]);

        switch (valkind) {

        case WASM_I32:
            ui32 = va_arg(args, uint32_t);
            dd("arg %ld i32: %d", i, ui32);
            ngx_wasm_vec_set_i32(out, i, ui32);
            break;

        case WASM_I64:
            ui64 = va_arg(args, uint64_t);
            dd("arg %ld i64: %ld", i, ui64);
            ngx_wasm_vec_set_i64(out, i, ui64);
            break;

        case WASM_F32:
            f = va_arg(args, double);
            dd("arg %ld f32: %f", i, f);
            ngx_wasm_vec_set_f32(out, i, f);
            break;

        case WASM_F64:
            d = va_arg(args, double);
            dd("arg %ld f64: %f", i, d);
            ngx_wasm_vec_set_f64(out, i, d);
            break;

        default:
            ngx_wasm_log_error(NGX_LOG_WASM_NYI, ngx_cycle->log, 0,
                               "NYI - variadic arg of valkind \"%ui\"",
                               valkind);

        }
    }
}


static ngx_int_t
ngx_wavm_instance_call_func_va(ngx_wavm_instance_t *instance,
    ngx_wavm_func_t *f, wasm_val_vec_t **rets, va_list args)
{
    ngx_int_t        rc;
    ngx_wavm_err_t   e;

    ngx_wavm_val_vec_set(&f->args, f->argstypes, args);

    rc = ngx_wavm_func_call(f, &f->args, &f->rets, &e);
    if (rc != NGX_OK) {
        ngx_wavm_log_error(NGX_LOG_ERR, instance->log, &e, NULL);
    }

    if (rets) {
        *rets = &f->rets;
    }

    return rc;
}


ngx_int_t
ngx_wavm_instance_call_func(ngx_wavm_instance_t *instance,
    ngx_wavm_func_t *f, wasm_val_vec_t **rets, ...)
{
    va_list          args;
    ngx_int_t        rc;

    va_start(args, rets);
    rc = ngx_wavm_instance_call_func_va(instance, f, rets, args);
    va_end(args);

    return rc;
}


ngx_int_t
ngx_wavm_instance_call_func_vec(ngx_wavm_instance_t *instance,
    ngx_wavm_func_t *f, wasm_val_vec_t **rets, wasm_val_vec_t *args)
{
    size_t           i;
    ngx_int_t        rc;
    ngx_wavm_err_t   e;
    const char      *err = NULL;

    if (args && args->size) {
        if (args->size == f->args.size) {
            for (i = 0; i < f->args.size; i++) {
                f->args.data[i] = args->data[i];
            }

        } else {
            err = "bad args";
            goto error;
        }
    }

    rc = ngx_wavm_func_call(f, &f->args, &f->rets, &e);
    if (rc != NGX_OK) {
        ngx_wavm_log_error(NGX_LOG_ERR, instance->log, &e, NULL);
    }

    if (rets) {
        *rets = &f->rets;
    }

    return rc;

error:

    ngx_wavm_log_error(NGX_LOG_ERR, instance->log, NULL,
                       "failed calling \"%*s\" function: %s",
                       f->name->size, f->name->data, err);

    return NGX_ERROR;
}


ngx_int_t
ngx_wavm_instance_call_funcref(ngx_wavm_instance_t *instance,
    ngx_wavm_funcref_t *funcref, wasm_val_vec_t **rets, ...)
{
    va_list           args;
    ngx_int_t         rc;
    ngx_wavm_func_t  *f;

    ngx_wasm_assert(funcref->module == instance->lmodule->module);

    f = instance->funcs[funcref->exports_idx];

    ngx_wasm_assert(f);

    va_start(args, rets);
    rc = ngx_wavm_instance_call_func_va(instance, f, rets, args);
    va_end(args);

    return rc;
}


ngx_int_t
ngx_wavm_instance_call_funcref_vec(ngx_wavm_instance_t *instance,
    ngx_wavm_funcref_t *funcref, wasm_val_vec_t **rets, wasm_val_vec_t *args)
{
    ngx_wavm_func_t  *f;

    ngx_wasm_assert(funcref->module == instance->lmodule->module);

    f = instance->funcs[funcref->exports_idx];

    ngx_wasm_assert(f);

    return ngx_wavm_instance_call_func_vec(instance, f, rets, args);
}


void
ngx_wavm_instance_destroy(ngx_wavm_instance_t *instance)
{
    size_t                     i;
    ngx_wavm_func_t           *f;
    ngx_wavm_linked_module_t  *lmodule;

    lmodule = instance->lmodule;

    ngx_log_debug5(NGX_LOG_DEBUG_WASM, instance->pool->log, 0,
                   "wasm free instance of \"%V\" module in \"%V\" vm"
                   " (vm: %p, module: %p, instance: %p)",
                   &lmodule->module->name, lmodule->module->vm->name,
                   lmodule->module->vm, lmodule->module, instance);

    if (instance->instance) {
        wasm_extern_vec_delete(&instance->env);
        wasm_extern_vec_delete(&instance->exports);
        wasm_instance_delete(instance->instance);
    }

    if (instance->funcs) {
        for (i = 0; i < lmodule->module->exports.size; i++) {
            f = instance->funcs[i];

            if (f) {
               wasm_val_vec_delete(&f->args);
               wasm_val_vec_delete(&f->rets);

               ngx_pfree(instance->pool, f);
            }
        }

        ngx_pfree(instance->pool, instance->funcs);
    }

    if (instance->tctxs) {
        ngx_pfree(instance->pool, instance->tctxs);
    }

    if (instance->log) {
        ngx_pfree(instance->pool, instance->log);
    }

    ngx_queue_remove(&instance->q);
    ngx_queue_remove(&instance->ctx_q);

    ngx_pfree(instance->pool, instance);
}


static void
ngx_wavm_log_error(ngx_uint_t level, ngx_log_t *log, ngx_wavm_err_t *e,
    const char *fmt, ...)
{
    va_list          args;
    u_char          *p, *last, errstr[NGX_MAX_ERROR_STR];
    wasm_message_t   trapmsg;

    last = errstr + NGX_MAX_ERROR_STR;
    p = &errstr[0];

    if (fmt) {
        va_start(args, fmt);
        p = ngx_vslprintf(p, last, fmt, args);
        va_end(args);
    }

    if (e && e->trap) {
        wasm_trap_message(e->trap, &trapmsg);

        p = ngx_slprintf(p, last, "%*s",
                         trapmsg.size
#ifdef NGX_WASM_HAVE_WASMER
                         /* wasmer appends NULL in wasm_trap_message */
                         - 1
#endif
                         , trapmsg.data);

        wasm_byte_vec_delete(&trapmsg);
        wasm_trap_delete(e->trap);
    }

    if (e && e->res) {
        p = ngx_wrt_error_log_handler(e->res, p, last - p);
    }

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
        p = ngx_snprintf(buf, len,
                         " <module: \"%V\", vm: \"%V\", runtime: \"%s\">",
                         &instance->lmodule->module->name, vm->name,
                         NGX_WASM_RUNTIME);

    } else {
        p = ngx_snprintf(buf, len, " <vm: \"%V\", runtime: \"%s\">",
                         vm->name, NGX_WASM_RUNTIME);
    }

    len -= p - buf;
    buf = p;

    if (orig_log && orig_log->handler) {
        p = orig_log->handler(orig_log, buf, len);
    }

    return p;
}
