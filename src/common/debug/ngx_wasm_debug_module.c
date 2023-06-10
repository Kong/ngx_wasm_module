#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_wasm.h>
#include <ngx_wasm_subsystem.h>

#if (!NGX_DEBUG)
#   error ngx_wasm_debug_module included in a non-debug build
#endif


static ngx_int_t ngx_wasm_debug_init(ngx_cycle_t *cycle);


static ngx_wasm_module_t  ngx_wasm_debug_module_ctx = {
    NULL,                                  /* create configuration */
    NULL,                                  /* init configuration */
    ngx_wasm_debug_init                    /* init module */
};


ngx_module_t  ngx_wasm_debug_module = {
    NGX_MODULE_V1,
    &ngx_wasm_debug_module_ctx,            /* module context */
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


static ngx_int_t
ngx_wasm_debug_init(ngx_cycle_t *cycle)
{
    static ngx_wasm_phase_t  ngx_wasm_debug_phases[] = {
        { ngx_string("a_phase"), 0, 0, 0 },
        { ngx_null_string, 0, 0, 0 }
    };

    static ngx_wasm_subsystem_t  ngx_wasm_debug_subsystem = {
        2,
        NGX_WASM_SUBSYS_HTTP, /* stub */
        ngx_wasm_debug_phases,
    };

    /* coverage */

    /**
     * ngx_wasm_phase_lookup NULL return branch is unreacheable in
     * normal ngx_wasm_module usage.
     */
    ngx_wasm_assert(
        ngx_wasm_phase_lookup(&ngx_wasm_debug_subsystem, 3) == NULL
    );

    return NGX_OK;
}
