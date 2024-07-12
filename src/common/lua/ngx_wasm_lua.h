#ifndef _NGX_WASM_LUA_H_INCLUDED_
#define _NGX_WASM_LUA_H_INCLUDED_


#include <ngx_wasm_subsystem.h>
#if (NGX_WASM_HTTP)
#include <ngx_http_lua_util.h>
#include <ngx_http_lua_coroutine.h>
#endif
#if (NGX_WASM_STREAM)
#include <ngx_stream_lua_util.h>
#include <ngx_stream_lua_cache.h>
#endif


typedef ngx_int_t (*ngx_wasm_lua_handler_pt)(ngx_wasm_lua_ctx_t *lctx);


struct ngx_wasm_lua_ctx_s {
    ngx_queue_t                     q;                /* entry sub_ctxs */
    ngx_queue_t                     sub_ctxs;         /* entry lctx only */

    ngx_pool_t                     *pool;
    ngx_log_t                      *log;
    ngx_wasm_subsys_env_t          *env;
    ngx_wasm_lua_handler_pt         error_handler;
    ngx_wasm_lua_handler_pt         success_handler;
    void                           *data;

    const char                     *code;
    u_char                         *cache_key;
    size_t                          code_len;
    int                             code_ref;

    int                             co_ref;
    ngx_http_lua_co_ctx_t          *co_ctx;
    lua_State                      *co;
    lua_State                      *L;
    ngx_uint_t                      nargs;

    union {
#if (NGX_WASM_HTTP)
        ngx_http_lua_ctx_t         *rlctx;
#endif
#if (NGX_WASM_STREAM)
        ngx_stream_lua_ctx_t       *slctx;
#endif
    } ctx;

    unsigned                        entry:1;        /* is entry lctx */
    unsigned                        yielded:1;      /* has yielded at least once */
    unsigned                        finished:1;     /* has finished */
    unsigned                        cancelled:1;    /* cancelled */
};


ngx_wasm_lua_ctx_t *ngx_wasm_lua_thread_new(const char *tag, const char *src,
    ngx_wasm_subsys_env_t *env, ngx_log_t *log, void *data,
    ngx_wasm_lua_handler_pt success_handler,
    ngx_wasm_lua_handler_pt error_handler);
ngx_int_t ngx_wasm_lua_thread_run(ngx_wasm_lua_ctx_t *lctx);
void ngx_wasm_lua_thread_cancel(ngx_wasm_lua_ctx_t *lctx);
ngx_int_t ngx_wasm_lua_resume(ngx_wasm_subsys_env_t *env);


#if 0
static ngx_inline void
dumpstack(lua_State *L)
{
    int top = lua_gettop(L);

    dd("top: %d", top);

    for (int i = 1; i <= top; i++) {
        switch (lua_type(L, i)) {
        case LUA_TNUMBER:
            dd("%d\t%s\t\"%g\"",
               i, luaL_typename(L, i), lua_tonumber(L, i));
            break;
        case LUA_TSTRING:
            dd("%d\t%s\t\"%s\"",
               i, luaL_typename(L, i), lua_tostring(L, i));
            break;
        case LUA_TBOOLEAN:
            dd("%d\t%s\t\"%s\"",
               i, luaL_typename(L, i), (lua_toboolean(L, i) ? "true" : "false"));
            break;
        case LUA_TNIL:
            dd("%d\t%s\t",
               i, luaL_typename(L, i));
            break;
        default:
            dd("%d\t%s\t\"%p\"",
               i, luaL_typename(L, i), lua_topointer(L, i));
            break;
        }
    }
}
#endif


#endif /* _NGX_WASM_LUA_H_INCLUDED_ */
