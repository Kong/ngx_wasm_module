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
    if (vm->wrt == NULL) {                                                   \
        ngx_wasm_log_error(NGX_LOG_EMERG, vm->log, 0,                        \
                           "\"%V\" vm not initialized (vm: %p)",             \
                           &vm->name, vm);                                   \
        ngx_wasm_assert(0);                                                  \
        return ret;                                                          \
    }                                                                        \


static void ngx_wasm_vm_log_error(ngx_uint_t level, ngx_log_t *log,
    ngx_wrt_error_pt err, ngx_wrt_trap_pt trap,
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

    vm->log = ngx_pcalloc(vm->pool, sizeof(ngx_log_t));
    if (vm->log == NULL) {
        goto failed;
    }

    vm->log->file = cycle->new_log.file;
    vm->log->next = cycle->new_log.next;
    vm->log->writer = cycle->new_log.writer;
    vm->log->wdata = cycle->new_log.wdata;
    vm->log->log_level = cycle->new_log.log_level;
    vm->log->handler = ngx_wasm_vm_log_error_handler;
    vm->log->data = &vm->log_ctx;

    ngx_rbtree_init(&vm->modules_tree, &vm->modules_sentinel,
                    ngx_str_rbtree_insert_value);

    ngx_rbtree_init(&vm->hfuncs_tree, &vm->hfuncs_sentinel,
                    ngx_str_rbtree_insert_value);

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
                       &module->name, &vm->name,
                       vm, module);

        while (!ngx_queue_empty(&module->instances_queue)) {
            q = ngx_queue_head(&module->instances_queue);

            inst = ngx_queue_data(q, ngx_wasm_vm_instance_t, queue);
            ngx_wasm_vm_instance_free(inst);
        }

        vm->runtime->module_free(module->wrt);

        ngx_pfree(vm->pool, module->name.data);
        ngx_pfree(vm->pool, module->path.data);
        ngx_pfree(vm->pool, module->bytes.data);
        ngx_pfree(vm->pool, module);
    } 

    root = &vm->hfuncs_tree.root;
    sentinel = &vm->hfuncs_tree.sentinel;

    while (*root != *sentinel) {
        node = ngx_rbtree_min(*root, *sentinel);
        sn = ngx_wasm_sn_n2sn(node);
        hfunc = ngx_wasm_sn_sn2data(sn, ngx_wasm_hfunc_t, sn);

        ngx_log_debug1(NGX_LOG_DEBUG_WASM, vm->log, 0,
                       "wasm free \"env.%V\" host function",
                       hfunc->name);

        ngx_rbtree_delete(&vm->hfuncs_tree, node);

        if (vm->runtime->hfunctype_free && hfunc->wrt_functype) {
           vm->runtime->hfunctype_free(hfunc->wrt_functype);
        }

        ngx_pfree(vm->pool, hfunc);
    }

    if (vm->runtime
        && vm->runtime->engine_free
        && vm->wrt)
    {
        vm->runtime->engine_free(vm->wrt);
    }

    if (vm->name.data) {
        ngx_pfree(vm->pool, vm->name.data);
    }

    if (vm->log) {
        ngx_pfree(vm->pool, vm->log);
    }

    ngx_pfree(vm->pool, vm);
}


static ngx_wasm_vm_module_t *
ngx_wasm_vm_get_module(ngx_wasm_vm_t *vm, ngx_str_t *mod_name)
{
    ngx_str_node_t        *sn;
    ngx_wasm_vm_module_t  *module;

    sn = ngx_wasm_sn_rbtree_lookup(&vm->modules_tree, mod_name);
    if (sn == NULL) {
        return NULL;
    }

    module = ngx_wasm_sn_sn2data(sn, ngx_wasm_vm_module_t, sn);

    return module;
}


