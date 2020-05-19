/*
 * Copyright (C) Thibault Charbonnier
 */

#include "ngx_wasm_wasmtime.h"


void
ngx_wasmtime_log_error(ngx_uint_t level, ngx_log_t *log,
    wasmtime_error_t *werror, wasm_trap_t *wtrap, const char *fmt, ...)
{
    va_list                      args;
    char                        *wfmt;
    wasm_byte_vec_t              werror_msg;

    ngx_wasm_assert(werror != NULL || wtrap != NULL);

    if (werror != NULL) {
        wasmtime_error_message(werror, &werror_msg);
        wasmtime_error_delete(werror);

    } else {
        wasm_trap_message(wtrap, &werror_msg);
        wasm_trap_delete(wtrap);
    }

    wfmt = strcat(" (%.*s)", fmt);

    va_start(args, fmt);
    ngx_log_error(level, log, 0, wfmt,
                  args, werror_msg.size, werror_msg.data);
    va_end(args);

    wasm_byte_vec_delete(&werror_msg);
}


ngx_int_t
ngx_wasmtime_init_engine(ngx_wasm_conf_t *nwcf, ngx_log_t *log)
{
    wasm_trap_t                 *wtrap = NULL;

    nwcf->wengine = wasm_engine_new();
    if (nwcf->wengine == NULL) {
        ngx_wasmtime_log_error(NGX_LOG_ALERT, log, NULL, NULL,
                               "failed to instantiate WASM engine");
        goto failed;
    }

    nwcf->wstore = wasm_store_new(nwcf->wengine);
    if (nwcf->wstore == NULL) {
        ngx_wasmtime_log_error(NGX_LOG_ALERT, log, NULL, NULL,
                               "failed to instantiate WASM store");
        goto failed;
    }

    nwcf->wasi_config = wasi_config_new();
    if (nwcf->wasi_config == NULL) {
        ngx_wasmtime_log_error(NGX_LOG_ALERT, log, NULL, NULL,
                               "failed to instantiate WASI config");
        goto failed;
    }

    wasi_config_inherit_stdout(nwcf->wasi_config);
    wasi_config_inherit_stderr(nwcf->wasi_config);

    nwcf->wasi_inst = wasi_instance_new(nwcf->wstore, "wasi_snapshot_preview1",
                                        nwcf->wasi_config, &wtrap);
    if (nwcf->wasi_inst == NULL) {
        ngx_wasmtime_log_error(NGX_LOG_ALERT, log, NULL, wtrap,
                               "failed to instantiate WASI instance");
        goto failed;
    }

    return NGX_OK;

failed:

    ngx_wasmtime_shutdown_engine(nwcf);

    return NGX_ERROR;
}


void
ngx_wasmtime_shutdown_engine(ngx_wasm_conf_t *nwcf)
{
    /* wasi_inst? */
    /* wasi_config? */

    if (nwcf->wstore != NULL) {
        wasm_store_delete(nwcf->wstore);
    }

    if (nwcf->wengine != NULL) {
        wasm_engine_delete(nwcf->wengine);
    }
}


ngx_int_t
ngx_wasmtime_load_module(wasm_module_t **wmodule, wasm_store_t *wstore,
    ngx_str_t path, ngx_log_t *log)
{
    ssize_t                      fsize, n;
    ngx_fd_t                     fd;
    ngx_file_t                   file;
    wasm_byte_vec_t              wbytes;
    wasmtime_error_t            *werror;
    ngx_int_t                    rc = NGX_ERROR;

    fd = ngx_open_file(path.data, NGX_FILE_RDONLY, NGX_FILE_OPEN, 0);
    if (fd == NGX_INVALID_FILE) {
        ngx_log_error(NGX_LOG_ALERT, log, ngx_errno,
                      ngx_open_file_n " \"%s\" failed", path.data);
        return NGX_ERROR;
    }

    ngx_memzero(&file, sizeof(ngx_file_t));

    file.fd = fd;
    file.log = log;
    file.name.len = path.len;
    file.name.data = path.data;

    if (ngx_fd_info(fd, &file.info) == NGX_FILE_ERROR) {
        ngx_log_error(NGX_LOG_ALERT, log, ngx_errno,
                      ngx_fd_info_n " \"%s\" failed", &file.name);
        goto failed;
    }

    fsize = ngx_file_size(&file.info);
    wasm_byte_vec_new_uninitialized(&wbytes, fsize);

    n = ngx_read_file(&file, (u_char *) wbytes.data, fsize, 0);
    if (n == NGX_ERROR) {
        ngx_log_error(NGX_LOG_ALERT, log, ngx_errno,
                      ngx_read_file_n " \"%V\" failed", &file.name);
        goto failed;
    }

    if (n != fsize) {
        ngx_log_error(NGX_LOG_ALERT, log, 0,
                      ngx_read_file_n " \"%V\" returned only "
                      "%z bytes instead of %uz", &file.name, n, fsize);
        goto failed;
    }

    werror = wasmtime_module_new(wstore, &wbytes, wmodule);
    if (wmodule == NULL) {
        ngx_wasmtime_log_error(NGX_LOG_ALERT, log, werror, NULL,
                               "failed to compile module \"%V\"", &file.name);
        goto failed;
    }

    rc = NGX_OK;

failed:

    if (ngx_close_file(file.fd) == NGX_FILE_ERROR) {
        ngx_log_error(NGX_LOG_ALERT, log, ngx_errno,
                      ngx_close_file_n " \"%V\" failed", &file.name);
    }

    wasm_byte_vec_delete(&wbytes);

    return rc;
}


ngx_int_t
ngx_wasmtime_load_instance(wasm_instance_t **winst,
    wasi_instance_t *wasi_inst, wasm_module_t *wmodule, ngx_log_t *log)
{
    const wasm_extern_t        **wexterns;
    wasm_importtype_vec_t        wimports;
    wasmtime_error_t            *werror;
    wasm_trap_t                 *wtrap = NULL;
    ngx_int_t                    rc = NGX_ERROR;

    wasm_module_imports(wmodule, &wimports);

    wexterns = calloc(wimports.size, sizeof(void*));
    if (wexterns == NULL) {
        ngx_wasmtime_log_error(NGX_LOG_ALERT, log, NULL, NULL,
                               "failed to instantiate WASM externs");
        goto failed;
    }

    for (size_t i = 0; i < wimports.size; i++) {
        wexterns[i] = wasi_instance_bind_import(wasi_inst, wimports.data[i]);
        if (wexterns == NULL) {
            ngx_wasmtime_log_error(NGX_LOG_ALERT, log, NULL, NULL,
                                   "failed to satisfy WASM import");
            goto failed;
        }
    }

    werror = wasmtime_instance_new(wmodule, wexterns, wimports.size,
                                   winst, &wtrap);
    if (winst == NULL) {
        ngx_wasmtime_log_error(NGX_LOG_ALERT, log, werror, wtrap,
                               "failed to instanciate WASM instance");
        goto failed;
    }

    rc = NGX_OK;

failed:

    wasm_importtype_vec_delete(&wimports);

    if (wexterns != NULL) {
        free(wexterns);
    }

    return rc;
}


/* vi:set ft=c ts=4 sw=4 et fdm=marker: */
