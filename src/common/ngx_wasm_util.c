/*
 * Copyright (C) Thibault Charbonnier
 */

#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_wasm_util.h>


ngx_int_t
ngx_wasm_read_file(wasm_byte_vec_t *wbytes, const u_char *path,
    ngx_log_t *log)
{
    ssize_t                      fsize, n;
    ngx_fd_t                     fd;
    ngx_file_t                   file;
    ngx_int_t                    rc = NGX_ERROR;

    fd = ngx_open_file(path, NGX_FILE_RDONLY, NGX_FILE_OPEN, 0);
    if (fd == NGX_INVALID_FILE) {
        ngx_log_error(NGX_LOG_ERR, log, ngx_errno,
                      ngx_open_file_n " \"%s\" failed", path);
        return NGX_ERROR;
    }

    ngx_memzero(&file, sizeof(ngx_file_t));

    file.fd = fd;
    file.log = log;
    file.name.len = ngx_strlen(path);
    file.name.data = (u_char *) path;

    if (ngx_fd_info(fd, &file.info) == NGX_FILE_ERROR) {
        ngx_log_error(NGX_LOG_ERR, log, ngx_errno,
                      ngx_fd_info_n " \"%s\" failed", &file.name);
        goto failed;
    }

    fsize = ngx_file_size(&file.info);
    wasm_byte_vec_new_uninitialized(wbytes, fsize);

    n = ngx_read_file(&file, (u_char *) wbytes->data, fsize, 0);
    if (n == NGX_ERROR) {
        ngx_log_error(NGX_LOG_ERR, log, ngx_errno,
                      ngx_read_file_n " \"%V\" failed", &file.name);
        goto failed;
    }

    if (n != fsize) {
        ngx_log_error(NGX_LOG_ERR, log, 0,
                      ngx_read_file_n " \"%V\" returned only "
                      "%z bytes instead of %uz", &file.name, n, fsize);
        goto failed;
    }

    rc = NGX_OK;

failed:

    if (ngx_close_file(file.fd) == NGX_FILE_ERROR) {
        ngx_log_error(NGX_LOG_ERR, log, ngx_errno,
                      ngx_close_file_n " \"%V\" failed", &file.name);
    }

    return rc;
}


void
ngx_wasm_log_error(ngx_uint_t level, ngx_log_t *log, ngx_err_t err,
#if (NGX_HAVE_VARIADIC_MACROS)
    const char *fmt, ...)

#else
    const char *fmt, va_list args)
#endif
{
#if (NGX_HAVE_VARIADIC_MACROS)
    va_list                      args;
#endif
    u_char                      *p, *last;
    u_char                       errstr[NGX_MAX_ERROR_STR];

    last = errstr + NGX_MAX_ERROR_STR;
    p = &errstr[0];

#if (NGX_HAVE_VARIADIC_MACROS)
    va_start(args, fmt);
    p = ngx_vslprintf(p, last, fmt, args);
    va_end(args);

#else
    p = ngx_vslprintf(p, last, fmt, args);
#endif

    ngx_log_error_core(level, log, err, "[wasm] %*s", p - errstr, errstr);
}


void
ngx_wasm_log_trap(ngx_uint_t level, ngx_log_t *log, wasm_trap_t *wtrap,
#if (NGX_HAVE_VARIADIC_MACROS)
    const char *fmt, ...)

#else
    const char *fmt, va_list args)
#endif
{
#if (NGX_HAVE_VARIADIC_MACROS)
    va_list                      args;
#endif
    u_char                      *p, *last;
    u_char                       errstr[NGX_MAX_ERROR_STR];
    wasm_byte_vec_t              werror_msg;

    ngx_memzero(&werror_msg, sizeof(wasm_byte_vec_t));

    if (wtrap != NULL) {
        wasm_trap_message(wtrap, &werror_msg);
        wasm_trap_delete(wtrap);
    }

    last = errstr + NGX_MAX_ERROR_STR;
    p = &errstr[0];

#if (NGX_HAVE_VARIADIC_MACROS)
    va_start(args, fmt);
    p = ngx_vslprintf(p, last, fmt, args);
    va_end(args);

#else
    p = ngx_vslprintf(p, last, fmt, args);
#endif

    if (werror_msg.size) {
        ngx_wasm_log_error(level, log, 0, "%*s (wasm trap: %*s)",
                           p - errstr, errstr,
                           werror_msg.size, werror_msg.data);

        wasm_byte_vec_delete(&werror_msg);

    } else {
        ngx_wasm_log_error(level, log, 0, "%*s (no trap)",
                           p - errstr, errstr);
    }
}


/* vi:set ft=c ts=4 sw=4 et fdm=marker: */
