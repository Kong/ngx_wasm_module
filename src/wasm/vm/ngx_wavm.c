#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_wavm.h>


#define ngx_wavm_state(m, s)  ((m)->state & (s))


typedef enum {
    NGX_WAVM_INIT = (1 << 0),
    NGX_WAVM_READY = (1 << 1),
} ngx_wavm_state;

typedef enum {
    NGX_WAVM_MODULE_ISWAT = (1 << 0),
    NGX_WAVM_MODULE_LOADED_BYTES = (1 << 1),
    NGX_WAVM_MODULE_LOADED = (1 << 2),
    NGX_WAVM_MODULE_LINKED = (1 << 3),
    NGX_WAVM_MODULE_INVALID = (1 << 4),
} ngx_wavm_module_state;

typedef enum {
    NGX_WAVM_INSTANCE_INIT = (1 << 0),
    NGX_WAVM_INSTANCE_CREATED = (1 << 1),
    NGX_WAVM_STORE_CREATED = (1 << 2),
    NGX_WAVM_INSTANCE_TRAPPED = (1 << 3),
} ngx_wavm_instance_state;


static ngx_int_t ngx_wavm_engine_init(ngx_wavm_t *vm);
static void ngx_wavm_engine_destroy(ngx_wavm_t *vm);
static void ngx_wavm_destroy_instances(ngx_wavm_t *vm);
static ngx_int_t ngx_wavm_module_load_bytes(ngx_wavm_module_t *module);
static ngx_int_t ngx_wavm_module_load(ngx_wavm_module_t *module);
static void ngx_wavm_module_destroy(ngx_wavm_module_t *module);
static ngx_int_t ngx_wavm_func_call(ngx_wavm_func_t *f, wasm_val_vec_t *args,
    wasm_val_vec_t *rets, ngx_wrt_err_t *e);
static void ngx_wavm_val_vec_set(wasm_val_vec_t *out,
    const wasm_valtype_vec_t *valtypes, va_list args);
static ngx_int_t ngx_wavm_instance_call_func_va(ngx_wavm_instance_t *instance,
    ngx_wavm_func_t *f, wasm_val_vec_t **rets, va_list args);
static u_char *ngx_wavm_log_error_handler(ngx_log_t *log, u_char *buf,
    size_t len);


static const char  NGX_WAVM_NOMEM_CHAR[] = "no memory";
static const char  NGX_WAVM_EMPTY_CHAR[] = "";


