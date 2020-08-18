/*
 * Copyright (C) Thibault Charbonnier
 */

#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_wasm_vm.h>
#include <ngx_wasm_hfuncs.h>
#include <ngx_wasm_util.h>


#define ngx_wasm_vm_check_init(vm, ret)                                      \
    if (vm->runtime == NULL) {                                               \
        ngx_wasm_log_error(NGX_LOG_EMERG, vm->log, 0,                        \
                           "\"%V\" vm not initialized (vm: %p)",             \
                           &vm->name, vm);                                   \
        ngx_wasm_assert(0);                                                  \
        return ret;                                                          \
    }                                                                        \


typedef struct ngx_wasm_vm_log_ctx_s {
    ngx_wasm_vm_t                 *vm;
    ngx_wasm_vm_instance_t        *instance;
    ngx_log_t                     *orig_log;
} ngx_wasm_vm_log_ctx_t;


struct ngx_wasm_vm_instance_s {
    ngx_pool_t                    *pool;
    ngx_log_t                     *log;
    ngx_wasm_vm_t                 *vm;
    ngx_wasm_vm_module_t          *module;
    ngx_queue_t                    queue;
    ngx_wasm_vm_log_ctx_t          log_ctx;
    ngx_wasm_hctx_t                hctx;
    ngx_wrt_instance_pt            wrt;
};


struct ngx_wasm_vm_module_s {
    ngx_wasm_rbtree_named_node_t   rbnode;
    ngx_uint_t                     flags;
    ngx_str_t                      bytes;
    ngx_str_t                      name;
    ngx_str_t                      path;
    ngx_queue_t                    instances_queue;
    ngx_wrt_module_pt              wrt;
};


typedef struct ngx_wasm_vm_modules_s {
    ngx_rbtree_t                   rbtree;
    ngx_rbtree_node_t              sentinel;
} ngx_wasm_vm_modules_t;


