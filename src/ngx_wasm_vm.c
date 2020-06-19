/*
 * Copyright (C) Thibault Charbonnier
 */

#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_wasm_vm.h>
#include <ngx_wasm_util.h>
#include <ngx_wasm_hostfuncs.h>


#define ngx_wasm_vm_check_init(vm, ret)                                     \
    if (vm->vm_actions == NULL) {                                           \
        ngx_wasm_log_error(NGX_LOG_EMERG, vm->log, 0,                       \
                           "\"%V\" vm not initialized (vm: %p)",            \
                           &vm->name, vm);                                  \
        ngx_wasm_assert(0);                                                 \
        return ret;                                                         \
    }                                                                       \


static void ngx_wasm_vm_log_error(ngx_uint_t level, ngx_log_t *log,
    ngx_wasm_vm_error_pt vm_error, ngx_wasm_vm_trap_pt vm_trap,
#if (NGX_HAVE_VARIADIC_MACROS)
    const char *fmt, ...);

#else
    const char *fmt, va_list args);
#endif
static u_char *ngx_wasm_vm_log_error_handler(ngx_log_t *log, u_char *buf,
    size_t len);


typedef struct {
    ngx_wasm_vm_pt                vm;
    ngx_log_t                    *orig_log;
} ngx_wasm_vm_log_ctx_t;


typedef struct {
    ngx_rbtree_node_t             rbnode;

    ngx_str_t                     name;
    ngx_str_t                     path;
    ngx_uint_t                    flags;

    ngx_wasm_vm_module_pt         vm_module;
} ngx_wasm_vm_module_t;


typedef struct {
    ngx_rbtree_t                  rbtree;
    ngx_rbtree_node_t             sentinel;
} ngx_wasm_vm_modules_t;


struct ngx_wasm_vm_s {
    ngx_str_t                     name;
    ngx_str_t                    *vm_name;
    ngx_pool_t                   *pool;
    ngx_log_t                    *log;

    ngx_wasm_vm_actions_t        *vm_actions;
    ngx_wasm_vm_engine_pt         vm_engine;
    ngx_wasm_vm_modules_t         modules;
};


struct ngx_wasm_instance_s {
    ngx_pool_t                   *pool;
    ngx_log_t                    *log;

    ngx_wasm_vm_pt                vm;
};


void/*{{{*/
ngx_wasm_vm_modules_insert(ngx_rbtree_node_t *temp,
    ngx_rbtree_node_t *node, ngx_rbtree_node_t *sentinel)
{
    ngx_wasm_vm_module_t   *n, *t;
    ngx_rbtree_node_t     **p;

    for ( ;; ) {

        n = (ngx_wasm_vm_module_t *) node;
        t = (ngx_wasm_vm_module_t *) temp;

        if (node->key != temp->key) {
            p = (node->key < temp->key) ? &temp->left : &temp->right;

        } else if (n->name.len != t->name.len) {
            p = (n->name.len < t->name.len) ? &temp->left : &temp->right;

        } else {
            p = (ngx_memcmp(n->name.data, t->name.data, n->name.len) < 0)
                ? &temp->left : &temp->right;
        }

        if (*p == sentinel) {
            break;
        }

        temp = *p;
    }

    *p = node;
    node->parent = temp;
    node->left = sentinel;
    node->right = sentinel;
    ngx_rbt_red(node);
}


static ngx_wasm_vm_module_t *
ngx_wasm_vm_modules_lookup(ngx_rbtree_t *rbtree, u_char *name, size_t len,
    uint32_t hash)
{
    ngx_wasm_vm_module_t   *nm;
    ngx_rbtree_node_t      *node, *sentinel;
    ngx_int_t               rc;

    node = rbtree->root;
    sentinel = rbtree->sentinel;

    while (node != sentinel) {
        nm = (ngx_wasm_vm_module_t *) node;

        if (hash != node->key) {
            node = (hash < node->key) ? node->left : node->right;
            continue;
        }

        rc = ngx_memcmp(name, nm->name.data, len);

        if (rc < 0) {
            node = node->left;
            continue;
        }

        if (rc > 0) {
            node = node->right;
            continue;
        }

        return nm;
    }

    return NULL;
}