ngx_wavm_t *
ngx_wavm_create(ngx_cycle_t *cycle, const ngx_str_t *name,
    ngx_wavm_conf_t *vm_conf, ngx_wavm_host_def_t *core_host)
{
    ngx_wavm_t  *vm;

    vm = ngx_pcalloc(cycle->pool, sizeof(ngx_wavm_t));
    if (vm == NULL) {
        goto error;
    }

    vm->name = name;
    vm->config = vm_conf;
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
ngx_wavm_engine_init(ngx_wavm_t *vm)
{
    ngx_int_t       rc;
    ngx_wrt_err_t   e;
    wasm_config_t  *config;
    const char     *err = NGX_WAVM_EMPTY_CHAR;

    ngx_wrt_err_init(&e);

    if (ngx_wavm_state(vm, NGX_WAVM_INIT)) {
        /* reentrant */
        return NGX_ABORT;
    }

    ngx_log_debug2(NGX_LOG_DEBUG_WASM, vm->pool->log, 0,
                   "wasm initializing \"%V\" vm engine (engine: %p)",
                   vm->name, &vm->wrt_engine);

    config = wasm_config_new();

    if (vm->config
        && ngx_wrt.conf_init(config, vm->config, vm->log) != NGX_OK)
    {
        err = "invalid configuration";
        wasm_config_delete(config);
        goto error;
    }

    rc = ngx_wrt.engine_init(&vm->wrt_engine, config, vm->pool, &e);
    if (rc != NGX_OK) {
        goto error;
    }

    vm->state |= NGX_WAVM_INIT;

    return NGX_OK;

error:

    ngx_wavm_log_error(NGX_LOG_EMERG, vm->log, &e,
                       "failed initializing wasm VM: %s", err);

    ngx_wavm_engine_destroy(vm);

    return NGX_ERROR;
}


ngx_int_t
ngx_wavm_init(ngx_wavm_t *vm)
{
    ngx_uint_t          rc = NGX_ERROR;
    ngx_str_node_t     *sn;
    ngx_rbtree_node_t  *root, *sentinel, *node;
    ngx_wavm_module_t  *module;

    ngx_wavm_log_error(NGX_LOG_INFO, vm->log, NULL,
                       "initializing \"%V\" wasm VM", vm->name);

    rc = ngx_wavm_engine_init(vm);
    if (rc != NGX_OK) {
        goto done;
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
        module = ngx_rbtree_data(&sn->node, ngx_wavm_module_t, sn);

        rc = ngx_wavm_module_load_bytes(module);
        if (rc != NGX_OK) {
            goto done;
        }
    }

empty:

    ngx_wavm_log_error(NGX_LOG_INFO, vm->log, NULL,
                       "\"%V\" wasm VM initialized",
                       vm->name);

    rc = NGX_OK;

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
    ngx_wrt_err_t      e;

    ngx_wrt_err_init(&e);

    if (ngx_wavm_engine_init(vm) != NGX_OK) {
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
        module = ngx_rbtree_data(&sn->node, ngx_wavm_module_t, sn);

        if (ngx_wavm_module_load(module) != NGX_OK) {
            return NGX_ERROR;
        }
    }

done:

    vm->state |= NGX_WAVM_READY;

    return NGX_OK;
}


static void
ngx_wavm_engine_destroy(ngx_wavm_t *vm)
{
    if (ngx_wavm_state(vm, NGX_WAVM_INIT)) {
        ngx_log_debug2(NGX_LOG_DEBUG_WASM, ngx_cycle->log, 0,
                       "wasm deleting \"%V\" engine (engine: %p)",
                       vm->name, &vm->wrt_engine);

        ngx_wrt.engine_destroy(&vm->wrt_engine);

        vm->state = 0;
    }
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

    ngx_log_debug2(NGX_LOG_DEBUG_WASM, ngx_cycle->log, 0,
                  "wasm freeing \"%V\" vm (vm: %p)",
                   vm->name, vm);

    ngx_wavm_destroy_instances(vm);

    root = &vm->modules_tree.root;
    sentinel = &vm->modules_tree.sentinel;

    while (*root != *sentinel) {
        node = ngx_rbtree_min(*root, *sentinel);
        sn = ngx_wasm_sn_n2sn(node);
        module = ngx_rbtree_data(&sn->node, ngx_wavm_module_t, sn);

        ngx_rbtree_delete(&vm->modules_tree, node);

        ngx_wavm_module_destroy(module);
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

    return ngx_rbtree_data(&sn->node, ngx_wavm_module_t, sn);
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

    ngx_array_init(&module->hfuncs, vm->pool, 2,
                   sizeof(ngx_wavm_hfunc_t *));

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

    return NGX_OK;

error:

    ngx_wavm_log_error(NGX_LOG_EMERG, vm->log, NULL,
                       "failed adding \"%V\" module from \"%V\": %s",
                       name, path, err);

    if (module) {
        ngx_wavm_module_destroy(module);
    }

    return NGX_ERROR;
}


static ngx_int_t
ngx_wavm_module_load_bytes(ngx_wavm_module_t *module)
{
    const char       *err = NGX_WAVM_EMPTY_CHAR;
    ngx_int_t         rc;
    ngx_wavm_t       *vm;
    ngx_wrt_err_t    e;
    wasm_byte_vec_t   file_bytes;

    vm = module->vm;

    ngx_wrt_err_init(&e);

    ngx_log_debug4(NGX_LOG_DEBUG_WASM, vm->log, 0,
                   "wasm loading \"%V\" module bytes from \"%V\""
                   " (module: %p, engine: %p)",
                   &module->name, &module->path,
                   &module->wrt_module, &module->vm->wrt_engine);

    rc = ngx_wasm_bytes_from_path(&file_bytes, module->path.data, vm->log);
    if (rc != NGX_OK) {
        return rc;
    }

    if (ngx_wavm_state(module, NGX_WAVM_MODULE_ISWAT)) {
        ngx_log_debug1(NGX_LOG_DEBUG_WASM, vm->log, 0,
                       "wasm compiling wat at \"%V\"", &module->path);

        rc = ngx_wrt.wat2wasm(&file_bytes, &module->bytes, &e);

        wasm_byte_vec_delete(&file_bytes);

        if (rc != NGX_OK) {
            goto error;
        }

    } else {
        module->bytes.size = file_bytes.size;
        module->bytes.data = file_bytes.data;
    }

    if (!module->bytes.size) {
        err = "unexpected EOF";
        goto error;
    }

    rc = ngx_wrt.validate(&vm->wrt_engine, &module->bytes, &e);
    if (rc != NGX_OK) {
        err = "invalid module";
        goto error;
    }

    module->state |= NGX_WAVM_MODULE_LOADED_BYTES;

    rc = NGX_OK;

    goto done;

error:

    rc = NGX_ERROR;

    ngx_wavm_log_error(NGX_LOG_EMERG, vm->log, &e,
                       "failed loading \"%V\" module bytes: %s",
                       &module->name, err);

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
    ngx_wrt_err_t             e;
    ngx_wavm_t               *vm;
    ngx_wavm_funcref_t       *funcref;
    wasm_exporttype_t        *exporttype;
    const wasm_externtype_t  *externtype;
    const wasm_importtype_t  *importtype;
    const wasm_name_t        *exportname;
#if (DDEBUG)
    const wasm_name_t        *importname, *importmodule;
#endif
    ngx_msec_t                start_time, end_time;

    vm = module->vm;

    if (ngx_wavm_state(module, NGX_WAVM_MODULE_LOADED)) {
        return NGX_OK;
    }

    if (!ngx_wavm_state(module, NGX_WAVM_MODULE_LOADED_BYTES)) {
        return NGX_ERROR;
    }

    ngx_wavm_log_error(NGX_LOG_INFO, vm->log, NULL,
                       "loading \"%V\" module", &module->name);

    ngx_wrt_err_init(&e);

    ngx_time_update();
    start_time = ngx_current_msec;

    rc = ngx_wrt.module_init(&module->wrt_module, &vm->wrt_engine,
                             &module->bytes,
                             &module->imports, &module->exports, &e);
    if (rc != NGX_OK) {
        err = NGX_WAVM_EMPTY_CHAR;
        goto error;
    }

    for (i = 0; i < module->imports.size; i++) {
        importtype = ((wasm_importtype_t **) module->imports.data)[i];
#if (DDEBUG)
        importmodule = wasm_importtype_module(importtype);
        importname = wasm_importtype_name(importtype);

        dd("checking \"%.*s\" module import \"%.*s.%.*s\" (%lu/%lu)",
           (int) module->name.len, module->name.data,
           (int) importmodule->size, importmodule->data,
           (int) importname->size, importname->data,
           i + 1, module->imports.size);
#endif

        if (wasm_externtype_kind(wasm_importtype_type(importtype))
            != WASM_EXTERN_FUNC)
        {
            ngx_wavm_log_error(NGX_LOG_WASM_NYI, vm->log, NULL,
                               "NYI: module import type not supported");
            goto failed;
        }
    }

    /* build exports lookup */

    ngx_rbtree_init(&module->funcs_tree, &module->funcs_sentinel,
                    ngx_str_rbtree_insert_value);

    for (i = 0; i < module->exports.size; i++) {
        exporttype = ((wasm_exporttype_t **) module->exports.data)[i];
        exportname = wasm_exporttype_name(exporttype);
        externtype = wasm_exporttype_type(exporttype);

        dd("caching \"%.*s\" module export \"%.*s\" (%lu/%lu)",
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

            if (module->f_start == NULL
                && (ngx_strncmp(exportname->data, "_start", 6) == 0
                    || ngx_strncmp(exportname->data, "_initialize", 11) == 0))
            {
                module->f_start = funcref;
            }

            break;

        default:
            break;

        }
    }

    module->idx = vm->modules_max++;
    module->state |= NGX_WAVM_MODULE_LOADED;

    ngx_time_update();
    end_time = ngx_current_msec;

    ngx_wavm_log_error(NGX_LOG_INFO, vm->log, NULL,
                       "successfully loaded \"%V\" module in %d ms",
                       &module->name, end_time - start_time);

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
}


ngx_int_t
ngx_wavm_module_link(ngx_wavm_module_t *module, ngx_wavm_host_def_t *host)
{
    const char               *err = NGX_WAVM_EMPTY_CHAR;
    size_t                    i;
    ngx_int_t                 rc;
    ngx_str_t                 s;
    ngx_wavm_t               *vm = module->vm;
    ngx_wavm_hfunc_t         *hfunc, **hfuncp;
    ngx_wrt_err_t            e;
    const wasm_importtype_t  *importtype;
    const wasm_name_t        *importmodule, *importname;

    ngx_wrt_err_init(&e);

    if (ngx_wavm_state(module, NGX_WAVM_MODULE_INVALID)) {
        err = "invalid module";
        goto error;

    } else if (!ngx_wavm_state(module, NGX_WAVM_MODULE_LOADED)) {
        err = "module not loaded";
        goto error;

    } else if (ngx_wavm_state(module, NGX_WAVM_MODULE_LINKED)) {
        if (module->host_def != host) {
            err = "module already linked to a different host";
            goto error;
        }

        rc = NGX_OK;
        goto done;
    }

    ngx_log_debug5(NGX_LOG_DEBUG_WASM, vm->log, 0,
                   "wasm linking \"%V\" module to \"%V\" host interface"
                   " (vm: %p, module: %p, host: %p)",
                   &module->name, &host->name, vm, module, host);

    module->host_def = host;

    for (i = 0; i < module->imports.size; i++) {
        importtype = ((wasm_importtype_t **) module->imports.data)[i];
        importmodule = wasm_importtype_module(importtype);
        importname = wasm_importtype_name(importtype);

        dd("wasm loading \"%.*s\" module import \"%.*s.%.*s\" (%lu/%lu)",
           (int) module->name.len, module->name.data,
           (int) importmodule->size, importmodule->data,
           (int) importname->size, importname->data,
           i + 1, module->imports.size);

        if (ngx_strncmp(importmodule->data, "env", 3) != 0) {
            dd("skipping");
            continue;
        }

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
                               "failed importing \"%*s.%*s\": "
                               "missing host function",
                               importmodule->size, importmodule->data,
                               importname->size, importname->data);

            err = "incompatible host interface";
            goto error;
        }

        hfunc->idx = i;

        hfuncp = ngx_array_push(&module->hfuncs);
        if (hfuncp == NULL) {
            err = NGX_WAVM_NOMEM_CHAR;
            goto error;
        }

        *hfuncp = hfunc;
    }

    rc = ngx_wrt.module_link(&module->wrt_module, &module->hfuncs, &e);
    if (rc != NGX_OK) {
        goto error;
    }

    rc = NGX_OK;

    module->state |= NGX_WAVM_MODULE_LINKED;

    ngx_log_debug5(NGX_LOG_DEBUG_WASM, vm->log, 0,
                   "wasm \"%V\" module successfully linked to \"%V\" host"
                   " interface (vm: %p, module: %p, host: %p)",
                   &module->name, &host->name, vm, module, host);

    goto done;

error:

    rc = NGX_ERROR;

    ngx_wavm_log_error(NGX_LOG_EMERG, vm->log, &e,
                       "failed linking \"%V\" module with \"%V\" "
                       "host interface: %s",
                       &module->name, &host->name, err);

done:

    return rc;
}


