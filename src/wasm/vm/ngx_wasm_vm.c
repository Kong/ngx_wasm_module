#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_core.h>
#include <ngx_wasm_vm.h>
#include <ngx_wasm_core.h>


#define NGX_WASM_MODULE_ISWAT        (1 << 0)
#define NGX_WASM_MODULE_LOADED       (1 << 1)

#define ngx_wasm_vm_check_init(vm, ret)                                      \
    if (vm->runtime == NULL) {                                               \
        ngx_wasm_log_error(NGX_LOG_EMERG, &vm->log, 0,                       \
                           "\"%V\" vm not initialized (vm: %p)",             \
                           &vm->name, vm);                                   \
        ngx_wasm_assert(0);                                                  \
        return ret;                                                          \
    }                                                                        \


static ngx_int_t ngx_wasm_vm_load_module(ngx_wasm_vm_t *vm,
    ngx_str_t *mod_name);

static void ngx_wasm_vm_log_error(ngx_uint_t level, ngx_log_t *log,
    ngx_wrt_error_pt err, wasm_trap_t *trap,
#if (NGX_HAVE_VARIADIC_MACROS)
    const char *fmt, ...);

#else
    const char *fmt, va_list args);
#endif
static u_char *ngx_wasm_vm_log_error_handler(ngx_log_t *log, u_char *buf,
    size_t len);


ngx_wasm_vm_t *
ngx_wasm_vm_new(ngx_cycle_t *cycle, ngx_str_t *vm_name)
{
    u_char         *p;
    ngx_wasm_vm_t  *vm;

    vm = ngx_pcalloc(cycle->pool, sizeof(ngx_wasm_vm_t));
    if (vm == NULL) {
        return NULL;
    }

    vm->pool = cycle->pool;
    vm->log_ctx.vm = vm;
    vm->log_ctx.orig_log = &cycle->new_log;

    vm->name.len = vm_name->len;
    vm->name.data = ngx_pnalloc(vm->pool, vm->name.len + 1);
    if (vm->name.data == NULL) {
        goto failed;
    }

    p = ngx_copy(vm->name.data, vm_name->data, vm->name.len);
    *p = '\0';

    vm->log.file = cycle->new_log.file;
    vm->log.next = cycle->new_log.next;
    vm->log.writer = cycle->new_log.writer;
    vm->log.wdata = cycle->new_log.wdata;
    vm->log.log_level = cycle->new_log.log_level;
    vm->log.handler = ngx_wasm_vm_log_error_handler;
    vm->log.data = &vm->log_ctx;

    ngx_rbtree_init(&vm->modules_tree, &vm->modules_sentinel,
                    ngx_str_rbtree_insert_value);

    ngx_rbtree_init(&vm->hfuncs_tree, &vm->hfuncs_sentinel,
                    ngx_str_rbtree_insert_value);

    vm->config = wasm_config_new();
    vm->engine = wasm_engine_new_with_config(vm->config);
    if (vm->engine == NULL) {
        goto failed;
    }

    ngx_log_debug2(NGX_LOG_DEBUG_WASM, cycle->log, 0,
                   "wasm created \"%V\" vm (vm: %p)",
                   &vm->name, vm);

    return vm;

failed:

    ngx_wasm_log_error(NGX_LOG_EMERG, cycle->log, 0,
                       "failed to create \"%V\" vm", vm_name);

    ngx_wasm_vm_free(vm);

    return NULL;
}