static ngx_wasm_vm_module_t *
ngx_wasm_vm_get_module(ngx_wasm_vm_pt vm, u_char *name)
{
    uint32_t                      hash;
    size_t                        nlen;

    nlen = ngx_strlen(name);
    hash = ngx_crc32_short(name, nlen);

    return ngx_wasm_vm_modules_lookup(&vm->modules.rbtree, name, nlen, hash);
}/*}}}*/


ngx_wasm_vm_pt
ngx_wasm_vm_new(const char *name, ngx_log_t *log)
{
    ngx_wasm_vm_pt           vm;
    ngx_pool_t              *pool;
    ngx_wasm_vm_log_ctx_t   *log_ctx;
    u_char                  *p;

    pool = ngx_create_pool(NGX_DEFAULT_POOL_SIZE, log);
    if (pool == NULL) {
        return NULL;
    }

    vm = ngx_pcalloc(pool, sizeof(struct ngx_wasm_vm_s));
    if (vm == NULL) {
        ngx_destroy_pool(pool);
        return NULL;
    }

    vm->pool = pool;

    vm->log = ngx_pcalloc(pool, sizeof(ngx_log_t));
    if (vm->log == NULL) {
        goto failed;
    }

    vm->log->file = log->file;
    vm->log->next = log->next;
    vm->log->writer = log->writer;
    vm->log->wdata = log->wdata;
    vm->log->log_level = log->log_level;
    vm->log->handler = ngx_wasm_vm_log_error_handler;

    log_ctx = ngx_palloc(pool,
                         sizeof(ngx_wasm_vm_log_ctx_t));
    if (log_ctx == NULL) {
        goto failed;
    }

    log_ctx->vm = vm;
    log_ctx->orig_log = log;

    vm->log->data = log_ctx;

    vm->name.len = ngx_strlen(name);
    vm->name.data = ngx_palloc(pool, vm->name.len + 1);
    if (vm->name.data == NULL) {
        goto failed;
    }

    p = ngx_copy(vm->name.data, name, vm->name.len);
    *p = '\0';

    ngx_rbtree_init(&vm->modules.rbtree, &vm->modules.sentinel,
                    ngx_wasm_vm_modules_insert);

    return vm;

failed:

    ngx_wasm_log_error(NGX_LOG_EMERG, log, 0,
                       "failed to create \"%s\" vm", name);

    ngx_wasm_vm_free(vm);

    return NULL;
}


void
ngx_wasm_vm_free(ngx_wasm_vm_pt vm)
{
    /* TODO: gracefully unload modules */

    if (vm->vm_actions
        && vm->vm_actions->free_engine
        && vm->vm_engine)
    {
        vm->vm_actions->free_engine(vm->vm_engine);
    }

    ngx_destroy_pool(vm->pool);
}


