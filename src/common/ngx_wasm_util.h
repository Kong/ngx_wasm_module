/*
 * Copyright (C) Thibault Charbonnier
 */

#ifndef _NGX_WASM_UTIL_H_INCLUDED_
#define _NGX_WASM_UTIL_H_INCLUDED_


#include <wasm.h>


ngx_int_t ngx_wasm_read_file(wasm_byte_vec_t *wbytes,
    const u_char *path, ngx_log_t *log);


void ngx_wasm_log_error(ngx_uint_t level, ngx_log_t *log, ngx_err_t err,
#if (NGX_HAVE_VARIADIC_MACROS)
    const char *fmt, ...);

#else
    const char *fmt, va_list args);
#endif


void ngx_wasm_log_trap(ngx_uint_t level, ngx_log_t *log, wasm_trap_t *wtrap,
#if (NGX_HAVE_VARIADIC_MACROS)
    const char *fmt, ...);

#else
    const char *fmt, va_list args);
#endif


#endif /* _NGX_WASM_UTIL_H_INCLUDED_ */


/* vi:set ft=c ts=4 sw=4 et fdm=marker: */
