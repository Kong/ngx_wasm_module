/*
 * Copyright (C) Thibault Charbonnier
 */

#ifndef _NGX_WASM_HOSTFUNCS_H_INCLUDED_
#define _NGX_WASM_HOSTFUNCS_H_INCLUDED_


#include <ngx_wasm.h>


#define ngx_wasm_hostfuncs_null  { ngx_null_string, NULL }


void ngx_wasm_hostfuncs_register(const char *namespace,
    ngx_wasm_hostfunc_t *hfuncs, ngx_log_t *log);

ngx_int_t ngx_wasm_hostfuncs_init();

ngx_wasm_hostfunc_pt ngx_wasm_hostfuncs_find(const char *namespace,
    size_t nlen, char *name, size_t len);

void ngx_wasm_hostfuncs_destroy();


#endif /* _NGX_WASM_HOSTFUNCS_H_INCLUDED_ */


/* vi:set ft=c ts=4 sw=4 et fdm=marker: */
