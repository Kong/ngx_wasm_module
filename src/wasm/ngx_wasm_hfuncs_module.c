/*
 * Copyright (C) Thibault Charbonnier
 */

#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_core.h>
#include <ngx_wasm_module.h>
#include <ngx_wasm_hfuncs.h>


ngx_int_t
ngx_wasm_hfuncs_log(ngx_wasm_hctx_t *hctx, const ngx_wasm_val_t args[],
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


ngx_wasm_hfunc_decl_t  ngx_wasm_hfuncs[] = {

    { ngx_string("ngx_log"),
      &ngx_wasm_hfuncs_log,
      NGX_WASM_ARGS_I32_I32_I32,
      NGX_WASM_RETS_NONE },

    ngx_wasm_hfunc_null
};


static ngx_int_t
ngx_wasm_hfuncs_init(ngx_cycle_t *cycle)
{
    ngx_wasm_core_hfuncs_add(cycle, ngx_wasm_hfuncs);

    return NGX_OK;
}


static ngx_wasm_module_t  ngx_wasm_hfuncs_module_ctx = {
    ngx_string("wasm_hfuncs"),
    NULL,                                  /* create configuration */
    NULL,                                  /* init configuration */
    ngx_wasm_hfuncs_init,                  /* init module */
    NULL,                                  /* exit process */
    NULL,                                  /* exit master */
    NGX_WASM_VM_NO_ACTIONS                 /* vm actions */
};


ngx_module_t  ngx_wasm_hfuncs_module = {
    NGX_MODULE_V1,
    &ngx_wasm_hfuncs_module_ctx,           /* module context */
    NULL,                                  /* module directives */
    NGX_WASM_MODULE,                       /* module type */
    NULL,                                  /* init master */
    NULL,                                  /* init module */
    NULL,                                  /* init process */
    NULL,                                  /* init thread */
    NULL,                                  /* exit thread */
    NULL,                                  /* exit process */
    NULL,                                  /* exit master */
    NGX_MODULE_V1_PADDING
};
