/*
 * Copyright (C) Thibault Charbonnier
 */

#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_http_wasm_util.h>


static ngx_str_t ngx_http_wasm_phases[] = {
    ngx_string("post_read"),
    ngx_string("server_rewrite"),
    ngx_string("find_config"),
    ngx_string("rewrite"),
    ngx_string("post_rewrite"),
    ngx_string("pre_access"),
    ngx_string("access"),
    ngx_string("post_access"),
    ngx_string("pre_content"),
    ngx_string("content"),
    ngx_string("log")
};


char *
ngx_http_wasm_conf_parse_phase(ngx_conf_t *cf, u_char *name,
    ngx_http_phases *phase)
{
    size_t   i;

    for (i = 0; i <= NGX_HTTP_LOG_PHASE; i++) {
        if (ngx_strncmp(name, ngx_http_wasm_phases[i].data,
                        ngx_http_wasm_phases[i].len) == 0)
        {
            *phase = i;
            return NGX_CONF_OK;
        }
    }

    ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "unknown phase \"%s\"",
                       name);

    return NGX_CONF_ERROR;
}