static void
ngx_wavm_module_destroy(ngx_wavm_module_t *module)
{
    size_t                i;
    ngx_str_node_t       *sn;
    ngx_rbtree_node_t   **root, **sentinel, *node;
    ngx_wavm_hfunc_t     *hfunc;
    ngx_wavm_funcref_t   *funcref;
    ngx_wavm_t           *vm = module->vm;

    ngx_log_debug4(NGX_LOG_DEBUG_WASM, vm->log, 0,
                   "wasm freeing \"%V\" module in \"%V\" vm"
                   " (vm: %p, module: %p)",
                   &module->name, vm->name, vm, module);

    if (ngx_wavm_state(module, NGX_WAVM_MODULE_LOADED)) {
        wasm_importtype_vec_delete(&module->imports);
        wasm_exporttype_vec_delete(&module->exports);
        ngx_wrt.module_destroy(&module->wrt_module);
    }

    root = &module->funcs_tree.root;
    sentinel = &module->funcs_tree.sentinel;

    while (*root != *sentinel) {
        node = ngx_rbtree_min(*root, *sentinel);
        sn = ngx_wasm_sn_n2sn(node);
        funcref = ngx_rbtree_data(&sn->node, ngx_wavm_funcref_t, sn);

        ngx_rbtree_delete(&module->funcs_tree, node);

        ngx_pfree(vm->pool, funcref);
    }

    if (module->bytes.size) {
        wasm_byte_vec_delete(&module->bytes);
    }

    if (module->name.data) {
        ngx_pfree(vm->pool, module->name.data);
    }

    if (module->path.data) {
        ngx_pfree(vm->pool, module->path.data);
    }

    for (i = 0; i < module->hfuncs.nelts; i++) {
        hfunc = ((ngx_wavm_hfunc_t **) module->hfuncs.elts)[i];

        ngx_wavm_host_hfunc_destroy(hfunc);
    }

    ngx_array_destroy(&module->hfuncs);

    ngx_pfree(vm->pool, module);
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

    return ngx_rbtree_data(&sn->node, ngx_wavm_funcref_t, sn);
}