ngx_int_t
ngx_wasm_vm_add_module(ngx_wasm_vm_pt vm, u_char *mod_name, u_char *path)
{
    ngx_wasm_vm_module_t         *module;
    u_char                       *p;
    ngx_rbtree_node_t            *n;
    ngx_int_t                     rc = NGX_ERROR;

    module = ngx_wasm_vm_get_module(vm, mod_name);
    if (module) {
        return NGX_DECLINED;
    }

    /* module == NULL */

    module = ngx_pcalloc(vm->pool, sizeof(ngx_wasm_vm_module_t));
    if (module == NULL) {
        goto failed;
    }

    module->name.len = ngx_strlen(mod_name);
    module->name.data = ngx_palloc(vm->pool, module->name.len + 1);
    if (module->name.data == NULL) {
        goto failed;
    }

    p = ngx_copy(module->name.data, mod_name, module->name.len);
    *p = '\0';

    module->path.len = ngx_strlen(path);
    module->path.data = ngx_palloc(vm->pool, module->path.len + 1);
    if (module->path.data == NULL) {
        goto failed;
    }

    p = ngx_copy(module->path.data, path, module->path.len);
    *p = '\0';

    if (ngx_strcmp(&module->path.data[module->path.len - 4], ".wat") == 0) {
        module->flags |= NGX_WASM_VM_MODULE_ISWAT;
    }

    n = &module->rbnode;
    n->key = ngx_crc32_short(module->name.data, module->name.len);
    ngx_rbtree_insert(&vm->modules.rbtree, n);

    return NGX_OK;

failed:

    ngx_wasm_log_error(NGX_LOG_EMERG, vm->log, 0,
                       "failed to add module \"%V\" from \"%V\"",
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

    return rc;
}


ngx_int_t
ngx_wasm_vm_has_module(ngx_wasm_vm_pt vm, u_char *mod_name)
{
    ngx_wasm_vm_module_t         *module;

    module = ngx_wasm_vm_get_module(vm, mod_name);
    if (module == NULL) {
        return NGX_DECLINED;
    }

    return NGX_OK;
}


ngx_int_t
ngx_wasm_vm_init_runtime(ngx_wasm_vm_pt vm, ngx_str_t *vm_name,
    ngx_wasm_vm_actions_t *vm_actions)
{
    ngx_wasm_vm_error_pt          vm_error = NULL;

    if (vm->vm_actions) {
        return NGX_DONE;
    }

    vm->vm_name = vm_name;
    vm->vm_actions = vm_actions;

    if (vm_actions->new_engine) {
        vm->vm_engine = vm_actions->new_engine(vm->pool, &vm_error);
        if (vm->vm_engine == NULL) {
            ngx_wasm_vm_log_error(NGX_LOG_EMERG, vm->log, vm_error, NULL,
                                  "failed to initialize \"%V\" vm runtime",
                                  &vm->name);
            return NGX_ERROR;
        }
    }

    return NGX_OK;
}


ngx_int_t
ngx_wasm_vm_load_module(ngx_wasm_vm_pt vm, u_char *mod_name)
{
    ngx_wasm_vm_module_t         *module;
    ngx_wasm_vm_error_pt          vm_error = NULL;
    ngx_str_t                     bytes;
    ngx_fd_t                      fd;
    ngx_file_t                    file;
    ssize_t                       n, fsize;

    ngx_wasm_vm_check_init(vm, NGX_ABORT);

    module = ngx_wasm_vm_get_module(vm, mod_name);
    if (module == NULL) {
        ngx_wasm_log_error(NGX_LOG_EMERG, vm->log, 0, "no \"%s\" module",
                           mod_name);
        return NGX_DECLINED;
    }

    if (module->flags & NGX_WASM_VM_MODULE_LOADED) {
        return NGX_OK;
    }

    if (!module->path.len) {
        ngx_wasm_log_error(NGX_LOG_ALERT, vm->log, 0,
                           "NYI - ngx_wasm_vm: module loading only supported "
                           "via path");
        return NGX_ERROR;
    }

    fd = ngx_open_file(module->path.data, NGX_FILE_RDONLY,
                       NGX_FILE_OPEN, 0);
    if (fd == NGX_INVALID_FILE) {
        ngx_wasm_log_error(NGX_LOG_EMERG, vm->log, ngx_errno,
                           ngx_open_file_n " \"%V\" failed",
                           &module->path);
        return NGX_ERROR;
    }

    ngx_memzero(&file, sizeof(ngx_file_t));

    file.fd = fd;
    file.log = vm->log;
    file.name.len = ngx_strlen(module->path.data);
    file.name.data = (u_char *) module->path.data;

    if (ngx_fd_info(fd, &file.info) == NGX_FILE_ERROR) {
        ngx_wasm_log_error(NGX_LOG_EMERG, vm->log, ngx_errno,
                           ngx_fd_info_n " \"%V\" failed", &file.name);
        goto close;
    }

    fsize = ngx_file_size(&file.info);

    bytes.len = fsize;
    bytes.data = ngx_alloc(bytes.len, vm->log);
    if (bytes.data == NULL) {
        ngx_wasm_log_error(NGX_LOG_EMERG, vm->log, 0,
                           "failed to allocate bytes for \"%V\" "
                           "module from \"%V\"",
                           &module->name, &module->path);
        goto close;
    }

    n = ngx_read_file(&file, (u_char *) bytes.data, fsize, 0);
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

    if (ngx_close_file(fd) == NGX_FILE_ERROR) {
        ngx_wasm_log_error(NGX_LOG_EMERG, vm->log, ngx_errno,
                           ngx_close_file_n " \"%V\" failed",
                           &file.name);
        fd = NGX_INVALID_FILE;
        goto close;
    }

    ngx_wasm_log_error(NGX_LOG_NOTICE, vm->log, 0,
                       "loading module \"%V\" from \"%V\"",
                       &module->name, &module->path);

    module->vm_module = vm->vm_actions->new_module(vm->vm_engine,
                                                   &module->name,
                                                   &bytes,
                                                   module->flags,
                                                   &vm_error);
    if (module->vm_module == NULL) {
        ngx_wasm_vm_log_error(NGX_LOG_EMERG, vm->log, vm_error, NULL,
                              "failed to load module \"%V\" from \"%V\"",
                              &module->name, &module->path);
        return NGX_ERROR;
    }

    module->flags |= NGX_WASM_VM_MODULE_LOADED;

    return NGX_OK;

close:

    if (fd != NGX_INVALID_FILE && ngx_close_file(fd) == NGX_FILE_ERROR) {
        ngx_wasm_log_error(NGX_LOG_ERR, vm->log, ngx_errno,
                           ngx_close_file_n " \"%V\" failed", &file.name);
    }

    if (bytes.data) {
        ngx_free(bytes.data);
    }

    return NGX_ERROR;
}


ngx_int_t
ngx_wasm_vm_load_modules(ngx_wasm_vm_pt vm)
{
    ngx_wasm_vm_module_t         *module;
    ngx_rbtree_node_t            *node, *root, *sentinel;
    ngx_int_t                     rc;

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

        rc = ngx_wasm_vm_load_module(vm, module->name.data);
        if (rc != NGX_OK) {
            return rc;
        }
    }

    return NGX_OK;
}


