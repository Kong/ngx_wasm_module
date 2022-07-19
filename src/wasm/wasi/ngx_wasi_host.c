#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_wavm.h>


static ngx_int_t
ngx_wasi_hfuncs_random_get(ngx_wavm_instance_t *instance,
    wasm_val_t args[], wasm_val_t rets[])
{
    size_t     i;
    uint8_t   *buf;
    uint32_t   buf_len;

    buf = (uint8_t *) ngx_wavm_memory_lift(instance->memory, args[0].of.i32);
    buf_len = args[1].of.i32;

    for (i = 0; i < buf_len; i++) {
        buf[i] = (uint8_t) ngx_random();
    }

    rets[0] = (wasm_val_t) WASM_I32_VAL(0); /* Ok */
    return NGX_WAVM_OK;
}


static ngx_int_t
ngx_wasi_hfuncs_environ_get(ngx_wavm_instance_t *instance,
    wasm_val_t args[], wasm_val_t rets[])
{
#if 0
    // A sample implementation documenting
    // the behavior of the arguments:
    uint32_t           environ = args[0].of.i32;
    uint32_t           environ_buf = args[1].of.i32;
    uint32_t          *addrs;
    uint8_t           *envp;
    ngx_wrt_extern_t  *mem = instance->memory;

    addrs = (uint32_t*) ngx_wavm_memory_lift(mem, environ);
    envp = (uint8_t*) ngx_wavm_memory_lift(mem, environ_buf);

    snprintf((char*) envp, 6, "A=aaa");
    snprintf((char*) envp + 6, 8, "BB=bbbb");

    addrs[0] = environ_buf;
    addrs[1] = environ_buf + 6;
#endif

    /* TODO: nothing is returned for now */

    rets[0] = (wasm_val_t) WASM_I32_VAL(0); /* Ok */
    return NGX_WAVM_OK;
}


static ngx_int_t
ngx_wasi_hfuncs_environ_sizes_get(ngx_wavm_instance_t *instance,
    wasm_val_t args[], wasm_val_t rets[])
{
    uint32_t          *environ_size;
    uint32_t          *environ_buf_size;
    ngx_wrt_extern_t  *mem = instance->memory;

    /**
     * environ_size is the number of environment variables to be returned.
     * environ_buf_size is the size of the buffer containing the concatenation
     * of all null-terminated key=value pairs.
     * For example, given an environment {"A=aaa", "BB=bbbb"},
     * environ_size would be 2 and environ_buf_size would be 14.
     */
    environ_size = (uint32_t *) ngx_wavm_memory_lift(mem, args[0].of.i32);
    environ_buf_size = (uint32_t *) ngx_wavm_memory_lift(mem, args[1].of.i32);

    /* TODO: nothing is returned for now */

    *environ_size = 0;
    *environ_buf_size = 0;

    rets[0] = (wasm_val_t) WASM_I32_VAL(0); /* Ok */
    return NGX_WAVM_OK;
}


static ngx_int_t
ngx_wasi_hfuncs_nop(ngx_wavm_instance_t *instance,
    wasm_val_t args[], wasm_val_t rets[])
{
    return NGX_WAVM_NYI;
}


static ngx_wavm_host_func_def_t  ngx_wasi_hfuncs[] = {

   { ngx_string("environ_get"),
     &ngx_wasi_hfuncs_environ_get,
     ngx_wavm_arity_i32x2,
     ngx_wavm_arity_i32 },

   { ngx_string("environ_sizes_get"),
     &ngx_wasi_hfuncs_environ_sizes_get,
     ngx_wavm_arity_i32x2,
     ngx_wavm_arity_i32 },

   { ngx_string("fd_write"),
     &ngx_wasi_hfuncs_nop,
     ngx_wavm_arity_i32x4,
     ngx_wavm_arity_i32 },

   { ngx_string("proc_exit"),
     &ngx_wasi_hfuncs_nop,
     ngx_wavm_arity_i32,
     NULL },

   { ngx_string("random_get"),
     &ngx_wasi_hfuncs_random_get,
     ngx_wavm_arity_i32x2,
     ngx_wavm_arity_i32 },

   ngx_wavm_hfunc_null
};


ngx_wavm_host_def_t  ngx_wasi_host = {
    ngx_string("ngx_wasi"),
    ngx_wasi_hfuncs,
};