#if 0
static void
ngx_wavm_instance_cleanup(void *data)
{
    ngx_wavm_instance_t  *instance = data;

    ngx_log_debug1(NGX_LOG_DEBUG_WASM, instance->log, 0,
                   "wasm cleaning up instance pool (instance: %p)",
                   instance);

    ngx_wavm_instance_destroy(instance);
}
#endif


ngx_wavm_instance_t *
ngx_wavm_instance_create(ngx_wavm_module_t *module, ngx_pool_t *pool,
    ngx_log_t *log, void *data)
{
    size_t                     i;
    ngx_int_t                  rc;
    u_char                    *p;
    const char                *err = NGX_WAVM_NOMEM_CHAR;
    ngx_wavm_t                *vm = module->vm;
    ngx_wavm_instance_t       *instance = NULL;
    ngx_wavm_func_t           *func;
    ngx_wrt_err_t              e;
    ngx_wrt_extern_t          *wextern;
    wasm_exporttype_t         *exporttype;
    const wasm_name_t         *exportname;
    const wasm_valtype_vec_t  *valtypes;

    ngx_wrt_err_init(&e);

    ngx_log_debug2(NGX_LOG_DEBUG_WASM, log, 0,
                   "wasm creating \"%V\" instance in \"%V\" vm",
                   &module->name, vm->name);

    if (!ngx_wavm_state(module, NGX_WAVM_MODULE_LINKED)) {
        err = "module not linked to host";
        goto error;
    }

    instance = ngx_pcalloc(pool, sizeof(ngx_wavm_instance_t));
    if (instance == NULL) {
        goto error;
    }

    instance->vm = vm;
    instance->module = module;
    instance->pool = pool;
    instance->externs = NULL;
    instance->memory = NULL;
    instance->data = data;

    ngx_wrt_err_init(&instance->wrt_error);

    ngx_array_init(&instance->funcs, instance->pool, module->exports.size,
                   sizeof(ngx_wavm_func_t));

#if 0
    instance->cln = ngx_pool_cleanup_add(instance->pool, 0);
    if (instance->cln == NULL) {
        goto error;
    }

    instance->cln->handler = ngx_wavm_instance_cleanup;
    instance->cln->data = instance;
#endif

    instance->log = ngx_pcalloc(instance->pool, sizeof(ngx_log_t));
    if (instance->log == NULL) {
        goto error;
    }

    instance->log->file = log->file;
    instance->log->next = log->next;
    instance->log->wdata = log->wdata;
    instance->log->writer = log->writer;
    instance->log->log_level = log->log_level;
    instance->log->handler = ngx_wavm_log_error_handler;
    instance->log->connection = log->connection;
    instance->log->data = &instance->log_ctx;

    instance->log_ctx.vm = module->vm;
    instance->log_ctx.instance = instance;
    instance->log_ctx.orig_log = log;

    rc = ngx_wrt.store_init(&instance->wrt_store, &vm->wrt_engine, instance);
    if (rc != NGX_OK) {
        goto error;
    }

    instance->state |= NGX_WAVM_STORE_CREATED;

    /* instantiate */

    rc = ngx_wrt.instance_init(&instance->wrt_instance,
                               &instance->wrt_store,
                               &module->wrt_module,
                               instance->pool, &e);
    if (rc != NGX_OK) {
        err = NGX_WAVM_EMPTY_CHAR;
        goto error;
    }

    instance->state |= NGX_WAVM_INSTANCE_CREATED;

    /* cache exports */

    instance->externs = ngx_pcalloc(instance->pool,
                                    module->exports.size
                                    * sizeof(ngx_wrt_extern_t));
    if (instance->externs == NULL) {
        goto error;
    }

    for (i = 0; i < module->exports.size; i++) {
        exporttype = ((wasm_exporttype_t **) module->exports.data)[i];
        exportname = wasm_exporttype_name(exporttype);
        wextern = &((ngx_wrt_extern_t *) instance->externs)[i];

        rc = ngx_wrt.extern_init(wextern, &instance->wrt_instance, i);
        if (rc != NGX_OK) {
            goto error;
        }

        func = ngx_array_push(&instance->funcs);
        if (func == NULL) {
            goto error;
        }

        /* stub for other extern kinds */
        func->functype = NULL;
        func->name.len = exportname->size;
        func->name.data = ngx_pnalloc(instance->pool, func->name.len + 1);
        if (func->name.data == NULL) {
            goto error;
        }

        p = ngx_copy(func->name.data, exportname->data, func->name.len);
        *p = '\0';

        switch (wextern->kind) {

        case NGX_WRT_EXTERN_FUNC:
#ifdef NGX_WASM_HAVE_WASMTIME
            func->functype = wasmtime_func_type(
                                 instance->wrt_instance.store->context,
                                 &wextern->ext.of.func);

#else
            func->functype = wasm_func_type(wasm_extern_as_func(wextern->ext));
#endif
            func->idx = i;
            func->ext = wextern;
            func->instance = instance;
            func->argstypes = wasm_functype_params(func->functype);

            if (func->argstypes->size) {
                wasm_val_vec_new_uninitialized(&func->args,
                                               func->argstypes->size);

            } else {
                wasm_val_vec_new_empty(&func->args);
            }

            valtypes = wasm_functype_results(func->functype);

            if (valtypes->size) {
                wasm_val_vec_new_uninitialized(&func->rets, valtypes->size);

            } else {
                wasm_val_vec_new_empty(&func->rets);
            }

            break;

        case NGX_WRT_EXTERN_MEMORY:
            ngx_wasm_assert(instance->memory == NULL);
            instance->memory = wextern;
            break;

        }
    }

    ngx_wasm_assert(instance->funcs.nelts == module->exports.size);

    /* _start */

    if (module->f_start) {
        if (ngx_wavm_instance_call_funcref(instance, module->f_start, NULL)
            != NGX_OK)
        {
            err = NGX_WAVM_EMPTY_CHAR;
            goto error;
        }
    }

    //ngx_queue_insert_tail(&vm->instances, &instance->q);

    return instance;

error:

    ngx_wavm_log_error(NGX_LOG_ERR, log, &e,
                       "failed instantiating \"%V\" module: %s",
                       &module->name, err);

    if (instance) {
        ngx_wavm_instance_destroy(instance);
    }

    return NULL;
}


