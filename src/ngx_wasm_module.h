/*
 * Copyright (C) Thibault Charbonnier
 */


#ifndef _NGX_WASM_H_INCLUDED_
#define _NGX_WASM_H_INCLUDED_


#include <wasm.h>
#include <wasi.h>
#include <wasmtime.h>


#define NGX_WASM_MODULE      0x5741534d   /* "WASM" */
#define NGX_WASM_CONF        0x02000000


#define ngx_wasm_conf_get_main_conf(cf)                                      \
    ((ngx_wasm_conf_t *) ngx_get_conf(cf->cycle->conf_ctx, ngx_wasm_module))

#define ngx_wasm_cycle_get_main_conf(cycle)                                  \
    ((ngx_wasm_conf_t *) ngx_get_conf(cycle->conf_ctx, ngx_wasm_module))


typedef struct {
    unsigned                     parsed_wasm_block:1;

    wasm_engine_t               *wasm_engine;
    wasm_store_t                *wasm_store;
    wasi_config_t               *wasi_config;
} ngx_wasm_conf_t;


#endif /* _NGX_WASM_H_INCLUDED_ */


/* vi:set ft=c ts=4 sw=4 et fdm=marker: */