ngx_int_t
ngx_wasm_vm_call_by_name(ngx_wasm_vm_pt vm, ngx_str_t *mod_name,
    ngx_str_t *func_name)
{
    ngx_wasm_vm_error_pt          vm_error = NULL;
    ngx_wasm_vm_trap_pt           vm_trap = NULL;
    ngx_int_t                     rc;

    ngx_wasm_vm_check_init(vm, NGX_ABORT);

    rc = vm->vm_actions->call_by_name(vm->vm_engine, mod_name, func_name,
                                      &vm_error, &vm_trap);
    if (rc != NGX_OK) {
        ngx_wasm_vm_log_error(NGX_LOG_ERR, vm->log, vm_error, vm_trap,
                              "NYI: stacktrace");
    }

    return rc;
}


#if 0
ngx_wasm_instance_pt
ngx_wasm_vm_new_instance(ngx_wasm_vm_pt vm, u_char *module_name)
{
    ngx_wasm_vm_module_t         *module;
    ngx_wasm_instance_pt          instance;
    ngx_wasm_vm_error_pt          vm_error = NULL;
    ngx_wasm_vm_trap_pt           vm_trap = NULL;

    ngx_wasm_vm_check_init(vm, NULL);

    module = ngx_wasm_vm_get_module(vm, module_name);
    if (module == NULL) {
        ngx_wasm_log_error(NGX_LOG_ERR, vm->log, 0,
                           "failed to create instance of \"%s\" module: "
                           "no such module defined", module_name);
        return NULL;
    }

    instance = ngx_palloc(vm->pool, sizeof(struct ngx_wasm_instance_s));
    if (instance == NULL) {
        ngx_wasm_log_error(NGX_LOG_ERR, vm->log, 0,
                           "failed to create instance of \"%s\" module: "
                           "could not allocate memory");
        return NULL;
    }

    instance->vm = vm;
    instance->pool = vm->pool;
    instance->log = vm->log;

    instance->vm_instance = vm->vm_actions->new_instance(module->vm_module,
                                                         &vm_error, &vm_trap);
    if (instance->vm_instance == NULL) {
        ngx_wasm_vm_log_error(NGX_LOG_EMERG, vm->log, vm_error, vm_trap,
                              "failed to create instance of \"%s\" module",
                              module_name);
        goto failed;
    }

    return instance;

failed:

    ngx_wasm_vm_free_instance(instance);

    return NULL;
}