static ngx_inline ngx_int_t
ngx_wavm_func_call(ngx_wavm_func_t *f, wasm_val_vec_t *args,
    wasm_val_vec_t *rets, ngx_wrt_err_t *e)
{
    ngx_int_t   rc;

    ngx_wasm_assert(args);
    ngx_wasm_assert(rets);

    if (ngx_wavm_state(f->instance, NGX_WAVM_INSTANCE_TRAPPED)) {
        rc = NGX_ABORT;
        goto done;
    }

    ngx_wrt_err_init(e);

    rc = ngx_wrt.call(&f->instance->wrt_instance,
                      &f->name, f->idx,
                      args, rets, e);

    if (rc == NGX_ABORT) {
        f->instance->state |= NGX_WAVM_INSTANCE_TRAPPED;
        f->instance->trapped = 1;
    }

done:

    dd("\"%.*s\" rc: %ld", (int) f->name.len, f->name.data, rc);

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
            dd("arg %ld i32: %u", i, ui32);
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
    ngx_wavm_func_t *func, wasm_val_vec_t **rets, va_list args)
{
    ngx_int_t   rc;

    ngx_wavm_val_vec_set(&func->args, func->argstypes, args);

    rc = ngx_wavm_func_call(func, &func->args, &func->rets,
                            &instance->wrt_error);
    if (rc != NGX_OK) {
        ngx_wavm_log_error(NGX_LOG_ERR, instance->log, &instance->wrt_error,
                           "%s in %V:",
                           rc == NGX_ABORT ? "trap" : "error",
                           &func->name);
    }

    if (rets) {
        *rets = &func->rets;
    }

    ngx_wasm_assert(rc == NGX_OK || rc == NGX_ERROR || rc == NGX_ABORT);

    return rc;
}