void
ngx_wasm_vm_free(ngx_wasm_vm_t *vm)
{
    ngx_queue_t              *q;
    ngx_rbtree_node_t       **root, **sentinel, *node;
    ngx_str_node_t           *sn;
    ngx_wasm_vm_module_t     *module;
    ngx_wasm_vm_instance_t   *inst;
    ngx_wasm_hfunc_t         *hfunc;

    ngx_log_debug2(NGX_LOG_DEBUG_WASM, vm->pool->log, 0,
                   "wasm free \"%V\" vm (vm: %p)",
                   &vm->name, vm);

    root = &vm->modules_tree.root;
    sentinel = &vm->modules_tree.sentinel;

    while (*root != *sentinel) {
        node = ngx_rbtree_min(*root, *sentinel);
        sn = ngx_wasm_sn_n2sn(node);
        module = ngx_wasm_sn_sn2data(sn, ngx_wasm_vm_module_t, sn);

        ngx_rbtree_delete(&vm->modules_tree, node);

        ngx_log_debug4(NGX_LOG_DEBUG_WASM, vm->pool->log, 0,
                       "wasm free \"%V\" module in \"%V\" vm"
                       " (vm: %p, module: %p)",
                       &module->name, &vm->name, vm, module);

        while (!ngx_queue_empty(&module->instances_queue)) {
            q = ngx_queue_head(&module->instances_queue);

            inst = ngx_queue_data(q, ngx_wasm_vm_instance_t, queue);
            ngx_wasm_vm_instance_free(inst);
        }

        if (module->module) {
            wasm_module_delete(module->module);
        }

        wasm_exporttype_vec_delete(&module->exports);

        ngx_pfree(vm->pool, module->hfuncs);
        ngx_pfree(vm->pool, module->name.data);
        ngx_pfree(vm->pool, module->path.data);
        ngx_pfree(vm->pool, module);
    }

    root = &vm->hfuncs_tree.root;
    sentinel = &vm->hfuncs_tree.sentinel;

    while (*root != *sentinel) {
        node = ngx_rbtree_min(*root, *sentinel);
        sn = ngx_wasm_sn_n2sn(node);
        hfunc = ngx_wasm_sn_sn2data(sn, ngx_wasm_hfunc_t, sn);

        ngx_log_debug1(NGX_LOG_DEBUG_WASM, vm->pool->log, 0,
                       "wasm free \"%V\" host function",
                       hfunc->name);

        ngx_rbtree_delete(&vm->hfuncs_tree, node);

        wasm_functype_delete(hfunc->functype);

        ngx_pfree(vm->pool, hfunc);
    }

    if (vm->runtime && vm->runtime->shutdown) {
        vm->runtime->shutdown(vm);
    }

    if (vm->engine) {
        wasm_engine_delete(vm->engine);
    }

    if (vm->name.data) {
        ngx_pfree(vm->pool, vm->name.data);
    }

    ngx_pfree(vm->pool, vm);
}


static void
ngx_wasm_vm_valtypes_init(wasm_valtype_vec_t *valtypes,
    const wasm_valkind_t **valkinds)
{
    size_t           i;
    wasm_valkind_t  *valkind;

    if (valkinds == NULL) {
        wasm_valtype_vec_new_empty(valtypes);
        return;
    }

    //valkind = ((wasm_valkind_t **) valkinds)[0];

    for (i = 0; ((wasm_valkind_t **) valkinds)[i]; i++) { /* void */ }

    wasm_valtype_vec_new_uninitialized(valtypes, i);

    for (i = 0; i < valtypes->size; i++) {
        valkind = ((wasm_valkind_t **) valkinds)[i];
        valtypes->data[i] = wasm_valtype_new(*valkind);
    }
}


ngx_int_t
ngx_wasm_vm_add_hdefs(ngx_wasm_vm_t *vm, ngx_wasm_hdefs_t *hdefs)
{
    u_char                *err = NULL;
    ngx_str_node_t        *sn;
    ngx_wasm_hdef_func_t  *hfunc_def;
    ngx_wasm_hfunc_t      *hfunc;
    wasm_valtype_vec_t     args, rets;

    hfunc_def = hdefs->funcs;

    for (/* void */; hfunc_def->ptr; hfunc_def++) {

        ngx_log_debug1(NGX_LOG_DEBUG_WASM, &vm->log, 0,
                       "wasm registering \"%V\" host function",
                       &hfunc_def->name);

        sn = ngx_wasm_sn_rbtree_lookup(&vm->hfuncs_tree, &hfunc_def->name);
        if (sn) {
            ngx_sprintf(err, "\"%V\" already defined", &hfunc_def->name);
            goto failed;
        }

        hfunc = ngx_pcalloc(vm->pool, sizeof(ngx_wasm_hfunc_t));
        if (hfunc == NULL) {
            err = (u_char *) "no memory";
            goto failed;
        }

        hfunc->name = &hfunc_def->name;
        hfunc->subsys = hdefs->subsys;
        hfunc->ptr = hfunc_def->ptr;

        ngx_wasm_sn_init(&hfunc->sn, hfunc->name);
        ngx_wasm_sn_rbtree_insert(&vm->hfuncs_tree, &hfunc->sn);

        ngx_wasm_vm_valtypes_init(&args, hfunc_def->args);
        ngx_wasm_vm_valtypes_init(&rets, hfunc_def->rets);

        hfunc->functype = wasm_functype_new(&args, &rets);
    }

    return NGX_OK;

failed:

    ngx_wasm_log_error(NGX_LOG_EMERG, &vm->log, 0,
                       "failed to register host functions: %s", err);

    return NGX_ERROR;
}


