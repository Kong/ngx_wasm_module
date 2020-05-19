/*
 * Copyright (C) Thibault Charbonnier
 */

#ifndef _NGX_WASM_COMMON_H_INCLUDED_
#define _NGX_WASM_COMMON_H_INCLUDED_

#include <nginx.h>
#include <ngx_core.h>
#include <wasm.h>
#include <wasi.h>
#include <wasmtime.h>


#ifdef NGX_WASM_USE_ASSERT
#include <assert.h>
#   define ngx_wasm_assert(a)  assert(a)
#else
#   define ngx_wasm_assert(a)
#endif


#define ngx_wasm_conf_get_main_conf(cf)                                      \
    ((ngx_wasm_conf_t *) ngx_get_conf(cf->cycle->conf_ctx, ngx_wasm_module))

#define ngx_wasm_cycle_get_main_conf(cycle)                                  \
    ((ngx_wasm_conf_t *) ngx_get_conf(cycle->conf_ctx, ngx_wasm_module))


#if (NGX_HAVE_VARIADIC_MACROS)
#   define ngx_wasm_log_error(level, log, err, fmt, ...)                     \
        ngx_log_error(level, log, err, "[wasm] " fmt, __VA_ARGS__);

#else
#   error "NYI: ngx_wasm_module requires C99 variadic macros"
#endif


typedef struct {
    unsigned                     parsed_wasm_block:1;

    wasm_engine_t               *wengine;
    wasm_store_t                *wstore;
    wasi_config_t               *wasi_config;
    wasi_instance_t             *wasi_inst;
} ngx_wasm_conf_t;

#endif /* _NGX_WASM_COMMON_H_INCLUDED_ */


/* vi:set ft=c ts=4 sw=4 et fdm=marker: */