ngx_int_t
ngx_wavm_instance_call_func(ngx_wavm_instance_t *instance,
    ngx_wavm_func_t *func, wasm_val_vec_t **rets, ...)
{
    va_list     args;
    ngx_int_t   rc;

    va_start(args, rets);
    rc = ngx_wavm_instance_call_func_va(instance, func, rets, args);
    va_end(args);

    return rc;
}


ngx_int_t
ngx_wavm_instance_call_func_vec(ngx_wavm_instance_t *instance,
    ngx_wavm_func_t *func, wasm_val_vec_t **rets, wasm_val_vec_t *args)
{
    size_t       i;
    ngx_int_t    rc;
    const char  *err = NULL;

    if (args && args->size) {
        if (args->size == func->args.size) {
            for (i = 0; i < func->args.size; i++) {
                func->args.data[i] = args->data[i];
            }

        } else {
            err = "bad args";
            goto error;
        }
    }

    rc = ngx_wavm_func_call(func, &func->args, &func->rets,
                            &instance->wrt_error);
    if (rc != NGX_OK) {
        ngx_wavm_log_error(NGX_LOG_ERR, instance->log, &instance->wrt_error,
                           "in %V", &func->name);
    }

    if (rets) {
        *rets = &func->rets;
    }

    return rc;

error:

    ngx_wavm_log_error(NGX_LOG_ERR, instance->log, NULL,
                       "failed calling \"%*s\" function: %s",
                       func->name.len, func->name.data, err);

    return NGX_ERROR;
}