ngx_int_t
ngx_wasm_vm_add_module(ngx_wasm_vm_t *vm, ngx_str_t *name, ngx_str_t *path)
{
    u_char                *p;
    ngx_wasm_vm_module_t  *module;

    module = ngx_wasm_vm_get_module(vm, name);
    if (module) {
        return NGX_DECLINED;
    }

    /* module == NULL */

    module = ngx_pcalloc(vm->pool, sizeof(ngx_wasm_vm_module_t));
    if (module == NULL) {
        goto failed;
    }

    module->vm = vm;
    module->name.len = name->len;
    module->name.data = ngx_pnalloc(vm->pool, module->name.len + 1);
    if (module->name.data == NULL) {
        goto failed;
    }

    p = ngx_copy(module->name.data, name->data, module->name.len);
    *p = '\0';

    module->path.len = path->len;
    module->path.data = ngx_pnalloc(vm->pool, module->path.len + 1);
    if (module->path.data == NULL) {
        goto failed;
    }

    p = ngx_copy(module->path.data, path->data, module->path.len);
    *p = '\0';

    if (ngx_strncmp(&module->path.data[module->path.len - 4], ".wat", 4) == 0) {
        module->flags |= NGX_WASM_MODULE_ISWAT;
    }

    ngx_queue_init(&module->instances_queue);

    ngx_wasm_sn_init(&module->sn, &module->name);
    ngx_wasm_sn_rbtree_insert(&vm->modules_tree, &module->sn);

    ngx_log_debug4(NGX_LOG_DEBUG_WASM, vm->pool->log, 0,
                   "wasm registered \"%V\" module in \"%V\" vm"
                   " (vm: %p, module: %p)",
                   &module->name, &vm->name, vm, module);

    vm->new_module = 1;

    return NGX_OK;

failed:

    ngx_wasm_log_error(NGX_LOG_EMERG, &vm->log, 0,
                       "failed to add \"%V\" module from \"%V\"",
                       name, path);

    if (module) {
        if (module->name.data) {
            ngx_pfree(vm->pool, module->name.data);
        }

        if (module->path.data) {
            ngx_pfree(vm->pool, module->path.data);
        }

        ngx_pfree(vm->pool, module);
    }

    return NGX_ERROR;
}


ngx_wasm_vm_module_t *
ngx_wasm_vm_get_module(ngx_wasm_vm_t *vm, ngx_str_t *name)
{
    ngx_str_node_t        *sn;
    ngx_wasm_vm_module_t  *module;

    sn = ngx_wasm_sn_rbtree_lookup(&vm->modules_tree, name);
    if (sn == NULL) {
        return NULL;
    }

    module = ngx_wasm_sn_sn2data(sn, ngx_wasm_vm_module_t, sn);

    return module;
}


ngx_int_t
ngx_wasm_vm_init(ngx_wasm_vm_t *vm, ngx_wrt_t *runtime)
{
    ngx_int_t              rc = NGX_ERROR;
    ngx_rbtree_node_t     *node, *root, *sentinel;
    ngx_str_node_t        *sn;
    ngx_wasm_vm_module_t  *module;

    if (vm->runtime) {
        ngx_wasm_log_error(NGX_LOG_INFO, &vm->log, 0,
                           "runtime already initialized");

    } else if (runtime->init) {
        if (runtime->init(vm) != NGX_OK) {
            ngx_wasm_log_error(NGX_LOG_EMERG, &vm->log, 0,
                               "failed to initialize \"%V\" runtime "
                               "for \"%V\" vm", &runtime->name, &vm->name);
            goto failed;
        }

        vm->runtime = runtime;
    }

    sentinel = vm->modules_tree.sentinel;
    root = vm->modules_tree.root;

    if (root == sentinel) {
        ngx_wasm_assert(!vm->new_module);
        goto done;
    }

    for (node = ngx_rbtree_min(root, sentinel);
         node;
         node = ngx_rbtree_next(&vm->modules_tree, node))
    {
        sn = ngx_wasm_sn_n2sn(node);
        module = ngx_wasm_sn_sn2data(sn, ngx_wasm_vm_module_t, sn);

        rc = ngx_wasm_vm_load_module(vm, &module->name);
        if (rc != NGX_OK) {
            goto failed;
        }
    }

    vm->new_module = 0;

done:

    rc = NGX_OK;

failed:

    return rc;
}


