#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_stream.h>
#include <ngx_wasm_ops.h>
#include <ngx_proxy_wasm.h>


static ngx_command_t  ngx_stream_wasm_module_cmds[] = {
    ngx_null_command
};


static ngx_stream_module_t  ngx_stream_wasm_module_ctx = {
    NULL,                                  /* preconfiguration */
    NULL,                                  /* postconfiguration */

    NULL,                                  /* create main configuration */
    NULL,                                  /* init main configuration */

    NULL,                                  /* create server configuration */
    NULL                                   /* merge server configuration */
};


ngx_module_t  ngx_stream_wasm_module = {
    NGX_MODULE_V1,
    &ngx_stream_wasm_module_ctx,            /* module context */
    ngx_stream_wasm_module_cmds,           /* module directives */
    NGX_STREAM_MODULE,                     /* module type */
    NULL,                                  /* init master */
    NULL,                                  /* init module */
    NULL,                                  /* init process */
    NULL,                                  /* init thread */
    NULL,                                  /* exit thread */
    NULL,                                  /* exit process */
    NULL,                                  /* exit master */
    NGX_MODULE_V1_PADDING
};
