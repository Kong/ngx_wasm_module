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


#define ngx_wasm_vm_check_init(vm)                                          \
    if (vm->vm_actions == NULL) {                                           \
        ngx_wasm_log_error(NGX_LOG_EMERG, vm->log, 0,                       \
                           "\"%V\" vm not initialized (vm: %p)",            \
                           &vm->name, vm);                                  \
        ngx_wasm_assert(0);                                                 \
        return NGX_ABORT;                                                   \
    }                                                                       \


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
    ngx_wasm_vm_bytes_t           vm_bytes;
    ngx_wasm_vm_module_pt         vm_module;
} ngx_wasm_vm_module_t;


typedef struct {
    ngx_rbtree_t                  rbtree;
    ngx_rbtree_node_t             sentinel;
} ngx_wasm_vm_modules_t;


struct ngx_wasm_vm_s {
    ngx_str_t                     name;
    ngx_pool_t                   *pool;
    ngx_log_t                    *log;
    ngx_wasm_vm_actions_t        *vm_actions;
    ngx_wasm_vm_engine_pt         vm_engine;
    ngx_wasm_vm_modules_t         modules;
    ngx_wasm_vm_error_t           last_err;
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
    ngx_rbtree_node_t             *node, *sentinel;
    ngx_int_t                      rc;

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

    vm->log = ngx_palloc(pool, sizeof(ngx_log_t));
    if (vm->log == NULL) {
        goto failed;
    }

    vm->log->file = log->file;
    vm->log->next = log->next;
    vm->log->writer = log->writer;
    vm->log->wdata = log->wdata;
    vm->log->log_level = log->log_level;
    vm->log->handler = ngx_wasm_vm_log_error_handler;

    log_ctx = ngx_palloc(pool, sizeof(ngx_wasm_vm_log_ctx_t));
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

    ngx_wasm_vm_free(vm);

    return NULL;
}


