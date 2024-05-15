#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_wasm_subsystem.h>


ngx_inline ngx_wasm_phase_t *
ngx_wasm_phase_lookup(ngx_wasm_subsystem_t *subsys, ngx_uint_t phaseidx)
{
    ngx_wasm_phase_t  *phase = subsys->phases;

    while (phase->index != phaseidx) {
        phase++;

        if (phase->name.len == 0) {
            return NULL;
        }
    }

    return phase;
}


ngx_inline void
ngx_wasm_set_resume_handler(ngx_wasm_subsys_env_t *env)
{
#if (NGX_WASM_HTTP)
    ngx_http_wasm_req_ctx_t  *rctx;

    if (env->subsys->kind == NGX_WASM_SUBSYS_HTTP) {
        rctx = env->ctx.rctx;

        ngx_http_wasm_set_resume_handler(rctx);
    }
#endif
}


ngx_inline void
ngx_wasm_resume(ngx_wasm_subsys_env_t *env)
{
#if (NGX_WASM_HTTP)
    ngx_http_wasm_req_ctx_t  *rctx;

    if (env->subsys->kind == NGX_WASM_SUBSYS_HTTP) {
        rctx = env->ctx.rctx;

        ngx_http_wasm_resume(rctx);
    }
#endif
}