ngx_int_t
ngx_wasm_vm_load_module(ngx_wasm_vm_t *vm, ngx_str_t *name)
{
    size_t                  i;
    ssize_t                 n, fsize;
    u_char                 *file_bytes = NULL;
    ngx_str_t               s;
    ngx_str_node_t         *sn;
    ngx_int_t               rc = NGX_ERROR;
    ngx_fd_t                fd;
    ngx_file_t              file;
    ngx_wasm_vm_module_t   *module;
    ngx_wrt_error_pt        err = NULL;
    wasm_byte_vec_t         wasm_bytes;
    wasm_importtype_vec_t   importtypes;
    wasm_importtype_t      *importtype;
    const wasm_name_t      *importmodule, *importname;

    ngx_wasm_vm_check_init(vm, NGX_ABORT);

    module = ngx_wasm_vm_get_module(vm, name);
    if (module == NULL) {
        ngx_wasm_log_error(NGX_LOG_EMERG, &vm->log, 0,
                           "no \"%V\" module defined", name);
        return NGX_DECLINED;
    }

    if (module->flags & NGX_WASM_MODULE_LOADED) {
        return NGX_OK;
    }

    if (!module->path.len) {
        ngx_wasm_log_error(NGX_LOG_ALERT, &vm->log, 0,
                           "NYI: module loading only supported via path");
        return NGX_ERROR;
    }

    fd = ngx_open_file(module->path.data, NGX_FILE_RDONLY, NGX_FILE_OPEN, 0);
    if (fd == NGX_INVALID_FILE) {
        ngx_wasm_log_error(NGX_LOG_EMERG, &vm->log, ngx_errno,
                           ngx_open_file_n " \"%V\" failed",
                           &module->path);
        return NGX_ERROR;
    }

    ngx_memzero(&file, sizeof(ngx_file_t));

    file.fd = fd;
    file.log = &vm->log;
    file.name.len = module->path.len;
    file.name.data = (u_char *) module->path.data;

    if (ngx_fd_info(fd, &file.info) == NGX_FILE_ERROR) {
        ngx_wasm_log_error(NGX_LOG_EMERG, &vm->log, ngx_errno,
                           ngx_fd_info_n " \"%V\" failed", &file.name);
        goto close;
    }

    fsize = ngx_file_size(&file.info);

    file_bytes = ngx_alloc(fsize, &vm->log);
    if (file_bytes == NULL) {
        ngx_wasm_log_error(NGX_LOG_EMERG, &vm->log, 0,
                           "failed to allocate file_bytes for \"%V\" "
                           "module from \"%V\"",
                           &module->name, &module->path);
        goto close;
    }

    n = ngx_read_file(&file, file_bytes, fsize, 0);
    if (n == NGX_ERROR) {
        ngx_wasm_log_error(NGX_LOG_EMERG, &vm->log, ngx_errno,
                           ngx_read_file_n " \"%V\" failed",
                           &file.name);
        goto close;
    }

    if (n != fsize) {
        ngx_wasm_log_error(NGX_LOG_EMERG, &vm->log, 0,
                           ngx_read_file_n " \"%V\" returned only "
                           "%z file_bytes instead of %uz", &file.name,
                           n, fsize);
        goto close;
    }

    ngx_memzero(&wasm_bytes, sizeof(wasm_byte_vec_t));

    if (module->flags & NGX_WASM_MODULE_ISWAT) {
        ngx_log_debug2(NGX_LOG_DEBUG_WASM, &vm->log, 0,
                       "wasm compiling \"%V\" .wat module at \"%V\"",
                       &module->name, &module->path);

        if (vm->runtime->wat2wasm(vm, file_bytes, fsize, &wasm_bytes, &err)
            != NGX_OK)
        {
            ngx_wasm_vm_log_error(NGX_LOG_EMERG, &vm->log, err, NULL,
                                  "failed to compile \"%V\" to wasm",
                                  &module->path);
            goto close;
        }

    } else {
        wasm_byte_vec_new(&wasm_bytes, fsize, (const char *) file_bytes);
    }

    ngx_wasm_log_error(NGX_LOG_NOTICE, &vm->log, 0,
                       "loading \"%V\" module from \"%V\"",
                       &module->name, &module->path);

    if (vm->runtime->module_new(module, &wasm_bytes, &err) != NGX_OK) {
        ngx_wasm_vm_log_error(NGX_LOG_EMERG, &vm->log, err, NULL,
                              "failed to load \"%V\" module from \"%V\"",
                              &module->name, &module->path);
        wasm_byte_vec_delete(&wasm_bytes);
        goto close;
    }

    wasm_module_imports(module->module, &importtypes);
    wasm_module_exports(module->module, &module->exports);

    module->nhfuncs = 0;

    if (importtypes.size) {
        module->hfuncs = ngx_palloc(vm->pool, importtypes.size
                                              * sizeof(ngx_wasm_hfunc_t *));
        if (module->hfuncs == NULL) {
            goto failed;
        }

        for (i = 0; i < importtypes.size; i++) {
            importtype = ((wasm_importtype_t **) importtypes.data)[i];
            importmodule = wasm_importtype_module(importtype);
            importname = wasm_importtype_name(importtype);

            ngx_log_debug7(NGX_LOG_DEBUG_WASM, &vm->log, 0,
                           "wasm \"%V\" module import \"%*s.\"%*s\" (%u/%u)",
                           &module->name,
                           importmodule->size, importmodule->data,
                           importname->size, importname->data,
                           i, importtypes.size);

            if (ngx_strncmp(importmodule->data, "env", 3) != 0) {
                continue;
            }

            ngx_wasm_log_error(NGX_LOG_INFO, &vm->log, 0,
                               "loading \"%V\" module \"env.\"%*s\" import",
                               &module->name, importname->size, importname->data);

            switch (wasm_externtype_kind(
                    wasm_importtype_type(importtype))) {

            case WASM_EXTERN_FUNC:
                s.data = (u_char *) importname->data;
                s.len = importname->size;

                sn = ngx_wasm_sn_rbtree_lookup(&vm->hfuncs_tree, &s);
                if (!sn) {
                    /* will fail instantiation */
                    ngx_wasm_log_error(NGX_LOG_ERR, &vm->log, 0,
                                       "no \"%*s\" host function defined",
                                       importname->size, importname->data);
                    continue;
                }

                module->hfuncs[module->nhfuncs] =
                    ngx_wasm_sn_sn2data(sn, ngx_wasm_hfunc_t, sn);
                module->nhfuncs++;
                break;

            default:
                ngx_wasm_log_error(NGX_LOG_ALERT, &vm->log, 0,
                                   "NYI: module import type not supported");
                ngx_wasm_assert(0);
                goto failed;

            }
        }
    }

    ngx_wasm_assert(module->nhfuncs == importtypes.size);

    module->flags |= NGX_WASM_MODULE_LOADED;

    ngx_log_debug4(NGX_LOG_DEBUG_WASM, &vm->log, 0,
                   "wasm loaded \"%V\" module in \"%V\" vm"
                   " (vm: %p, module: %p)",
                   &module->name, &vm->name, vm, module);

    rc = NGX_OK;

failed:

    wasm_importtype_vec_delete(&importtypes);

    wasm_byte_vec_delete(&wasm_bytes);

close:

    if (ngx_close_file(fd) == NGX_FILE_ERROR) {
        ngx_wasm_log_error(NGX_LOG_ERR, &vm->log, ngx_errno,
                           ngx_close_file_n " \"%V\" failed", &file.name);
    }

    if (file_bytes) {
        ngx_free(file_bytes);
    }

    return rc;
}


