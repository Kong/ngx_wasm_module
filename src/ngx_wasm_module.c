/*
 * Copyright (C) Thibault Charbonnier
 */

#include <nginx.h>
#include <ngx_core.h>
#include <ngx_config.h>

#include "ngx_wasm_module.h"


static void *ngx_wasm_module_create_conf(ngx_cycle_t *cycle);
static char *ngx_wasm_wasm_block(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);

static ngx_int_t ngx_wasm_module_init(ngx_cycle_t *cycle);


static ngx_command_t ngx_wasm_cmds[] = {

    { ngx_string("wasm"),
      NGX_MAIN_CONF|NGX_CONF_BLOCK|NGX_CONF_NOARGS,
      ngx_wasm_wasm_block,
      0,
      0,
      NULL },

    ngx_null_command
};


static ngx_core_module_t  ngx_wasm_module_ctx = {
    ngx_string("wasm"),
    ngx_wasm_module_create_conf,
    NULL
};


ngx_module_t  ngx_wasm_module = {
    NGX_MODULE_V1,
    &ngx_wasm_module_ctx,              /* module context */
    ngx_wasm_cmds,                     /* module directives */
    NGX_CORE_MODULE,                   /* module type */
    NULL,                              /* init master */
    ngx_wasm_module_init,              /* init module */
    NULL,                              /* init process */
    NULL,                              /* init thread */
    NULL,                              /* exit thread */
    NULL,                              /* exit process */
    NULL,                              /* exit master */
    NGX_MODULE_V1_PADDING
};


static void *
ngx_wasm_module_create_conf(ngx_cycle_t *cycle)
{
    ngx_wasm_conf_t         *nwcf;

    nwcf = ngx_pcalloc(cycle->pool, sizeof(ngx_wasm_conf_t));
    if (nwcf == NULL) {
        return NULL;
    }

    return nwcf;
}


static char *
ngx_wasm_wasm_block(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    char                        *rv;
    ngx_conf_t                   pcf;
    ngx_wasm_conf_t             *nwcf = *(ngx_wasm_conf_t **) conf;

    if (nwcf->parsed_wasm_block) {
        return "is duplicate";
    }

    /* parse the wasm{} block */

    nwcf->parsed_wasm_block = 1;

    pcf = *cf;

    cf->ctx = nwcf;
    cf->module_type = NGX_CORE_MODULE;
    cf->cmd_type = NGX_WASM_CONF;

    rv = ngx_conf_parse(cf, NULL);

    *cf = pcf;

    if (rv != NGX_CONF_OK) {
        return NGX_CONF_ERROR;
    }

    return NGX_CONF_OK;
}


