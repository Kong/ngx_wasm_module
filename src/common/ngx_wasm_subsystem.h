#ifndef _NGX_WASM_SUBSYSTEM_H_INCLUDED_
#define _NGX_WASM_SUBSYSTEM_H_INCLUDED_


#if (NGX_WASM_HTTP)
#include <ngx_http_wasm.h>
#endif
#if (NGX_WASM_STREAM)
#include <ngx_stream_wasm.h>
#endif


typedef struct {
    ngx_connection_t                        *connection;
    ngx_buf_tag_t                           *buf_tag;
    ngx_wasm_subsystem_t                    *subsys;
#if (NGX_SSL)
    ngx_wasm_ssl_conf_t                     *ssl_conf;
#endif

    union {
#if (NGX_WASM_HTTP)
        ngx_http_wasm_req_ctx_t             *rctx;
#endif
#if (NGX_WASM_STREAM)
        ngx_stream_wasm_ctx_t               *sctx;
#endif
    } ctx;
} ngx_wasm_subsys_env_t;


static ngx_inline void
ngx_wasm_set_resume_handler(ngx_wasm_subsys_env_t *env)
{
#if (NGX_WASM_HTTP)
    ngx_http_request_t       *r;
    ngx_http_wasm_req_ctx_t  *rctx;

    if (env->subsys->kind == NGX_WASM_SUBSYS_HTTP) {
        rctx = env->ctx.rctx;
        r = rctx->r;

        if (rctx->entered_content_phase || rctx->fake_request) {
            r->write_event_handler = ngx_http_wasm_content_wev_handler;

        } else {
            r->write_event_handler = ngx_http_core_run_phases;
        }
    }
#endif
}


#endif /* _NGX_WASM_SUBSYSTEM_H_INCLUDED_ */