static wasm_trap_t *
ngx_wasm_vm_hfuncs_trampoline(void *env, const wasm_val_t args[],
    wasm_val_t rets[])
{
    size_t                   traplen, len;
    char                    *trap = NULL;
    u_char                  *p, *buf, trapmsg[NGX_WASM_MAX_HOST_TRAP_STR];
    ngx_int_t                rc;
    ngx_wasm_hfunc_tctx_t   *tctx = env;
    ngx_wasm_hfunc_t        *hfunc = tctx->hfunc;
    ngx_wasm_vm_instance_t  *instance = tctx->instance;
    ngx_wasm_hctx_t         *hctx = instance->hctx;
    wasm_name_t              trapmsg_ret;
    wasm_trap_t             *trap_ret = NULL;

    ngx_log_debug2(NGX_LOG_DEBUG_WASM, hctx->log, 0,
                   "wasm host function trampoline (hfunc: %V, tctx: %p)",
                   hfunc->name, tctx);

    if (hfunc->subsys != NGX_WASM_HOST_SUBSYS_ANY &&
        hfunc->subsys != hctx->subsys)
    {
        trap = "bad subsystem";
        goto trap;
    }

    hctx->trapmsglen = 0;
    hctx->trapmsg = (u_char *) &trapmsg;
    hctx->mem_offset = (u_char *) wasm_memory_data(instance->memory);

    rc = hfunc->ptr(hctx, args, rets);

    ngx_log_debug1(NGX_LOG_DEBUG_WASM, hctx->log, 0,
                   "wasm host function trampoline rc: %d", rc);

    switch (rc) {

    case NGX_WASM_OK:
        break;

    case NGX_WASM_ERROR:
        trap = "nginx host error";
        goto trap;

    case NGX_WASM_BAD_CTX:
        trap = "bad context";
        goto trap;

    case NGX_WASM_BAD_USAGE:
        trap = "bad usage";
        goto trap;

    default:
        trap = "NYI - host function rc";
        goto trap;

    }

    return NULL;

trap:

    if (hctx->trapmsglen) {
        /* hfunc trap msg */

        traplen = ngx_strlen(trap);

        len = traplen + hctx->trapmsglen + 3;

        buf = ngx_alloc(len, hctx->log);
        if (buf != NULL) {
            p = ngx_copy(buf, trap, traplen);

            p = ngx_snprintf(p, len - (p - buf), ": %*s",
                             hctx->trapmsglen, hctx->trapmsg);

            *p++ = '\0';

            wasm_name_new(&trapmsg_ret, len, (const char *) buf);

            trap_ret = wasm_trap_new(instance->store, &trapmsg_ret);

            wasm_name_delete(&trapmsg_ret);
            ngx_free(buf);
        }

    } else {
        wasm_name_new_from_string(&trapmsg_ret, trap);
        trap_ret = wasm_trap_new(instance->store, &trapmsg_ret);
        wasm_name_delete(&trapmsg_ret);
    }

    return trap_ret;
}


