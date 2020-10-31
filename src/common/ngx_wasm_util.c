/*
 * Copyright (C) Thibault Charbonnier
 */

#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_wasm_util.h>
#include <ngx_http.h>


ngx_int_t
ngx_wasm_conf_parse_phase(ngx_conf_t *cf, u_char *str, ngx_uint_t module_type)
{
    size_t             i;
    ngx_str_t         *phases;
    static ngx_str_t   http_phases[] = {
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
        ngx_string("log"),
        ngx_null_string
    };

    if (ngx_strlen(str) == 0) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "invalid phase \"%s\"", str);
        goto unset;
    }

    switch (module_type) {

    case NGX_HTTP_MODULE:
        phases = http_phases;
        break;

    default:
        ngx_conf_log_error(NGX_LOG_ALERT, cf, 0, "unreachable");
        ngx_wasm_assert(0);
        goto fail;

    }

    for (i = 0; phases->len; phases++, i++) {
        if (ngx_strncmp(str, phases->data, phases->len) == 0) {
            return i;
        }
    }

fail:

    ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "unknown phase \"%s\"", str);

unset:

    return NGX_CONF_UNSET_UINT;
}


void
ngx_wasm_log_error(ngx_uint_t level, ngx_log_t *log, ngx_err_t err,
#if (NGX_HAVE_VARIADIC_MACROS)
    const char *fmt, ...)

#else
    const char *fmt, va_list args)
#endif
{
    u_char  *p;
    u_char   errstr[NGX_MAX_ERROR_STR];

#if (NGX_HAVE_VARIADIC_MACROS)
    va_list  args;

    va_start(args, fmt);
    p = ngx_vsnprintf(errstr, NGX_MAX_ERROR_STR, fmt, args);
    va_end(args);

#else
    p = ngx_vsnprintf(errstr, NGX_MAX_ERROR_STR, fmt, args);
#endif

    ngx_log_error_core(level, log, err, "[wasm] %*s", p - errstr, errstr);
}
