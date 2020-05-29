/*
 * Copyright (C) Thibault Charbonnier
 */

#ifndef _NGX_WASM_UTIL_H_INCLUDED_
#define _NGX_WASM_UTIL_H_INCLUDED_


#include <ngx_wasm.h>
#include <wasm.h>


ngx_int_t ngx_wasm_read_file(wasm_byte_vec_t *wbytes,
                             const u_char *path, ngx_log_t *log);


#endif /* _NGX_WASM_UTIL_H_INCLUDED_ */


/* vi:set ft=c ts=4 sw=4 et fdm=marker: */