ngx_wasm_vm_instance_t *
ngx_wasm_vm_instance_new(ngx_wasm_vm_t *vm, ngx_str_t *mod_name)
{
    off_t                    off;
    size_t                   i;
    ngx_wasm_vm_instance_t  *instance;
    ngx_wasm_vm_module_t    *module;
    ngx_wasm_hfunc_t        *hfunc;
    ngx_wasm_hfunc_tctx_t   *tctx;
    wasm_func_t             *func;
    wasm_extern_t           *export;
    wasm_trap_t             *trap = NULL;
    ngx_wrt_error_pt         err = NULL;

    ngx_wasm_vm_check_init(vm, NULL);

    module = ngx_wasm_vm_get_module(vm, mod_name);
    if (module == NULL) {
        ngx_wasm_log_error(NGX_LOG_ERR, &vm->log, 0,
                           "failed to create instance of \"%V\" module: "
                           "no such module defined", mod_name);
        return NULL;
    }

    if (!(module->flags & NGX_WASM_MODULE_LOADED)) {
        ngx_wasm_log_error(NGX_LOG_ERR, &vm->log, 0,
                           "failed to create instance of \"%V\" module: "
                           "module not loaded", mod_name);
        return NULL;
    }

    instance = ngx_pcalloc(vm->pool, sizeof(struct ngx_wasm_vm_instance_s));
    if (instance == NULL) {
        ngx_wasm_log_error(NGX_LOG_ERR, &vm->log, 0,
                           "failed to create instance of \"%V\" module: "
                           "could not allocate memory", mod_name);
        return NULL;
    }

    instance->vm = vm;
    instance->hctx = NULL;
    instance->pool = vm->pool;
    instance->module = module;
    instance->log_ctx.vm = vm;
    instance->log_ctx.instance = instance;
    instance->log_ctx.orig_log = &vm->log;

    instance->log.file = vm->log.file;
    instance->log.next = vm->log.next;
    instance->log.wdata = vm->log.wdata;
    instance->log.writer = vm->log.writer;
    instance->log.log_level = vm->log.log_level;
    instance->log.handler = ngx_wasm_vm_log_error_handler;
    instance->log.data = &instance->log_ctx;

    instance->store = wasm_store_new(vm->engine);
    if (instance->store == NULL) {
        goto failed;
    }

    /* imports (hfuncs) */

    wasm_extern_vec_new_uninitialized(&instance->env, module->nhfuncs);

    if (module->nhfuncs) {
        //instance->tctxs = ngx_palloc(vm->pool, module->nhfuncs *
        //                                       sizeof(ngx_wasm_hfunc_tctx_t));
        instance->tctxs = ngx_pmemalign(vm->pool, module->nhfuncs *
                                        sizeof(ngx_wasm_hfunc_tctx_t),
                                        NGX_ALIGNMENT);
        if (instance->tctxs == NULL) {
            goto failed;
        }

        off = sizeof(ngx_wasm_hfunc_tctx_t) / NGX_ALIGNMENT;

        for (i = 0; i < module->nhfuncs; i++) {
            hfunc = ((ngx_wasm_hfunc_t **) module->hfuncs)[i];
            tctx = (ngx_wasm_hfunc_tctx_t *) (instance->tctxs + i * off);
            //tctx = (ngx_wasm_hfunc_tctx_t *) ngx_align_ptr(tctx, NGX_ALIGNMENT);

            tctx->hfunc = hfunc;
            tctx->instance = instance;

            func = wasm_func_new_with_env(instance->store, hfunc->functype,
                                          &ngx_wasm_vm_hfuncs_trampoline,
                                          tctx, NULL);

            instance->env.data[i] = wasm_func_as_extern(func);
        }
    }

    /* instantiate */

    ngx_log_debug5(NGX_LOG_DEBUG_WASM, &vm->log, 0,
                   "wasm creating instance of \"%V\" module in \"%V\" vm"
                   " (vm: %p, module: %p, instance: %p)",
                   &module->name, &vm->name, vm, module, instance);

    if (vm->runtime->instance_new(instance, &trap, &err)) {
        ngx_wasm_vm_log_error(NGX_LOG_ERR, &vm->log, err, trap,
                              "failed to create instance of \"%V\" module",
                              mod_name);
        goto failed;
    }

    /* exports */

    wasm_instance_exports(instance->instance, &instance->exports);

    for (i = 0; i < instance->exports.size; i++) {
        export = ((wasm_extern_t **) instance->exports.data)[i];

        if (wasm_extern_kind(export) == WASM_EXTERN_MEMORY) {
            instance->memory = wasm_extern_as_memory(export);
        }
    }

    ngx_queue_insert_tail(&module->instances_queue, &instance->queue);

    return instance;

failed:

    ngx_wasm_vm_instance_free(instance);

    return NULL;
}


