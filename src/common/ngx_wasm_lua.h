#ifndef _NGX_WASM_LUA_H_INCLUDED_
#define _NGX_WASM_LUA_H_INCLUDED_


#include <ngx_proxy_wasm.h>
#if (NGX_WASM_HTTP)
#include <ngx_http_wasm.h>
#include <ngx_http_lua_util.h>
#endif
#if (0 && NGX_WASM_STREAM)
#include <ngx_stream_wasm.h>
#endif


typedef enum {
    NGX_WASM_LUA_SUBSYS_NONE = 0,
    NGX_WASM_LUA_SUBSYS_HTTP,
    NGX_WASM_LUA_SUBSYS_STREAM,
} ngx_wasm_lua_subsys_e;


struct ngx_wasm_lua_ctx_s {
    int                     co_ref;
    ngx_wasm_lua_subsys_e   subsys;
    lua_State              *L, *co;
    ngx_http_lua_co_ctx_t  *co_ctx;

    size_t                  code_len;
    int                     code_ref;
    const char             *code_name;
    const u_char           *code;
    const u_char           *cache_key;

#if (NGX_WASM_HTTP)
    ngx_http_lua_ctx_t     *rlctx;
#endif
#if (0 && NGX_WASM_STREAM)
    ngx_stream_lua_ctx_t   *slctx;
#endif

    union {
#if (NGX_WASM_HTTP)
        struct ngx_http_wasm_req_ctx_s  *request;
#endif
#if (0 && NGX_WASM_STREAM)
        ngx_stream_wasm_ctx_t           *session;
#endif
    } ctx;
};


typedef struct ngx_wasm_lua_ctx_s  ngx_wasm_lua_ctx_t;


ngx_int_t ngx_wasm_lua_init(ngx_wasm_lua_ctx_t *lua_ctx);
ngx_int_t ngx_wasm_lua_resume_coroutine(ngx_wasm_lua_ctx_t *lua_ctx);


#endif /* _NGX_WASM_LUA_H_INCLUDED_ */