ngx_int_t
ngx_wasm_vm_init(ngx_wasm_vm_pt vm, ngx_wasm_vm_actions_t *vm_actions)
{
    if (vm->vm_actions) {
        return NGX_DONE;
    }

    vm->vm_actions = vm_actions;

    if (vm_actions->new_engine) {
        vm->vm_engine = vm_actions->new_engine(vm->pool);
        if (vm->vm_engine == NULL) {
            return NGX_ERROR;
        }
    }

    return NGX_OK;
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
ngx_wasm_vm_add_module(ngx_wasm_vm_pt vm, u_char *name, u_char *path)
{
    ngx_wasm_vm_module_t         *module;
    u_char                       *p;
    ngx_rbtree_node_t            *n;
    uint32_t                      hash;
    size_t                        nlen, plen;
    ngx_int_t                     rc = NGX_ERROR;

    nlen = ngx_strlen(name);
    plen = ngx_strlen(path);

    hash = ngx_crc32_short(name, nlen);

    module = ngx_wasm_vm_modules_lookup(&vm->modules.rbtree, name, nlen, hash);
    if (module) {
        return NGX_DECLINED;
    }

    /* module == NULL */

    module = ngx_pcalloc(vm->pool, sizeof(ngx_wasm_vm_module_t));
    if (module == NULL) {
        return NGX_ERROR;
    }

    module->path.len = plen;
    module->name.len = nlen;
    module->name.data = ngx_palloc(vm->pool, module->name.len + 1);
    if (module->name.data == NULL) {
        goto failed;
    }

    p = ngx_copy(module->name.data, name, module->name.len);
    *p = '\0';

    if (path) {
        module->path.data = ngx_palloc(vm->pool, module->path.len + 1);
        if (module->path.data == NULL) {
            goto failed;
        }

        p = ngx_copy(module->path.data, path, module->path.len);
        *p = '\0';

        if (ngx_strcmp(&module->path.data[module->path.len - 4], ".wat") == 0) {
            module->flags |= NGX_WASM_VM_MODULE_ISWAT;
        }
    }

    n = &module->rbnode;
    n->key = ngx_crc32_short(module->name.data, module->name.len);
    ngx_rbtree_insert(&vm->modules.rbtree, n);

    return NGX_OK;

failed:

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
ngx_wasm_vm_load_modules(ngx_wasm_vm_pt vm)
{
    ngx_wasm_vm_module_t         *module;
    ngx_rbtree_node_t            *node, *root, *sentinel;
    ngx_fd_t                      fd;
    ngx_file_t                    file;
    ssize_t                       n, fsize;

    ngx_wasm_vm_check_init(vm);

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

        if (module->flags & NGX_WASM_VM_MODULE_LOADED) {
            continue;
        }

        if (!module->vm_bytes.data && !module->path.data) {
            ngx_wasm_log_error(NGX_LOG_ALERT, vm->log, 0,
                               "NYI: module loading only supported via path");
            return NGX_ERROR;
        }

        if (!module->vm_bytes.data) {
            fd = ngx_open_file(module->path.data, NGX_FILE_RDONLY,
                               NGX_FILE_OPEN, 0);
            if (fd == NGX_INVALID_FILE) {
                ngx_wasm_log_error(NGX_LOG_EMERG, vm->log, ngx_errno,
                                   ngx_open_file_n " \"%V\" failed",
                                   &module->path);
                goto failed;
            }

            ngx_memzero(&file, sizeof(ngx_file_t));

            file.fd = fd;
            file.log = vm->log;
            file.name.len = ngx_strlen(module->path.data);
            file.name.data = (u_char *) module->path.data;

            if (ngx_fd_info(fd, &file.info) == NGX_FILE_ERROR) {
                ngx_wasm_log_error(NGX_LOG_EMERG, vm->log, ngx_errno,
                                   ngx_fd_info_n " \"%V\" failed", &file.name);
                goto failed;
            }

            fsize = ngx_file_size(&file.info);
            module->vm_bytes.len = fsize;
            module->vm_bytes.data = ngx_palloc(vm->pool, fsize);
            if (module->vm_bytes.data == NULL) {
                ngx_wasm_log_error(NGX_LOG_EMERG, vm->log, 0,
                                   "failed to allocate bytes for \"%V\" "
                                   "module from \"%V\"",
                                   &module->name, &module->path);
                goto failed;
            }

            n = ngx_read_file(&file, (u_char *) module->vm_bytes.data,
                              fsize, 0);
            if (n == NGX_ERROR) {
                ngx_wasm_log_error(NGX_LOG_EMERG, vm->log, ngx_errno,
                                   ngx_read_file_n " \"%V\" failed",
                                   &file.name);
                goto failed;
            }

            if (n != fsize) {
                ngx_wasm_log_error(NGX_LOG_EMERG, vm->log, 0,
                                   ngx_read_file_n " \"%V\" returned only "
                                   "%z bytes instead of %uz", &file.name,
                                   n, fsize);
                goto failed;
            }

            if (ngx_close_file(fd) == NGX_FILE_ERROR) {
                ngx_wasm_log_error(NGX_LOG_EMERG, vm->log, ngx_errno,
                                   ngx_close_file_n " \"%V\" failed",
                                   &file.name);
                goto failed;
            }
        }

        ngx_wasm_log_error(NGX_LOG_NOTICE, vm->log, 0,
                           "loading module \"%V\" from \"%V\"",
                           &module->name, &module->path);

        module->vm_module = vm->vm_actions->new_module(vm->vm_engine,
                                                       &module->vm_bytes,
                                                       module->flags,
                                                       &vm->last_err.vm_error);
        if (module->vm_module == NULL) {
            ngx_wasm_log_error(NGX_LOG_EMERG, vm->log, 0,
                               "failed to load module \"%V\" from \"%V\"",
                               &module->name, &module->path);
            goto failed;
        }

        module->flags |= NGX_WASM_VM_MODULE_LOADED;
    }

    return NGX_OK;

failed:

    if (fd != NGX_INVALID_FILE && ngx_close_file(fd) == NGX_FILE_ERROR) {
        ngx_wasm_log_error(NGX_LOG_ERR, vm->log, ngx_errno,
                           ngx_close_file_n " \"%V\" failed", &file.name);
    }

    return NGX_ERROR;
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
        p = ngx_snprintf(buf, len, " (\"%V\" vm not initialized)", &vm->name);
        len -= p - buf;
        buf = p;
        goto orig;
    }

    if (vm->last_err.vm_error || vm->last_err.vm_trap) {
        p = vm->vm_actions->log_error_handler(vm->last_err.vm_error,
                                              vm->last_err.vm_trap,
                                              buf, len);
        len -= p - buf;
        buf = p;
    }

    p = ngx_snprintf(buf, len, " <vm: %V, runtime: %V>", &vm->name,
                     vm->vm_actions->vm_name);
    len -= p - buf;
    buf = p;

orig:

    if (orig_log->handler) {
        p = orig_log->handler(orig_log, buf, len);
    }

    return p;
}


/* vi:set ft=c ts=4 sw=4 et fdm=marker: */
