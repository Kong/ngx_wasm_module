/*
 * Copyright (C) Thibault Charbonnier
 */

#ifndef _NGX_WASM_WASMTIME_H_INCLUDED_
#define _NGX_WASM_WASMTIME_H_INCLUDED_

#include "ngx_wasm_common.h"

void ngx_wasmtime_log_error(ngx_uint_t level, ngx_log_t *log,
    wasmtime_error_t *werror, wasm_trap_t *wtrap, const char *fmt, ...);

ngx_int_t ngx_wasmtime_init_engine(ngx_wasm_conf_t *nwcf, ngx_log_t *log);
void ngx_wasmtime_shutdown_engine(ngx_wasm_conf_t *nwcf);

ngx_int_t ngx_wasmtime_load_module(wasm_module_t **wmodule,
    wasm_store_t *wstore, ngx_str_t path, ngx_log_t *log);

ngx_int_t ngx_wasmtime_load_instance(wasm_instance_t **winst,
    wasi_instance_t *wasi_inst, wasm_module_t *wmodule, ngx_log_t *log);

#endif /* _NGX_WASM_WASMTIME_H_INCLUDED */


/* vi:set ft=c ts=4 sw=4 et fdm=marker: */
