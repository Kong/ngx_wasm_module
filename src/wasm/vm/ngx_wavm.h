#ifndef _NGX_WAVM_H_INCLUDED_
#define _NGX_WAVM_H_INCLUDED_


#include <ngx_wasm.h>
#include <ngx_wavm_host.h>


#define NGX_WAVM_OK                   0
#define NGX_WAVM_ERROR               -10
#define NGX_WAVM_BAD_ARG             -11
#define NGX_WAVM_BAD_USAGE           -12
#define NGX_WAVM_NYI                 -13


typedef struct {
    ngx_log_t                         *orig_log;
    ngx_wavm_t                        *vm;
    ngx_wavm_instance_t               *instance;
} ngx_wavm_log_ctx_t;


struct ngx_wavm_funcref_s {
    ngx_str_node_t                     sn;         /* module->funcs */
    ngx_str_t                          name;
    ngx_uint_t                         exports_idx;
    ngx_wavm_module_t                 *module;
};


struct ngx_wavm_func_s {
    ngx_uint_t                         idx;
    ngx_str_t                          name;
    wasm_functype_t                   *functype;
    const wasm_valtype_vec_t          *argstypes;
    wasm_val_vec_t                     args;
    wasm_val_vec_t                     rets;
    ngx_wavm_instance_t               *instance;
    ngx_wrt_extern_t                  *ext;
};


struct ngx_wavm_instance_s {
    ngx_queue_t                        q;          /* vm->instances */
    ngx_uint_t                         state;
    ngx_wavm_t                        *vm;
    ngx_wavm_module_t                 *module;
    ngx_pool_cleanup_t                *cln;
    ngx_pool_t                        *pool;
    ngx_log_t                         *log;
    ngx_wavm_log_ctx_t                 log_ctx;
    ngx_wrt_err_t                      wrt_error;
    ngx_wrt_store_t                    wrt_store;
    ngx_wrt_instance_t                 wrt_instance;
    ngx_wrt_extern_t                 **externs;
    ngx_wrt_extern_t                  *memory;
    ngx_array_t                        funcs;
    ngx_str_t                          trapmsg;
    u_char                            *trapbuf;
    void                              *data;
    unsigned                           hostcall:1;
    unsigned                           trapped:1;
};


struct ngx_wavm_module_s {
    ngx_str_node_t                     sn;         /* vm->modules_tree */
    ngx_wavm_t                        *vm;
    ngx_uint_t                         idx;
    ngx_uint_t                         state;
    ngx_str_t                          name;
    ngx_str_t                          path;
    wasm_byte_vec_t                    bytes;
    wasm_importtype_vec_t              imports;
    wasm_exporttype_vec_t              exports;
    ngx_wrt_module_t                   wrt_module;
    ngx_rbtree_t                       funcs_tree;
    ngx_rbtree_node_t                  funcs_sentinel;
    ngx_wavm_funcref_t                *f_start;
    ngx_wavm_host_def_t               *host_def;
    ngx_array_t                        hfuncs;
};


struct ngx_wavm_s {
    const ngx_str_t                   *name;
    ngx_wavm_conf_t                   *config;
    ngx_uint_t                         state;
    ngx_uint_t                         modules_max;
    ngx_pool_t                        *pool;
    ngx_log_t                         *log;
    ngx_wavm_log_ctx_t                 log_ctx;
    ngx_rbtree_t                       modules_tree;
    ngx_rbtree_node_t                  modules_sentinel;
    ngx_queue_t                        instances;
    ngx_wavm_host_def_t               *core_host;
    ngx_wrt_engine_t                   wrt_engine;
};


ngx_wavm_t *ngx_wavm_create(ngx_cycle_t *cycle, const ngx_str_t *name,
    ngx_wavm_conf_t *vm_conf, ngx_wavm_host_def_t *core_host);
ngx_int_t ngx_wavm_init(ngx_wavm_t *vm);
ngx_int_t ngx_wavm_load(ngx_wavm_t *vm);
void ngx_wavm_destroy(ngx_wavm_t *vm);


ngx_int_t ngx_wavm_module_add(ngx_wavm_t *vm, ngx_str_t *name, ngx_str_t *path);
ngx_wavm_module_t *ngx_wavm_module_lookup(ngx_wavm_t *vm, ngx_str_t *name);
ngx_int_t ngx_wavm_module_link(ngx_wavm_module_t *module,
    ngx_wavm_host_def_t *host);