ngx_int_t
ngx_wasm_vm_add_module(ngx_wasm_vm_t *vm, ngx_str_t *mod_name, ngx_str_t *path)
{
    u_char                *p;
    ngx_wasm_vm_module_t  *module;

    module = ngx_wasm_vm_get_module(vm, mod_name);
    if (module) {
        return NGX_DECLINED;
    }

    /* module == NULL */

    module = ngx_pcalloc(vm->pool, sizeof(ngx_wasm_vm_module_t));
    if (module == NULL) {
        goto failed;
    }

    module->name.len = mod_name->len;
    module->name.data = ngx_pnalloc(vm->pool, module->name.len + 1);
    if (module->name.data == NULL) {
        goto failed;
    }

    p = ngx_copy(module->name.data, mod_name->data, module->name.len);
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

    ngx_wasm_sn_init(&module->sn, &module->name);
    ngx_wasm_sn_rbtree_insert(&vm->modules_tree, &module->sn);

    ngx_log_debug4(NGX_LOG_DEBUG_WASM, vm->log, 0,
                   "wasm registered \"%V\" module in \"%V\" vm"
                   " (vm: %p, module: %p)",
                   &module->name, &vm->name,
                   vm, module);

    return NGX_OK;

failed:

    ngx_wasm_log_error(NGX_LOG_EMERG, vm->log, 0,
                       "failed to add \"%V\" module from \"%V\"",
                       mod_name, path);

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


ngx_uint_t
ngx_wasm_vm_has_module(ngx_wasm_vm_t *vm, ngx_str_t *mod_name)
{
    return ngx_wasm_vm_get_module(vm, mod_name) != NULL;
}


ngx_int_t
ngx_wasm_vm_add_hdefs(ngx_wasm_vm_t *vm, ngx_wasm_hdefs_t *hdefs)
{
    size_t                 nargs, nrets;
    u_char                *err = NULL;
    ngx_str_node_t        *sn;
    ngx_wasm_hdef_func_t  *hfunc_def;
    ngx_wasm_hfunc_t      *hfunc;

    hfunc_def = hdefs->funcs;

    for (/* void */; hfunc_def->ptr; hfunc_def++) {

        ngx_log_debug1(NGX_LOG_DEBUG_WASM, vm->log, 0,
                       "wasm registering \"env.%V\" host function",
                       &hfunc_def->name);

        sn = ngx_wasm_sn_rbtree_lookup(&vm->hfuncs_tree, &hfunc_def->name);
        if (sn) {
            ngx_sprintf(err, "\"env.%V\" already defined", &hfunc_def->name);
            goto failed;
        }

        hfunc = ngx_pcalloc(vm->pool, sizeof(ngx_wasm_hfunc_t));
        if (hfunc == NULL) {
            err = (u_char *) "no memory";
            goto failed;
        }

        hfunc->name = &hfunc_def->name;
        hfunc->ptr = hfunc_def->ptr;
        hfunc->subsys = hdefs->subsys;

        for (nargs = 0; hfunc_def->args[nargs] && nargs < NGX_WASM_ARGS_MAX;
             nargs++) { /* void */ }

        for (nrets = 0; hfunc_def->rets[nrets] && nrets < NGX_WASM_RETS_MAX;
             nrets++) { /* void */ }

        hfunc->args.size = nargs;
        hfunc->args.vals = ngx_palloc(vm->pool,
                                      nargs * sizeof(ngx_wasm_val_kind));
        if (hfunc->args.vals == NULL) {
            err = (u_char *) "no memory";
            goto failed;
        }

        hfunc->rets.size = nrets;
        hfunc->rets.vals = ngx_palloc(vm->pool,
                                      nrets * sizeof(ngx_wasm_val_kind));
        if (hfunc->rets.vals == NULL) {
            err = (u_char *) "no memory";
            goto failed;
        }

        for (nargs = 0; nargs < hfunc->args.size; nargs++) {
            hfunc->args.vals[nargs] = hfunc_def->args[nargs];
        }

        for (nrets = 0; nrets < hfunc->rets.size; nrets++) {
            hfunc->rets.vals[nrets] = hfunc_def->rets[nrets];
        }

        ngx_wasm_sn_init(&hfunc->sn, hfunc->name);
        ngx_wasm_sn_rbtree_insert(&vm->hfuncs_tree, &hfunc->sn);
    }

    return NGX_OK;

failed:

    ngx_wasm_log_error(NGX_LOG_EMERG, vm->log, 0,
                       "failed to register host functions: %s", err);

    return NGX_ERROR;
}


ngx_int_t
ngx_wasm_vm_init(ngx_wasm_vm_t *vm, ngx_wrt_t *runtime)
{
    ngx_int_t              rc;
    ngx_rbtree_node_t     *node, *root, *sentinel;
    ngx_str_node_t        *sn;
    ngx_wasm_vm_module_t  *module;
    ngx_wasm_hfunc_t      *hfunc;

    if (vm->wrt) {
        return NGX_DONE;
    }

    vm->runtime = runtime;

    if (runtime->engine_new) {
        vm->wrt = runtime->engine_new(vm->pool);
        if (vm->wrt == NULL) {
            ngx_wasm_log_error(NGX_LOG_EMERG, vm->log, 0,
                               "failed to initialize \"%V\" runtime "
                               "for \"%V\" vm", &vm->runtime->name, &vm->name);
            return NGX_ERROR;
        }
    }

    sentinel = vm->modules_tree.sentinel;
    root = vm->modules_tree.root;

    if (root == sentinel) {
        return NGX_OK;
    }

    for (node = ngx_rbtree_min(root, sentinel);
         node;
         node = ngx_rbtree_next(&vm->modules_tree, node))
    {
        sn = ngx_wasm_sn_n2sn(node);
        module = ngx_wasm_sn_sn2data(sn, ngx_wasm_vm_module_t, sn);

        rc = ngx_wasm_vm_load_module(vm, &module->name);
        if (rc != NGX_OK) {
            return rc;
        }
    }

    root = vm->hfuncs_tree.root;
    sentinel = vm->hfuncs_tree.sentinel;

    if (root == sentinel) {
        return NGX_OK;
    }

    for (node = ngx_rbtree_min(root, sentinel);
         node;
         node = ngx_rbtree_next(&vm->hfuncs_tree, node))
    {
        sn = ngx_wasm_sn_n2sn(node);
        hfunc = ngx_wasm_sn_sn2data(sn, ngx_wasm_hfunc_t, sn);

        hfunc->wrt_functype = vm->runtime->hfunctype_new(hfunc);
    }

    return NGX_OK;
}


ngx_int_t
ngx_wasm_vm_load_module(ngx_wasm_vm_t *vm, ngx_str_t *mod_name)
{
    ssize_t                n, fsize;
    u_char                *file_bytes;
    ngx_str_t              wasm_bytes;
    ngx_int_t              rc = NGX_ERROR;
    ngx_fd_t               fd;
    ngx_file_t             file;
    ngx_wasm_vm_module_t  *module;
    ngx_wrt_error_pt       err = NULL;

    ngx_wasm_vm_check_init(vm, NGX_ABORT);

    ngx_memzero(&wasm_bytes, sizeof(ngx_str_t));

    module = ngx_wasm_vm_get_module(vm, mod_name);
    if (module == NULL) {
        ngx_wasm_log_error(NGX_LOG_EMERG, vm->log, 0,
                           "no \"%V\" module defined", mod_name);
        return NGX_DECLINED;
    }

    if (module->flags & NGX_WASM_MODULE_LOADED) {
        return NGX_OK;
    }

    if (!module->path.len) {
        ngx_wasm_log_error(NGX_LOG_ALERT, vm->log, 0,
                           "NYI: module loading only supported via path");
        return NGX_ERROR;
    }

    fd = ngx_open_file(module->path.data, NGX_FILE_RDONLY, NGX_FILE_OPEN, 0);
    if (fd == NGX_INVALID_FILE) {
        ngx_wasm_log_error(NGX_LOG_EMERG, vm->log, ngx_errno,
                           ngx_open_file_n " \"%V\" failed",
                           &module->path);
        return NGX_ERROR;
    }

    ngx_memzero(&file, sizeof(ngx_file_t));

    file.fd = fd;
    file.log = vm->log;
    file.name.len = module->path.len;
    file.name.data = (u_char *) module->path.data;

    if (ngx_fd_info(fd, &file.info) == NGX_FILE_ERROR) {
        ngx_wasm_log_error(NGX_LOG_EMERG, vm->log, ngx_errno,
                           ngx_fd_info_n " \"%V\" failed", &file.name);
        goto close;
    }

    fsize = ngx_file_size(&file.info);

    file_bytes = ngx_alloc(fsize, vm->log);
    if (file_bytes == NULL) {
        ngx_wasm_log_error(NGX_LOG_EMERG, vm->log, 0,
                           "failed to allocate file_bytes for \"%V\" "
                           "module from \"%V\"",
                           &module->name, &module->path);
        goto close;
    }

    n = ngx_read_file(&file, file_bytes, fsize, 0);
    if (n == NGX_ERROR) {
        ngx_wasm_log_error(NGX_LOG_EMERG, vm->log, ngx_errno,
                           ngx_read_file_n " \"%V\" failed",
                           &file.name);
        goto close;
    }

    if (n != fsize) {
        ngx_wasm_log_error(NGX_LOG_EMERG, vm->log, 0,
                           ngx_read_file_n " \"%V\" returned only "
                           "%z file_bytes instead of %uz", &file.name,
                           n, fsize);
        goto close;
    }

    if (module->flags & NGX_WASM_MODULE_ISWAT) {
        ngx_log_debug2(NGX_LOG_DEBUG_WASM, vm->log, 0,
                       "wasm compiling \"%V\" .wat module at \"%V\"",
                       &module->name, &module->path);

        err = vm->runtime->wat2wasm(vm->wrt, file_bytes, fsize, &wasm_bytes);

        ngx_free(file_bytes);

        if (wasm_bytes.data == NULL) {
            ngx_wasm_vm_log_error(NGX_LOG_EMERG, vm->log, err, NULL,
                                  "failed to compile \"%V\" to wasm",
                                  &module->path);
            goto close;
        }

    } else {
        wasm_bytes.len = fsize;
        wasm_bytes.data = file_bytes;
    }

    ngx_wasm_log_error(NGX_LOG_NOTICE, vm->log, 0,
                       "loading \"%V\" module from \"%V\"",
                       &module->name, &module->path);

    module->wrt = vm->runtime->module_new(vm->wrt, &vm->hfuncs_tree,
                                          &module->name, &wasm_bytes, &err);
    if (module->wrt == NULL) {
        ngx_wasm_vm_log_error(NGX_LOG_EMERG, vm->log, err, NULL,
                              "failed to load \"%V\" module from \"%V\"",
                              &module->name, &module->path);
        goto close;
    }

    ngx_queue_init(&module->instances_queue);

    module->flags |= NGX_WASM_MODULE_LOADED;

    ngx_log_debug4(NGX_LOG_DEBUG_WASM, vm->log, 0,
                   "wasm loaded \"%V\" module in \"%V\" vm"
                   " (vm: %p, module: %p)",
                   &module->name, &vm->name,
                   vm, module);

    rc = NGX_OK;

close:

    if (ngx_close_file(fd) == NGX_FILE_ERROR) {
        ngx_wasm_log_error(NGX_LOG_ERR, vm->log, ngx_errno,
                           ngx_close_file_n " \"%V\" failed", &file.name);
    }

    if (wasm_bytes.data) {
        ngx_free(wasm_bytes.data);
    }

    return rc;
}


ngx_wasm_vm_instance_t *
ngx_wasm_vm_instance_new(ngx_wasm_vm_t *vm, ngx_str_t *mod_name)
{
    ngx_wasm_vm_instance_t  *instance;
    ngx_wasm_vm_module_t    *module;
    ngx_wrt_error_pt         err = NULL;
    ngx_wrt_trap_pt          trap = NULL;

    ngx_wasm_vm_check_init(vm, NULL);

    module = ngx_wasm_vm_get_module(vm, mod_name);
    if (module == NULL) {
        ngx_wasm_log_error(NGX_LOG_ERR, vm->log, 0,
                           "failed to create instance of \"%V\" module: "
                           "no such module defined", mod_name);
        return NULL;
    }

    instance = ngx_pcalloc(vm->pool, sizeof(struct ngx_wasm_vm_instance_s));
    if (instance == NULL) {
        ngx_wasm_log_error(NGX_LOG_ERR, vm->log, 0,
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
    instance->log_ctx.orig_log = vm->log;

    instance->log = ngx_pcalloc(instance->pool, sizeof(ngx_log_t));
    if (instance->log == NULL) {
        goto failed;
    }

    instance->log->file = vm->log->file;
    instance->log->next = vm->log->next;
    instance->log->wdata = vm->log->wdata;
    instance->log->writer = vm->log->writer;
    instance->log->log_level = vm->log->log_level;
    instance->log->handler = ngx_wasm_vm_log_error_handler;
    instance->log->data = &instance->log_ctx;

    ngx_log_debug5(NGX_LOG_DEBUG_WASM, vm->log, 0,
                   "wasm creating instance of \"%V\" module in \"%V\" vm"
                   " (vm: %p, module: %p, instance: %p)",
                   &module->name, &vm->name,
                   vm, module, instance);

    instance->wrt = vm->runtime->instance_new(module->wrt, &instance->hctx,
                                              &err, &trap);
    if (instance->wrt == NULL) {
        ngx_wasm_vm_log_error(NGX_LOG_ERR, vm->log, err, trap,
                              "failed to create instance of \"%V\" module",
                              mod_name);
        goto failed;
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
    ngx_wasm_vm_log_ctx_t  *log_ctx;

    ngx_wasm_assert(hctx->pool);
    ngx_wasm_assert(hctx->log);

    log_ctx = (ngx_wasm_vm_log_ctx_t *) instance->log->data;
    log_ctx->orig_log = hctx->log;
    hctx->log = instance->log;

    instance->hctx = hctx;
}


ngx_int_t
ngx_wasm_vm_instance_call(ngx_wasm_vm_instance_t *instance,
    ngx_str_t *func_name)
{
    ngx_int_t          rc;
    ngx_wasm_vm_t     *vm = instance->vm;
    ngx_wrt_error_pt   err = NULL;
    ngx_wrt_trap_pt    trap = NULL;

    ngx_wasm_vm_check_init(vm, NGX_ABORT);

    rc = vm->runtime->instance_call(instance->wrt, func_name, &err, &trap);
    if (err || trap) {
        ngx_wasm_vm_log_error(NGX_LOG_ERR, instance->log, err, trap, NULL);

    } else if (rc == NGX_DECLINED) {
        ngx_wasm_vm_log_error(NGX_LOG_ERR, instance->log, NULL, NULL,
                              "no \"%V\" function defined in \"%V\" module",
                              func_name, &instance->module->name);
    }

    return rc;
}


void
ngx_wasm_vm_instance_free(ngx_wasm_vm_instance_t *instance)
{
    ngx_log_debug5(NGX_LOG_DEBUG_WASM, instance->vm->log, 0,
                   "wasm free instance of \"%V\" module in \"%V\" vm"
                   " (vm: %p, module: %p, instance: %p)",
                   &instance->module->name, &instance->vm->name,
                   instance->vm, instance->module, instance);

    if (instance->wrt) {
        instance->vm->runtime->instance_free(instance->wrt);
    }

    if (instance->log) {
        ngx_pfree(instance->pool, instance->log);
    }

    if (instance->queue.next) {
        ngx_queue_remove(&instance->queue);
    }

    ngx_pfree(instance->pool, instance);
}


static void
ngx_wasm_vm_log_error(ngx_uint_t level, ngx_log_t *log,
    ngx_wrt_error_pt err, ngx_wrt_trap_pt trap,
#if (NGX_HAVE_VARIADIC_MACROS)
    const char *fmt, ...)

#else
    const char *fmt, va_list args)
#endif
{
#if (NGX_HAVE_VARIADIC_MACROS)
    va_list                 args;
#endif
    u_char                 *p, *last;
    u_char                  errstr[NGX_MAX_ERROR_STR];
    ngx_wasm_vm_t          *vm;
    ngx_wasm_vm_log_ctx_t  *ctx;

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

    } else if (err || trap) {
        p = vm->runtime->trap_log_handler(err, trap, p, last - p);
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
