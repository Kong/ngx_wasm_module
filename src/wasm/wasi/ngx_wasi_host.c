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
    /**
     * TODO: nothing is returned for now
     * See ngx_wasi_hfuncs_environ_get for a sample implementation
     */

    rets[0] = (wasm_val_t) WASM_I32_VAL(WASI_ERRNO_SUCCESS);
    return NGX_WAVM_OK;
}


static ngx_int_t
ngx_wasi_hfuncs_args_sizes_get(ngx_wavm_instance_t *instance,
    wasm_val_t args[], wasm_val_t rets[])
{
    uint32_t  *args_size;
    uint32_t  *args_buf_size;

    /**
     * TODO: nothing is returned for now
     * See ngx_wasi_hfuncs_environ_get for more info on the format
     * of the arguments
     */

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
    uint32_t     clock_id;
    uint64_t     precision;
    void        *rtime;
    ngx_msec_t   t;

    clock_id = args[0].of.i32;
    precision = args[1].of.i64;
    /* WASM might not align 64-bit integers to 8-byte boundaries. So we
     * need to buffer & copy here. */
    rtime = NGX_WAVM_HOST_LIFT_SLICE(instance, args[2].of.i32,
                                     sizeof(uint64_t));

    /* precision is ignored for now, same as proxy-wasm-cpp-host */
    (void) precision;

    switch (clock_id) {
    case WASI_CLOCKID_REALTIME:
        ngx_wasm_wall_time(rtime);
        break;

    case WASI_CLOCKID_MONOTONIC:
        t = ngx_wasm_monotonic_time() * 1000000;
        ngx_memcpy(rtime, &t, sizeof(uint64_t));
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
ngx_wasi_hfuncs_errno_badf(ngx_wavm_instance_t *instance,
    wasm_val_t args[], wasm_val_t rets[])
{
    rets[0] = (wasm_val_t) WASM_I32_VAL(WASI_ERRNO_BADF);
    return NGX_WAVM_OK;
}


static ngx_int_t
ngx_wasi_hfuncs_errno_notdir(ngx_wavm_instance_t *instance,
    wasm_val_t args[], wasm_val_t rets[])
{
    rets[0] = (wasm_val_t) WASM_I32_VAL(WASI_ERRNO_NOTDIR);
    return NGX_WAVM_OK;
}


static ngx_int_t
ngx_wasi_hfuncs_errno_notsup(ngx_wavm_instance_t *instance,
    wasm_val_t args[], wasm_val_t rets[])
{
    rets[0] = (wasm_val_t) WASM_I32_VAL(WASI_ERRNO_NOTSUP);
    return NGX_WAVM_OK;
}


static ngx_int_t
ngx_wasi_hfuncs_fd_write(ngx_wavm_instance_t *instance,
    wasm_val_t args[], wasm_val_t rets[])
{
    uint32_t     fd;
    uint32_t     iovs;
    uint32_t     iovs_len;
    uint32_t    *nwritten;

    uint8_t     *buf;
    uint32_t     buf_ptr;
    uint32_t     buf_len;
    uint32_t     iovs_ptr;

    char        *msg_data;
    ngx_uint_t   msg_size;

    ngx_uint_t   level;
    ngx_uint_t   i;

    fd = args[0].of.i32;
    iovs = args[1].of.i32;
    iovs_len = args[2].of.i32;
    nwritten = NGX_WAVM_HOST_LIFT(instance, args[3].of.i32, uint32_t);

    /* translate file descriptors to log levels */

    switch (fd) {
    case 1:
        level = NGX_LOG_INFO;
        break;

    case 2:
        level = NGX_LOG_ERR;
        break;

    default:
        rets[0] = (wasm_val_t) WASM_I32_VAL(WASI_ERRNO_BADF);
        return NGX_WAVM_OK;
    }

    /* first pass: determine size of the string to be written */

    msg_size = 0;
    iovs_ptr = iovs;

    for (i = 0; i < iovs_len; i++) {
        iovs_ptr += sizeof(uint32_t);
        buf_len = *NGX_WAVM_HOST_LIFT(instance, iovs_ptr, uint32_t);

        iovs_ptr += sizeof(uint32_t);

        msg_size += buf_len;
    }

    if (msg_size == 0) {
        *nwritten = 0;

        rets[0] = (wasm_val_t) WASM_I32_VAL(WASI_ERRNO_SUCCESS);
        return NGX_WAVM_OK;
    }

    /* second pass: build string from iov buffers */

    msg_data = ngx_pcalloc(instance->pool, msg_size);
    if (msg_data == NULL) {
        rets[0] = (wasm_val_t) WASM_I32_VAL(WASI_ERRNO_EFAULT);
        return NGX_WAVM_OK;
    }

    msg_size = 0;
    iovs_ptr = iovs;

    for (i = 0; i < iovs_len; i++) {
        buf_ptr = *NGX_WAVM_HOST_LIFT(instance, iovs_ptr, uint32_t);
        iovs_ptr += sizeof(uint32_t);
        buf_len = *NGX_WAVM_HOST_LIFT(instance, iovs_ptr, uint32_t);

        iovs_ptr += sizeof(uint32_t);

        buf = NGX_WAVM_HOST_LIFT_SLICE(instance, buf_ptr, buf_len);

        ngx_memcpy(msg_data + msg_size, buf, buf_len);
        msg_size += buf_len;
    }

    *nwritten = msg_size;

    if (msg_size > 0) {
        if (msg_data[msg_size - 1] == '\n') {
            msg_size--;
        }

        ngx_log_error(level, instance->log_ctx.orig_log, 0, "%*s",
                      msg_size, msg_data);
    }

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

    { ngx_string("fd_close"),
      &ngx_wasi_hfuncs_errno_badf,
      ngx_wavm_arity_i32,
      ngx_wavm_arity_i32 },

    { ngx_string("fd_fdstat_get"),
      &ngx_wasi_hfuncs_errno_badf,
      ngx_wavm_arity_i32x2,
      ngx_wavm_arity_i32 },

    { ngx_string("fd_prestat_get"),
      &ngx_wasi_hfuncs_errno_badf,
      ngx_wavm_arity_i32x2,
      ngx_wavm_arity_i32 },

    { ngx_string("fd_prestat_dir_name"),
      &ngx_wasi_hfuncs_errno_notsup,
      ngx_wavm_arity_i32x3,
      ngx_wavm_arity_i32 },

    { ngx_string("fd_read"),
      &ngx_wasi_hfuncs_errno_badf,
      ngx_wavm_arity_i32x4,
      ngx_wavm_arity_i32 },

    { ngx_string("fd_seek"),
      &ngx_wasi_hfuncs_errno_badf,
      ngx_wavm_arity_i32_i64_i32x2,
      ngx_wavm_arity_i32 },

    { ngx_string("fd_write"),
      &ngx_wasi_hfuncs_fd_write,
      ngx_wavm_arity_i32x4,
      ngx_wavm_arity_i32 },

    { ngx_string("path_open"),
      &ngx_wasi_hfuncs_errno_notdir,
      ngx_wavm_arity_i32x5_i64x2_i32x2,
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