ngx_int_t
ngx_wavm_instance_call_funcref(ngx_wavm_instance_t *instance,
    ngx_wavm_funcref_t *funcref, wasm_val_vec_t **rets, ...)
{
    va_list           args;
    ngx_int_t         rc;
    ngx_wavm_func_t  *func;

#if 0
    ngx_log_debug4(NGX_LOG_DEBUG_WASM, instance->log, 0,
                   "wasm calling \"%V.%V\" function "
                   "(instance: %p, store: %p)",
                   &instance->module->name, &funcref->name,
                   instance, instance->wrt_store.store);
#endif

    ngx_wasm_assert(funcref->module == instance->module);

    func = &((ngx_wavm_func_t *) instance->funcs.elts)[funcref->exports_idx];

    ngx_wasm_assert(func);

    va_start(args, rets);
    rc = ngx_wavm_instance_call_func_va(instance, func, rets, args);
    va_end(args);

    dd("\"%.*s.%.*s\" rc: %ld",
       (int) instance->module->name.len, instance->module->name.data,
       (int) funcref->name.len, funcref->name.data, rc);

    return rc;
}


ngx_int_t
ngx_wavm_instance_call_funcref_vec(ngx_wavm_instance_t *instance,
    ngx_wavm_funcref_t *funcref, wasm_val_vec_t **rets, wasm_val_vec_t *args)
{
    ngx_wavm_func_t  *func;

    ngx_wasm_assert(funcref->module == instance->module);

    func = &((ngx_wavm_func_t *) instance->funcs.elts)[funcref->exports_idx];

    ngx_wasm_assert(func);

    return ngx_wavm_instance_call_func_vec(instance, func, rets, args);
}