ngx_inline void
ngx_wasm_vm_instance_set_hctx(ngx_wasm_vm_instance_t *instance,
    ngx_wasm_hctx_t *hctx)
{
    ngx_wasm_assert(hctx->log);
    ngx_wasm_assert(hctx->pool);
    /* subsys + data */

    ((ngx_wasm_vm_log_ctx_t *) instance->log.data)->orig_log = hctx->log;

    /* hctx init */

    hctx->log = &instance->log;
    instance->hctx = hctx;
}


ngx_int_t
ngx_wasm_vm_instance_call(ngx_wasm_vm_instance_t *instance,
    ngx_str_t *func_name)
{
    size_t              i;
    ngx_int_t           rc;
    ngx_wasm_vm_t      *vm = instance->vm;
    wasm_extern_t      *export;
    wasm_exporttype_t  *exporttype;
    const wasm_name_t  *name;
    wasm_func_t        *func = NULL;
    wasm_trap_t        *trap = NULL;
    ngx_wrt_error_pt    err = NULL;

    ngx_wasm_vm_check_init(vm, NGX_ABORT);

    for (i = 0; i < instance->exports.size; i++) {
        export = ((wasm_extern_t **) instance->exports.data)[i];

        if (wasm_extern_kind(export) != WASM_EXTERN_FUNC) {
            continue;
        }

        exporttype = ((wasm_exporttype_t **) instance->module->exports.data)[i];
        name = wasm_exporttype_name(exporttype);

        if (ngx_strncmp(name->data, func_name->data, name->size) == 0) {
            func = wasm_extern_as_func(export);
            break;
        }

    }

    if (func == NULL) {
        ngx_wasm_vm_log_error(NGX_LOG_ERR, &instance->log, NULL, NULL,
                              "no \"%V\" function defined in \"%V\" module",
                              func_name, &instance->module->name);
        return NGX_DECLINED;
    }

    rc = vm->runtime->func_call(func, &trap, &err);
    if (err || trap) {
        ngx_wasm_vm_log_error(NGX_LOG_ERR, &instance->log, err, trap, NULL);
    }

    return rc;
}


