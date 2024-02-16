#ifndef _NGX_WASM_SUBSYSTEM_H_INCLUDED_
#define _NGX_WASM_SUBSYSTEM_H_INCLUDED_


#include <ngx_wasm.h>
#if (NGX_WASM_HTTP)
#include <ngx_http_wasm.h>
#endif
#if (NGX_WASM_STREAM)
#include <ngx_stream_wasm.h>
#endif


#define ngx_wasm_continue(env)                                               \
    (env)->state = NGX_WASM_STATE_CONTINUE

#define ngx_wasm_error(env)                                                  \
    (env)->state = NGX_WASM_STATE_ERROR

#define ngx_wasm_yield(env)                                                  \
    (env)->state = NGX_WASM_STATE_YIELD;                                     \
    ngx_wasm_set_resume_handler(env)

#define ngx_wasm_yielding(env)                                               \
    (env)->state == NGX_WASM_STATE_YIELD


ngx_wasm_phase_t *ngx_wasm_phase_lookup(ngx_wasm_subsystem_t *subsys,
    ngx_uint_t phaseidx);
void ngx_wasm_set_resume_handler(ngx_wasm_subsys_env_t *env);


#endif /* _NGX_WASM_SUBSYSTEM_H_INCLUDED_ */
