#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_wasm.h>
#include <ngx_event.h>


ngx_uint_t
ngx_wasm_list_nelts(ngx_list_t *list)
{
    ngx_uint_t        c;
    ngx_list_part_t  *part;

    part = &list->part;
    c = part->nelts;

    while (part->next != NULL) {
        part = part->next;
        c += part->nelts;
    }

    return c;
}


ngx_int_t
ngx_wasm_bytes_from_path(wasm_byte_vec_t *out, u_char *path, ngx_log_t *log)
{
    ssize_t      n, fsize;
    u_char      *file_bytes = NULL;
    ngx_fd_t     fd;
    ngx_file_t   file;
    ngx_int_t    rc = NGX_ERROR;

    fd = ngx_open_file(path, NGX_FILE_RDONLY, NGX_FILE_OPEN, 0);
    if (fd == NGX_INVALID_FILE) {
        ngx_wasm_log_error(NGX_LOG_EMERG, log, ngx_errno,
                           ngx_open_file_n " \"%s\" failed",
                           path);
        return NGX_ERROR;
    }

    ngx_memzero(&file, sizeof(ngx_file_t));

    file.fd = fd;
    file.log = log;
    file.name.len = ngx_strlen(path);;
    file.name.data = path;

    if (ngx_fd_info(fd, &file.info) == NGX_FILE_ERROR) {
        ngx_wasm_log_error(NGX_LOG_EMERG, log, ngx_errno,
                           ngx_fd_info_n " \"%V\" failed", &file.name);
        goto close;
    }

    fsize = ngx_file_size(&file.info);

    file_bytes = ngx_alloc(fsize, log);
    if (file_bytes == NULL) {
        ngx_wasm_log_error(NGX_LOG_EMERG, log, 0,
                           "failed to allocate file_bytes for \"%V\"",
                           path);
        goto close;
    }

    n = ngx_read_file(&file, file_bytes, fsize, 0);
    if (n == NGX_ERROR) {
        ngx_wasm_log_error(NGX_LOG_EMERG, log, ngx_errno,
                           ngx_read_file_n " \"%V\" failed",
                           &file.name);
        goto close;
    }

    if (n != fsize) {
        ngx_wasm_log_error(NGX_LOG_EMERG, log, 0,
                           ngx_read_file_n " \"%V\" returned only "
                           "%z file_bytes instead of %uiz", &file.name,
                           n, fsize);
        goto close;
    }

    wasm_byte_vec_new(out, fsize, (const char *) file_bytes);

    rc = NGX_OK;

close:

    if (ngx_close_file(fd) == NGX_FILE_ERROR) {
        ngx_wasm_log_error(NGX_LOG_ERR, log, ngx_errno,
                           ngx_close_file_n " \"%V\" failed", &file.name);
    }

    if (file_bytes) {
        ngx_free(file_bytes);
    }

    return rc;
}
void
ngx_wasm_log_error(ngx_uint_t level, ngx_log_t *log, ngx_err_t err,
    const char *fmt, ...)
{
    va_list   args;
    u_char   *p, errstr[NGX_MAX_ERROR_STR];

    va_start(args, fmt);
    p = ngx_vsnprintf(errstr, NGX_MAX_ERROR_STR, fmt, args);
    va_end(args);

    ngx_log_error_core(level, log, err, "[wasm] %*s", p - errstr, errstr);
}


#if 0
ngx_connection_t *
ngx_wasm_connection_create(ngx_pool_t *pool)
{
    ngx_log_t         *log;
    ngx_connection_t  *c, *orig = NULL;

    if (ngx_cycle->files) {
        orig = ngx_cycle->files[0];
    }

    c = ngx_get_connection(0, ngx_cycle->log);

    if (orig) {
        ngx_cycle->files[0] = orig;
    }

    if (c == NULL) {
        return NULL;
    }

    c->fd = NGX_WASM_BAD_FD;
    c->number = ngx_atomic_fetch_add(ngx_connection_counter, 1);
    c->pool = pool;

    log = ngx_pcalloc(c->pool, sizeof(ngx_log_t));
    if (log == NULL) {
        goto failed;
    }

    c->log = log;
    c->log_error = NGX_ERROR_INFO;
    c->log->connection = c->number;
    c->log->action = NULL;
    c->log->data = NULL;
    c->error = 1;

    return c;

failed:

    ngx_wasm_connection_destroy(c);
    return NULL;
}


void
ngx_wasm_connection_destroy(ngx_connection_t *c)
{
    ngx_connection_t  *orig = NULL;

    if (c->read->timer_set) {
        ngx_del_timer(c->read);
    }

    if (c->write->timer_set) {
        ngx_del_timer(c->write);
    }

    c->read->closed = 1;
    c->write->closed = 1;
    c->destroyed = 1;

    if (ngx_cycle->files) {
        orig = ngx_cycle->files[0];
    }

    c->fd = 0;
    ngx_free_connection(c);

    if (orig) {
        ngx_cycle->files[0] = orig;
    }
}
#endif