ngx_wavm_funcref_t *ngx_wavm_module_func_lookup(ngx_wavm_module_t *module,
    ngx_str_t *name);


ngx_wavm_instance_t *ngx_wavm_instance_create(ngx_wavm_module_t *module,
    ngx_pool_t *pool, ngx_log_t *log, void *data);
ngx_int_t ngx_wavm_instance_call_func(ngx_wavm_instance_t *instance,
    ngx_wavm_func_t *f, wasm_val_vec_t **rets, ...);
ngx_int_t ngx_wavm_instance_call_func_vec(ngx_wavm_instance_t *instance,
    ngx_wavm_func_t *f, wasm_val_vec_t **rets, wasm_val_vec_t *args);
ngx_int_t ngx_wavm_instance_call_funcref(ngx_wavm_instance_t *instance,
    ngx_wavm_funcref_t *funcref, wasm_val_vec_t **rets, ...);
ngx_int_t ngx_wavm_instance_call_funcref_vec(ngx_wavm_instance_t *instance,
    ngx_wavm_funcref_t *funcref, wasm_val_vec_t **rets, wasm_val_vec_t *args);
void ngx_wavm_instance_destroy(ngx_wavm_instance_t *instance);
void ngx_wavm_instance_trap_printf(ngx_wavm_instance_t *instance,
    const char *fmt, ...);
void ngx_wavm_instance_trap_vprintf(ngx_wavm_instance_t *instance,
    const char *fmt, va_list args);


static ngx_inline void
ngx_wavm_instance_set_data(ngx_wavm_instance_t *instance, void *data,
    ngx_log_t *log)
{
    instance->data = data;
    instance->log->connection = log->connection;
    instance->log_ctx.orig_log = log;
}


static ngx_inline size_t
ngx_wavm_memory_data_size(ngx_wrt_extern_t *mem)
{
    ngx_wasm_assert(mem->kind == NGX_WRT_EXTERN_MEMORY);

#ifdef NGX_WASM_HAVE_WASMTIME
    return wasmtime_memory_data_size(mem->context, &mem->ext.of.memory);

#else
    return wasm_memory_data_size(wasm_extern_as_memory(mem->ext));
#endif
}


static ngx_inline byte_t *
ngx_wavm_memory_base(ngx_wrt_extern_t *mem)
{
    ngx_wasm_assert(mem->kind == NGX_WRT_EXTERN_MEMORY);

#ifdef NGX_WASM_HAVE_WASMTIME
    return (byte_t *) wasmtime_memory_data(mem->context, &mem->ext.of.memory);

#else
    return wasm_memory_data(wasm_extern_as_memory(mem->ext));
#endif
}


static ngx_inline void *
ngx_wavm_memory_lift(ngx_wrt_extern_t *mem, ngx_wavm_ptr_t p,
    uint32_t size, uint32_t align, unsigned *err_count)
{
    ngx_wasm_assert(mem->kind == NGX_WRT_EXTERN_MEMORY);

    if (p == 0) {
        /* NULL pointers are only valid for empty slices */

        if (size != 0) {
            goto fail;
        }

        return NULL;
    }

    /* Check bounds */

    if (p + size < p || p + size > ngx_wavm_memory_data_size(mem)) {
        dd("bound check failed, p=%x, size=%d", p, size);
        goto fail;
    }

    /* Check alignment */

    if (align > 1 && (p & (align - 1)) != 0) {
        dd("alignment check failed, p=%x, size=%d", p, size);
        goto fail;
    }

    return ngx_wavm_memory_base(mem) + p;

fail:

    if (err_count) {
        *err_count = 1;
    }

    return NULL;
}


static ngx_inline unsigned
ngx_wavm_memory_memcpy(ngx_wrt_extern_t *mem, ngx_wavm_ptr_t p, u_char *buf,
    size_t len)
{
    unsigned   err_count = 0;
    void      *dest;

    dest = ngx_wavm_memory_lift(mem, p, len, 1, &err_count);
    if (err_count) {
        return 0;
    }

    ngx_memcpy(dest, buf, len);

    return 1;
}


static ngx_inline ngx_wavm_ptr_t
ngx_wavm_memory_cpymem(ngx_wrt_extern_t *mem, ngx_wavm_ptr_t p, u_char *buf,
    size_t len)
{
    if (ngx_wavm_memory_memcpy(mem, p, buf, len) == 0) {
        return 0;
    }

    return p + len;
}


#endif /* _NGX_WAVM_H_INCLUDED_ */