void
ngx_wasm_vm_free_instance(ngx_wasm_instance_pt instance)
{
    if (instance->vm_instance) {
        instance->vm->vm_actions->free_instance(instance->vm_instance);
    }

    ngx_pfree(instance->pool, instance);
}
#endif


static void
ngx_wasm_vm_log_error(ngx_uint_t level, ngx_log_t *log,
    ngx_wasm_vm_error_pt vm_error, ngx_wasm_vm_trap_pt vm_trap,
#if (NGX_HAVE_VARIADIC_MACROS)
    const char *fmt, ...)

#else
    const char *fmt, va_list args)
#endif
{
#if (NGX_HAVE_VARIADIC_MACROS)
    va_list                  args;
#endif
    u_char                  *p, *last;
    u_char                   errstr[NGX_MAX_ERROR_STR];
    ngx_wasm_vm_log_ctx_t   *ctx;
    ngx_wasm_vm_pt           vm;

    ctx = log->data;
    vm = ctx->vm;

    last = errstr + NGX_MAX_ERROR_STR;
    p = &errstr[0];

#if (NGX_HAVE_VARIADIC_MACROS)
    va_start(args, fmt);
    p = ngx_vslprintf(p, last, fmt, args);
    va_end(args);

#else
    p = ngx_vslprintf(p, last, fmt, args);
#endif

    if (vm->vm_actions == NULL) {
        p = ngx_snprintf(p, last - p, " (\"%V\" vm not initialized)",
                         &vm->name);

    } else if (vm_error || vm_trap) {
        p = vm->vm_actions->log_error_handler(vm_error, vm_trap, p, last - p);
    }

    ngx_wasm_log_error(level, log, 0, "%*s", p - errstr, errstr);
}


static u_char *
ngx_wasm_vm_log_error_handler(ngx_log_t *log, u_char *buf, size_t len)
{
    u_char                  *p = buf;
    ngx_log_t               *orig_log;
    ngx_wasm_vm_log_ctx_t   *ctx;
    ngx_wasm_vm_pt           vm;

    ctx = log->data;
    vm = ctx->vm;
    orig_log = ctx->orig_log;

    if (vm->vm_actions == NULL) {
        p = ngx_snprintf(buf, len, " <vm: %V, runtime: ?>", &vm->name);

    } else {
        p = ngx_snprintf(buf, len, " <vm: %V, runtime: %V>", &vm->name,
                         vm->vm_name);
    }

    len -= p - buf;
    buf = p;

    if (orig_log->handler) {
        p = orig_log->handler(orig_log, buf, len);
    }

    return p;
}


/* vi:set ft=c ts=4 sw=4 et fdm=marker: */