static ngx_int_t
ngx_wasm_module_init(ngx_cycle_t *cycle)
{
    ngx_wasm_conf_t             *nwcf;
    ssize_t                      file_size, n;
    ngx_file_t                   file;
    ngx_fd_t                     fd;
    ngx_str_t                    filename = ngx_string("/home/chasum/code/wasm/hello-world/target/wasm32-wasi/debug/hello-world.wasm");

    wasm_byte_vec_t              wasm_bytes, wasm_error_msg;
    wasm_trap_t                 *wasm_trap = NULL;
    wasmtime_error_t            *wasm_error = NULL;

    wasm_module_t               *wasm_module;

    wasi_instance_t             *wasi_inst;

    const wasm_extern_t        **wasm_imports = NULL;
    wasm_importtype_vec_t        wasm_import_types;
    wasm_instance_t             *wasm_instance;

    wasm_extern_vec_t            wasm_externs;
    wasm_exporttype_vec_t        wasm_exports;
    wasm_extern_t               *start_extern = NULL;
    wasm_func_t                 *start = NULL;

    nwcf = ngx_wasm_cycle_get_main_conf(cycle);

    /* reading wasm file */

    fd = ngx_open_file(filename.data, NGX_FILE_RDONLY, NGX_FILE_OPEN, 0);
    if (fd == NGX_INVALID_FILE) {
        ngx_log_error(NGX_LOG_ALERT, cycle->log, ngx_errno,
                      ngx_open_file_n " \"%s\" failed", filename.data);
        return NGX_ERROR;
    }

    ngx_memzero(&file, sizeof(ngx_file_t));

    file.fd = fd;
    file.log = cycle->log;
    file.name.len = filename.len;
    file.name.data = filename.data;

    if (ngx_fd_info(fd, &file.info) == NGX_FILE_ERROR) {
        ngx_log_error(NGX_LOG_ALERT, cycle->log, ngx_errno,
                      ngx_fd_info_n " \"%s\" failed", &file.name);
        goto failed;
    }

    file_size = ngx_file_size(&file.info);

    wasm_byte_vec_new_uninitialized(&wasm_bytes, file_size);

    n = ngx_read_file(&file, (u_char *) wasm_bytes.data, file_size, 0);
    if (n == NGX_ERROR) {
        ngx_log_error(NGX_LOG_ALERT, cycle->log, ngx_errno,
                      ngx_read_file_n " \"%V\" failed", &file.name);
        goto failed;
    }

    if (n != file_size) {
        ngx_log_error(NGX_LOG_ALERT, cycle->log, 0,
                      ngx_read_file_n " \"%V\" returned only "
                      "%z bytes instead of %uz", &file.name, n, file_size);
        goto failed;
    }

    /* wasm engine */

    nwcf->wasm_engine = wasm_engine_new();
    if (nwcf->wasm_engine == NULL) {
        ngx_log_error(NGX_LOG_ALERT, cycle->log, 0,
                      "failed to instanciate wasm engine");
        goto failed;
    }

    /* wasm store */

    nwcf->wasm_store = wasm_store_new(nwcf->wasm_engine);
    if (nwcf->wasm_store == NULL) {
        ngx_log_error(NGX_LOG_ALERT, cycle->log, 0,
                      "failed to instanciate wasm store");
        goto failed;
    }

    /* wasm module */

    wasm_error = wasmtime_module_new(nwcf->wasm_store, &wasm_bytes,
                                     &wasm_module);
    if (wasm_module == NULL) {
        wasmtime_error_message(wasm_error, &wasm_error_msg);
        wasmtime_error_delete(wasm_error);

        ngx_log_error(NGX_LOG_ALERT, cycle->log, 0,
                      "\"%V\" %s", &file.name, &wasm_error_msg);
        goto failed;
    }

    /* wasi config */

    nwcf->wasi_config = wasi_config_new();
    if (nwcf->wasi_config == NULL) {
        ngx_log_error(NGX_LOG_ALERT, cycle->log, 0,
                      "failed to instanciate wasi config");
        goto failed;
    }

    wasi_config_inherit_stdout(nwcf->wasi_config);
    wasi_config_inherit_stderr(nwcf->wasi_config);

    /* wasi instance */

    wasi_inst = wasi_instance_new(nwcf->wasm_store, "wasi_snapshot_preview1",
                                  nwcf->wasi_config, &wasm_trap);
    if (wasi_inst == NULL) {
        ngx_log_error(NGX_LOG_ALERT, cycle->log, 0,
                      "failed to instanciate wasi instance");
        goto failed;
    }

    /* imports */

    wasm_module_imports(wasm_module, &wasm_import_types);

    wasm_imports = calloc(wasm_import_types.size, sizeof(void*));
    if (wasm_imports == NULL) {
        ngx_log_error(NGX_LOG_ALERT, cycle->log, 0, "failed importing wasm");
        goto failed;
    }

    for (size_t i = 0; i < wasm_import_types.size; i++) {
        const wasm_extern_t *binding = wasi_instance_bind_import(wasi_inst,
                                           wasm_import_types.data[i]);
        if (binding == NULL) {
            ngx_log_error(NGX_LOG_ALERT, cycle->log, 0,
                          "failed to satisfy import");
            goto failed;
        }

        wasm_imports[i] = binding;
    }

    /* module instance */

    wasm_error = wasmtime_instance_new(wasm_module, wasm_imports,
                                       wasm_import_types.size,
                                       &wasm_instance, &wasm_trap);
    if (wasm_instance == NULL) {
        ngx_log_error(NGX_LOG_ALERT, cycle->log, 0,
                      "failed to instanciate wasm instance");
        goto failed;
    }

    free(wasm_imports);
    wasm_importtype_vec_delete(&wasm_import_types);

    /* load & run _start function */

    wasm_instance_exports(wasm_instance, &wasm_externs);
    wasm_module_exports(wasm_module, &wasm_exports);

    for (size_t i = 0; i < wasm_exports.size; i++) {
        const wasm_name_t *name = wasm_exporttype_name(wasm_exports.data[i]);
        if (ngx_strncmp(name->data, "_start", name->size) == 0) {
            start_extern = wasm_externs.data[i];
        }
    }

    if (start_extern == NULL) {
        ngx_log_error(NGX_LOG_ALERT, cycle->log, 0,
                      "failed to find _start function");
        goto failed;
    }

    start = wasm_extern_as_func(start_extern);
    if (start == NULL) {
        ngx_log_error(NGX_LOG_ALERT, cycle->log, 0,
                      "failed to load _start function");
        goto failed;
    }

    wasm_error = wasmtime_func_call(start, NULL, 0, NULL, 0, &wasm_trap);
    if (wasm_error != NULL || wasm_trap != NULL) {
        if (wasm_error != NULL) {
            wasmtime_error_message(wasm_error, &wasm_error_msg);
            wasmtime_error_delete(wasm_error);

        } else {
            assert(wasm_trap != NULL);
            wasm_trap_message(wasm_trap, &wasm_error_msg);
            wasm_trap_delete(wasm_trap);
        }

        ngx_log_error(NGX_LOG_ALERT, cycle->log, 0,
                      "failed to call _start function: %s",
                      &file.name, &wasm_error_msg);
        goto failed;
    }

    return NGX_OK;

failed:

    wasm_importtype_vec_delete(&wasm_import_types);
    wasm_byte_vec_delete(&wasm_error_msg);

    if (start != NULL) {
        //free(start);
        wasm_func_delete(start);
    }

    if (start_extern != NULL) {
        //free(start_extern);
        //wasm_extern_delete(start_extern);
    }

    if (wasm_imports != NULL) {
        free(wasm_imports);
    }

    /* wasi inst? */

    /* wasi config? */

    if (wasm_module != NULL) {
        wasm_module_delete(wasm_module);
    }

    if (nwcf->wasm_store != NULL) {
        wasm_store_delete(nwcf->wasm_store);
    }

    if (nwcf->wasm_engine != NULL) {
        wasm_engine_delete(nwcf->wasm_engine);
    }

    if (ngx_close_file(file.fd) == NGX_FILE_ERROR) {
        ngx_log_error(NGX_LOG_ALERT, cycle->log, ngx_errno,
                      ngx_close_file_n " \"%V\" failed", &file.name);
    }

    return NGX_ERROR;
}


/* vi:set ft=c ts=4 sw=4 et fdm=marker: */
