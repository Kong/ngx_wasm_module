/*
 * Copyright (C) Thibault Charbonnier
 */

#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_http_wasm_hostfuncs.h>


ngx_int_t
ngx_wasm_host_log(ngx_wasm_hctx_t *hctx, const ngx_wasm_val_t args[],
    ngx_wasm_val_t rets[])
{
    int32_t  level, len, msg_offset;

    level = args[0].value.I32;
    msg_offset = args[1].value.I32;
    len = args[2].value.I32;

    ngx_log_error((ngx_uint_t) level, hctx->log, 0, "%*s",
                  len, hctx->memory_offset + msg_offset);

    return NGX_OK;
}


ngx_int_t
ngx_wasm_host_resp_get_status(ngx_wasm_hctx_t *hctx, const ngx_wasm_val_t args[],
    ngx_wasm_val_t rets[])
{
    ngx_http_request_t  *r = hctx->data.r;
    ngx_int_t            status;

    if (r->connection->fd == (ngx_socket_t) -1) {
        return NGX_DECLINED;
    }

    if (r->err_status) {
        status = r->err_status;

    } else if (r->headers_out.status) {
        status = r->headers_out.status;

    } else if (r->http_version == NGX_HTTP_VERSION_9) {
        status = 9;

    } else {
        status = 0;
    }

    rets[0].kind = NGX_WASM_I32;
    rets[0].value.I32 = status;

    return NGX_OK;
}


ngx_wasm_hfunc_decl_t  ngx_http_wasm_hfuncs[] = {

    { ngx_string("ngx_wasm_log"),
      &ngx_wasm_host_log,
      NGX_WASM_ARGS_I32_I32_I32,
      NGX_WASM_RETS_NONE },

    { ngx_string("ngx_wasm_resp_get_status"),
      &ngx_wasm_host_resp_get_status,
      NGX_WASM_ARGS_NONE,
      NGX_WASM_RETS_I32 },

    ngx_wasm_hfunc_null
};