void
ngx_wavm_instance_trap_printf(ngx_wavm_instance_t *instance,
    const char *fmt, ...)
{
    va_list  args;

    va_start(args, fmt);
    ngx_wavm_instance_trap_vprintf(instance, fmt, args);
    va_end(args);
}


void
ngx_wavm_instance_trap_vprintf(ngx_wavm_instance_t *instance,
    const char *fmt, va_list args)
{
    u_char               *p;
    static const size_t   maxlen = NGX_WAVM_HFUNCS_MAX_TRAP_LEN - 1;

    if (!instance->trapmsg.len) {
        p = ngx_vsnprintf(instance->trapbuf, maxlen, fmt, args);

        *p++ = '\0';

        instance->trapmsg.len = p - instance->trapbuf;
        instance->trapmsg.data = (u_char *) instance->trapbuf;
    }
}


void
ngx_wavm_instance_destroy(ngx_wavm_instance_t *instance)
{
    size_t              i;
    ngx_wrt_extern_t   *wextern;
    ngx_wavm_func_t    *func;
    ngx_wavm_module_t  *module = instance->module;

    ngx_log_debug5(NGX_LOG_DEBUG_WASM,
                   instance->log ? instance->log : ngx_cycle->log, 0,
                   "wasm freeing \"%V\" instance in \"%V\" vm"
                   " (vm: %p, module: %p, instance: %p)",
                   &module->name, module->vm->name,
                   module->vm, module, instance);

    if (instance->funcs.nelts) {
        for (i = 0; i < instance->funcs.nelts; i++) {
            func = &((ngx_wavm_func_t *) instance->funcs.elts)[i];

            if (func->functype) {
                dd("freeing func \"%*s\" (functype: %p, i: %zu)",
                   (int) func->name.len, (u_char *) func->name.data,
                   func->functype, i);

                wasm_val_vec_delete(&func->args);
                wasm_val_vec_delete(&func->rets);
                wasm_functype_delete(func->functype);
                func->functype = NULL;
            }
        }
    }

    ngx_array_destroy(&instance->funcs);

    if (instance->externs) {
        for (i = 0; i < module->exports.size; i++) {
            wextern = &((ngx_wrt_extern_t *) instance->externs)[i];
            ngx_wrt.extern_destroy(wextern);
        }
    }

    if (instance->log) {
        ngx_pfree(instance->pool, instance->log);
    }

    if (ngx_wavm_state(instance, NGX_WAVM_INSTANCE_CREATED)) {
        ngx_wrt.instance_destroy(&instance->wrt_instance);
    }

    if (ngx_wavm_state(instance, NGX_WAVM_STORE_CREATED)) {
        ngx_wrt.store_destroy(&instance->wrt_store);
    }

    if (instance->cln) {
        /* cancel cleanup */
        instance->cln->handler = NULL;
    }

    //ngx_queue_remove(&instance->q);

    ngx_pfree(instance->pool, instance);
}


void
ngx_wavm_log_error(ngx_uint_t level, ngx_log_t *log, ngx_wrt_err_t *e,
    const char *fmt, ...)
{
    va_list          args;
    wasm_message_t   trapmsg;
    u_char          *p, *last, errstr[NGX_MAX_ERROR_STR];

    last = errstr + NGX_MAX_ERROR_STR;
    p = &errstr[0];

    if (fmt) {
        va_start(args, fmt);
        p = ngx_vslprintf(p, last, fmt, args);
        va_end(args);
    }

    if (e && e->trap) {
        if (fmt) {
            p = ngx_slprintf(p, last, " ");
        }

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
        p = ngx_wrt.log_handler(e->res, p, last - p);
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
                         &instance->module->name, vm->name,
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