void
ngx_wasm_vm_instance_free(ngx_wasm_vm_instance_t *instance)
{
    ngx_log_debug5(NGX_LOG_DEBUG_WASM, &instance->vm->log, 0,
                   "wasm free instance of \"%V\" module in \"%V\" vm"
                   " (vm: %p, module: %p, instance: %p)",
                   &instance->module->name, &instance->vm->name,
                   instance->vm, instance->module, instance);

    if (instance->store) {
        wasm_store_delete(instance->store);
    }

    if (instance->instance) {
        wasm_instance_delete(instance->instance);
        wasm_extern_vec_delete(&instance->exports);
        wasm_extern_vec_delete(&instance->env);
    }

    if (instance->tctxs) {
       ngx_pfree(instance->pool, instance->tctxs);
    }

    if (instance->queue.next) {
        ngx_queue_remove(&instance->queue);
    }

    ngx_pfree(instance->pool, instance);
}


static void
ngx_wasm_vm_log_error(ngx_uint_t level, ngx_log_t *log,
    ngx_wrt_error_pt err, wasm_trap_t *trap,
#if (NGX_HAVE_VARIADIC_MACROS)
    const char *fmt, ...)

#else
    const char *fmt, va_list args)
#endif
{
#if (NGX_HAVE_VARIADIC_MACROS)
    va_list                 args;
#endif
    u_char                 *p, *last, errstr[NGX_MAX_ERROR_STR];
    ngx_wasm_vm_t          *vm;
    ngx_wasm_vm_log_ctx_t  *ctx;
    wasm_message_t          trapmsg;

    ctx = log->data;
    vm = ctx->vm;

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

    if (vm->runtime == NULL) {
        p = ngx_snprintf(p, last - p, " (\"%V\" vm not initialized)",
                         &vm->name);

    } else {
        if (trap) {
            wasm_trap_message(trap, &trapmsg);

            p = ngx_slprintf(p, last, "%*s", trapmsg.size, trapmsg.data);

            wasm_byte_vec_delete(&trapmsg);
        }

        if (err && vm->runtime->error_log_handler) {
            p = vm->runtime->error_log_handler(err, p, last - p);
        }
    }

    if (trap) {
        wasm_trap_delete(trap);
    }

    ngx_wasm_log_error(level, log, 0, "%*s", p - errstr, errstr);
}


static u_char *
ngx_wasm_vm_log_error_handler(ngx_log_t *log, u_char *buf, size_t len)
{
    u_char                 *p = buf;
    ngx_log_t              *orig_log;
    ngx_wasm_vm_t          *vm;
    ngx_wasm_vm_log_ctx_t  *ctx;

    ctx = log->data;
    vm = ctx->vm;
    orig_log = ctx->orig_log;

    if (vm->runtime == NULL) {
        p = ngx_snprintf(buf, len, " <vm: %V, runtime: ?>",
                         &vm->name);

    } else {
        p = ngx_snprintf(buf, len, " <vm: %V, runtime: %V>",
                         &vm->name,
                         &vm->runtime->name);
    }

    len -= p - buf;
    buf = p;

    if (orig_log->handler) {
        p = orig_log->handler(orig_log, buf, len);
    }

    return p;
}
