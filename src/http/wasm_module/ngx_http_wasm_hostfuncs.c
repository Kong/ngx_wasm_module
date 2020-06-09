/*
 * Copyright (C) Thibault Charbonnier
 */

#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_http_wasm_hostfuncs.h>


ngx_wasm_vm_trap_pt
ngx_wasm_host_log(void *env, const ngx_wasm_vm_val_t args[],
    ngx_wasm_vm_val_t rets[])
{
#if 0
    ngx_wasm_winstance_t     *winstance = env;
    int32_t                   level, len, offset;

    level = args[0].of.i32;
    offset = args[1].of.i32;
    len = args[2].of.i32;

    byte_t *p = wasm_memory_data(current_memory);

    ngx_log_error(level, wtinstance->log, 0,
                  "[from wasm] (level: %d, offset: %p, len: %d, memory_data: %p)",
                  level, offset, len, p);

    ngx_log_error(level, wtinstance->log, 0, "[from wasm] \"%*s\"",
                  len, p + offset);
#endif

    return NULL;
}


ngx_wasm_hostfunc_t  ngx_http_wasm_hfuncs[] = {
    { ngx_string("ngx_wasm_log"), &ngx_wasm_host_log },

    ngx_wasm_hostfuncs_null
};


/* vi:set ft=c ts=4 sw=4 et fdm=marker: */
