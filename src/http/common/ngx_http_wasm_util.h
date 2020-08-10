/*
 * Copyright (C) Thibault Charbonnier
 */

#ifndef _NGX_HTTP_WASM_UTIL_H_INCLUDED_
#define _NGX_HTTP_WASM_UTIL_H_INCLUDED_


#include <ngx_core.h>
#include <ngx_http.h>


char *ngx_http_wasm_conf_parse_phase(ngx_conf_t *cf, u_char *name,
    ngx_http_phases *phase);


#endif /* _NGX_HTTP_WASM_UTIL_H_INCLUDED_ */
