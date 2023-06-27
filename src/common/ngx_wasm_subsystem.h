#ifndef _NGX_WASM_SUBSYSTEM_H_INCLUDED_
#define _NGX_WASM_SUBSYSTEM_H_INCLUDED_


#include <ngx_wasm.h>
#if (NGX_WASM_HTTP)
#include <ngx_http_wasm.h>
#endif
#if (NGX_WASM_STREAM)
#include <ngx_stream_wasm.h>
#endif


ngx_wasm_phase_t *ngx_wasm_phase_lookup(ngx_wasm_subsystem_t *subsys,
    ngx_uint_t phaseidx);
void ngx_wasm_set_resume_handler(ngx_wasm_subsys_env_t *env);
void ngx_wasm_yield(ngx_wasm_subsys_env_t *env);


#endif /* _NGX_WASM_SUBSYSTEM_H_INCLUDED_ */