struct ngx_wasm_vm_s {
    ngx_str_t                      name;
    ngx_pool_t                    *pool;
    ngx_log_t                     *log;
    ngx_wasm_vm_log_ctx_t          log_ctx;
    ngx_wasm_vm_modules_t          modules;
    ngx_wrt_t                     *runtime;
    ngx_wrt_engine_pt              wrt;
};


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

    ngx_rbtree_init(&vm->modules.rbtree, &vm->modules.sentinel,
                    ngx_wasm_rbtree_insert_named_node);

    ngx_log_debug2(NGX_LOG_DEBUG_WASM, cycle->log, 0,
                   "[wasm] created \"%V\" vm (vm: %p)",
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
    ngx_rbtree_node_t       **root, **sentinel;
    ngx_wasm_vm_module_t     *module;
    ngx_wasm_vm_instance_t   *inst;

    ngx_log_debug2(NGX_LOG_DEBUG_WASM, vm->pool->log, 0,
                   "[wasm] free \"%V\" vm (vm: %p)",
                   &vm->name, vm);

    root = &vm->modules.rbtree.root;
    sentinel = &vm->modules.rbtree.sentinel;

    while (*root != *sentinel) {
        module = (ngx_wasm_vm_module_t *) ngx_rbtree_min(*root, *sentinel);

        ngx_rbtree_delete(&vm->modules.rbtree, &module->rbnode.node);

        ngx_log_debug4(NGX_LOG_DEBUG_WASM, vm->pool->log, 0,
                       "[wasm] free \"%V\" module in \"%V\" vm"
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

    ngx_wasm_rbtree_set_named_node(&module->rbnode, &module->name);
    ngx_rbtree_insert(&vm->modules.rbtree, &module->rbnode.node);

    ngx_log_debug4(NGX_LOG_DEBUG_WASM, vm->log, 0,
                   "[wasm] registered \"%V\" module in \"%V\" vm"
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


ngx_wasm_vm_module_t *
ngx_wasm_vm_get_module(ngx_wasm_vm_t *vm, ngx_str_t *mod_name)
{
    ngx_rbtree_node_t     *n;
    ngx_wasm_vm_module_t  *module;

    n = ngx_wasm_rbtree_lookup_named_node(&vm->modules.rbtree, mod_name->data,
                                          mod_name->len);
    if (n != NULL) {
        module = (ngx_wasm_vm_module_t *)
                     ((u_char *) n - offsetof(ngx_wasm_vm_module_t, rbnode));
        return module;
    }

    return NULL;
}


ngx_int_t
ngx_wasm_vm_init(ngx_wasm_vm_t *vm, ngx_wasm_hfuncs_resolver_t *hf_resolver,
    ngx_wrt_t *runtime)
{
    if (vm->wrt) {
        return NGX_DONE;
    }

    vm->runtime = runtime;

    if (runtime->engine_new) {
        vm->wrt = runtime->engine_new(vm->pool, hf_resolver);
        if (vm->wrt == NULL) {
            ngx_wasm_log_error(NGX_LOG_EMERG, vm->log, 0,
                               "failed to initialize \"%V\" runtime "
                               "for \"%V\" vm", &vm->runtime->name, &vm->name);
            return NGX_ERROR;
        }
    }

    return NGX_OK;
}


ngx_int_t
ngx_wasm_vm_load_module(ngx_wasm_vm_t *vm, ngx_str_t *mod_name)
{
    ssize_t                n, fsize;
    ngx_int_t              rc = NGX_ERROR;
    ngx_file_t             file;
    ngx_fd_t               fd;
    ngx_wasm_vm_module_t  *module;
    ngx_wrt_error_pt       err = NULL;

    ngx_wasm_vm_check_init(vm, NGX_ABORT);

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

    module->bytes.len = fsize;
    module->bytes.data = ngx_pnalloc(vm->pool, module->bytes.len);
    if (module->bytes.data == NULL) {
        ngx_wasm_log_error(NGX_LOG_EMERG, vm->log, 0,
                           "failed to allocate bytes for \"%V\" "
                           "module from \"%V\"",
                           &module->name, &module->path);
        goto close;
    }

    n = ngx_read_file(&file, (u_char *) module->bytes.data, fsize, 0);
    if (n == NGX_ERROR) {
        ngx_wasm_log_error(NGX_LOG_EMERG, vm->log, ngx_errno,
                           ngx_read_file_n " \"%V\" failed",
                           &file.name);
        goto close;
    }

    if (n != fsize) {
        ngx_wasm_log_error(NGX_LOG_EMERG, vm->log, 0,
                           ngx_read_file_n " \"%V\" returned only "
                           "%z bytes instead of %uz", &file.name,
                           n, fsize);
        goto close;
    }

    ngx_wasm_log_error(NGX_LOG_NOTICE, vm->log, 0,
                       "loading \"%V\" module from \"%V\"",
                       &module->name, &module->path);

    module->wrt = vm->runtime->module_new(vm->wrt, &module->name, &module->bytes,
                                          module->flags, &err);
    if (module->wrt == NULL) {
        ngx_wasm_vm_log_error(NGX_LOG_EMERG, vm->log, err, NULL,
                              "failed to load \"%V\" module from \"%V\"",
                              &module->name, &module->path);
        goto close;
    }

    ngx_queue_init(&module->instances_queue);

    module->flags |= NGX_WASM_MODULE_LOADED;

    ngx_log_debug4(NGX_LOG_DEBUG_WASM, vm->log, 0,
                   "[wasm] loaded \"%V\" module in \"%V\" vm"
                   " (vm: %p, module: %p)",
                   &module->name, &vm->name,
                   vm, module);

    rc = NGX_OK;

close:

    if (ngx_close_file(fd) == NGX_FILE_ERROR) {
        ngx_wasm_log_error(NGX_LOG_ERR, vm->log, ngx_errno,
                           ngx_close_file_n " \"%V\" failed", &file.name);
    }

    return rc;
}


ngx_int_t
ngx_wasm_vm_load_modules(ngx_wasm_vm_t *vm)
{
    ngx_int_t              rc;
    ngx_rbtree_node_t     *node, *root, *sentinel;
    ngx_wasm_vm_module_t  *module;

    ngx_wasm_vm_check_init(vm, NGX_ABORT);

    sentinel = vm->modules.rbtree.sentinel;
    root = vm->modules.rbtree.root;

    if (root == sentinel) {
        return NGX_OK;
    }

    for (node = ngx_rbtree_min(root, sentinel);
         node;
         node = ngx_rbtree_next(&vm->modules.rbtree, node))
    {
        module = (ngx_wasm_vm_module_t *) node;

        rc = ngx_wasm_vm_load_module(vm, &module->name);
        if (rc != NGX_OK) {
            return rc;
        }
    }

    return NGX_OK;
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
                   "[wasm] creating instance of \"%V\" module in \"%V\" vm"
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


void
ngx_wasm_vm_instance_bind_request(ngx_wasm_vm_instance_t *instance,
    ngx_http_request_t *r)
{
    ngx_wasm_vm_log_ctx_t  *log_ctx;

    log_ctx = (ngx_wasm_vm_log_ctx_t *) instance->log->data;
    log_ctx->orig_log = r->connection->log;

    ngx_memzero(&instance->hctx, sizeof(ngx_wasm_hctx_t));

    instance->hctx.log = instance->log;
    instance->hctx.type = NGX_WASM_HTYPE_REQUEST;
    instance->hctx.data.r = r;
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
                   "[wasm] free instance of \"%V\" module in \"%V\" vm"
                   " (vm: %p, module: %p, instance: %p)",
                   &instance->module->name, &instance->vm->name,
                   instance->vm, instance->module, instance);

    if (instance->wrt) {
        instance->vm->runtime->instance_free(instance->wrt);
    }

    if (instance->log) {
        ngx_pfree(instance->pool, instance->log);
    }

    ngx_queue_remove(&instance->queue);

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

#if (NGX_HAVE_VARIADIC_MACROS)
    if (fmt) {
        va_start(args, fmt);
        p = ngx_vslprintf(p, last, fmt, args);
        va_end(args);
    }

#else
    if (fmt) {
        p = ngx_vslprintf(p, last, fmt, args);
    }
#endif

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
