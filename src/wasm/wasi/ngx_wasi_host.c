#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_wavm.h>
#include <ngx_wasi.h>


static ngx_int_t
ngx_wasi_hfuncs_random_get(ngx_wavm_instance_t *instance,
    wasm_val_t args[], wasm_val_t rets[])
{
    size_t     i;
    uint8_t   *buf;
    uint32_t   buf_len;

    buf_len = args[1].of.i32;
    buf = NGX_WAVM_HOST_LIFT_SLICE(instance, args[0].of.i32, buf_len);

    for (i = 0; i < buf_len; i++) {
        buf[i] = (uint8_t) ngx_random();
    }

    rets[0] = (wasm_val_t) WASM_I32_VAL(WASI_ERRNO_SUCCESS);
    return NGX_WAVM_OK;
}


static ngx_int_t
ngx_wasi_hfuncs_args_get(ngx_wavm_instance_t *instance,
    wasm_val_t args[], wasm_val_t rets[])
{
    /* TODO: nothing is returned for now.
       See ngx_wasi_hfuncs_environ_get for a sample implementation. */

    rets[0] = (wasm_val_t) WASM_I32_VAL(WASI_ERRNO_SUCCESS);
    return NGX_WAVM_OK;
}


static ngx_int_t
ngx_wasi_hfuncs_args_sizes_get(ngx_wavm_instance_t *instance,
    wasm_val_t args[], wasm_val_t rets[])
{
    uint32_t  *args_size;
    uint32_t  *args_buf_size;

    /* TODO: nothing is returned for now.
       See ngx_wasi_hfuncs_environ_get for more info on the format
       of the arguments. */

    args_size = NGX_WAVM_HOST_LIFT(instance, args[0].of.i32, uint32_t);
    args_buf_size = NGX_WAVM_HOST_LIFT(instance, args[1].of.i32, uint32_t);

    *args_size = 0;
    *args_buf_size = 0;

    rets[0] = (wasm_val_t) WASM_I32_VAL(WASI_ERRNO_SUCCESS);
    return NGX_WAVM_OK;
}


static ngx_int_t
ngx_wasi_hfuncs_clock_time_get(ngx_wavm_instance_t *instance,
    wasm_val_t args[], wasm_val_t rets[])
{
    uint32_t   clock_id;
    uint64_t   precision;
    uint64_t  *time;

    clock_id = args[0].of.i32;
    precision = args[1].of.i64;
    time = NGX_WAVM_HOST_LIFT(instance, args[2].of.i32, uint64_t);

    /* precision is ignored for now, same as proxy-wasm-cpp-host */
    (void) precision;

    switch (clock_id) {
    case WASI_CLOCKID_REALTIME:
        ngx_time_update();
        *time = ngx_current_msec;
        break;

    case WASI_CLOCKID_MONOTONIC:
        *time = ngx_wasm_monotonic_time();
        break;

    default:
        rets[0] = (wasm_val_t) WASM_I32_VAL(WASI_ERRNO_NOTSUP);
        return NGX_WAVM_OK;
    }

    rets[0] = (wasm_val_t) WASM_I32_VAL(WASI_ERRNO_SUCCESS);
    return NGX_WAVM_OK;
}


static ngx_int_t
ngx_wasi_hfuncs_environ_get(ngx_wavm_instance_t *instance,
    wasm_val_t args[], wasm_val_t rets[])
{
#if 0
    // A sample implementation documenting
    // the behavior of the arguments:
    uint32_t   environ = args[0].of.i32;
    uint32_t   environ_buf = args[1].of.i32;
    uint32_t  *addrs;
    uint8_t   *envp;

    addrs = NGX_WAVM_HOST_LIFT(instance, environ, uint64_t);
    envp = NGX_WAVM_HOST_LIFT(instance, environ_buf, uint8_t);

    snprintf((char *) envp, 6, "A=aaa");
    snprintf((char *) envp + 6, 8, "BB=bbbb");

    addrs[0] = environ_buf;
    addrs[1] = environ_buf + 6;
#endif

    /* TODO: nothing is returned for now */

    rets[0] = (wasm_val_t) WASM_I32_VAL(WASI_ERRNO_SUCCESS);
    return NGX_WAVM_OK;
}


static ngx_int_t
ngx_wasi_hfuncs_environ_sizes_get(ngx_wavm_instance_t *instance,
    wasm_val_t args[], wasm_val_t rets[])
{
    uint32_t  *environ_size;
    uint32_t  *environ_buf_size;

    /**
     * environ_size is the number of environment variables to be returned.
     * environ_buf_size is the size of the buffer containing the concatenation
     * of all null-terminated key=value pairs.
     * For example, given an environment {"A=aaa", "BB=bbbb"},
     * environ_size would be 2 and environ_buf_size would be 14.
     */
    environ_size = NGX_WAVM_HOST_LIFT(instance, args[0].of.i32, uint32_t);
    environ_buf_size = NGX_WAVM_HOST_LIFT(instance, args[1].of.i32, uint32_t);

    /* TODO: nothing is returned for now */

    *environ_size = 0;
    *environ_buf_size = 0;

    rets[0] = (wasm_val_t) WASM_I32_VAL(WASI_ERRNO_SUCCESS);
    return NGX_WAVM_OK;
}


static ngx_int_t
ngx_wasi_hfuncs_nop(ngx_wavm_instance_t *instance,
    wasm_val_t args[], wasm_val_t rets[])
{
    return NGX_WAVM_NYI;
}


static ngx_wavm_host_func_def_t  ngx_wasi_hfuncs[] = {

    { ngx_string("args_get"),
      &ngx_wasi_hfuncs_args_get,
      ngx_wavm_arity_i32x2,
      ngx_wavm_arity_i32 },

    { ngx_string("args_sizes_get"),
      &ngx_wasi_hfuncs_args_sizes_get,
      ngx_wavm_arity_i32x2,
      ngx_wavm_arity_i32 },

    { ngx_string("clock_time_get"),
      &ngx_wasi_hfuncs_clock_time_get,
      ngx_wavm_arity_i32_i64_i32,
      ngx_wavm_arity_i32 },

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
