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


/* vi:set ft=c ts=4 sw=4 et fdm=marker: */
