#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_wasm_lua.h>
#include <ngx_http_lua_cache.h>


static ngx_int_t
ngx_wasm_lua_set_lua_context(ngx_wasm_lua_ctx_t *lua_ctx)
{
#if (NGX_WASM_HTTP)
    ngx_http_request_t    *r;
#endif
#if (0 && NGX_WASM_STREAM)
    ngx_stream_session_t  *s;
#endif

    switch (lua_ctx->subsys) {

#if (NGX_WASM_HTTP)
    case NGX_WASM_LUA_SUBSYS_HTTP:
        r = lua_ctx->ctx.request->r;

        lua_ctx->rlctx = ngx_http_get_module_ctx(r, ngx_http_lua_module);
        if (lua_ctx->rlctx == NULL) {
            lua_ctx->rlctx = ngx_http_lua_create_ctx(r);
            if (lua_ctx->rlctx == NULL) {
                return NGX_ERROR;
            }

        } else {
            ngx_http_lua_reset_ctx(r, lua_ctx->L, lua_ctx->rlctx);
        }

        return NGX_OK;
#endif

#if (0 && NGX_WASM_STREAM)
    case NGX_WASM_LUA_SUBSYS_STREAM:
        s = lua_ctx->ctx.session->s;
        lua_ctx->slctx = ngx_stream_get_module_ctx(s, ngx_stream_lua_module);
        if (lua_ctx->slctx == NULL) {
            lua_ctx->slctx = ngx_stream_lua_create_ctx(s);
            if (lua_ctx->slctx == NULL) {
                return NGX_ERROR;
            }

        } else {
            ngx_stream_lua_reset_ctx(s, lua_ctx->L, lua_ctx->slctx);
        }

        return NGX_OK;
#endif

    default:
        /* unreachable */
        ngx_wasm_assert(0);
        return NGX_ERROR;

    }
}


static void
ngx_wasm_lua_init_lua_context(ngx_wasm_lua_ctx_t *lua_ctx)
{
    switch (lua_ctx->subsys) {

#if (NGX_WASM_HTTP)
    case NGX_WASM_LUA_SUBSYS_HTTP:
        lua_ctx->ctx.request->wasm_lua_ctx = lua_ctx;

        /* TODO: set lua_ctx->rlctx->context properly */
        /* maybe lua_ctx->rlctx->context will be NGX_HTTP_LUA_CONTEXT_TIMER */
        /* when running from on_tick */
        lua_ctx->rlctx->context = NGX_HTTP_LUA_CONTEXT_CONTENT;
        lua_ctx->rlctx->cur_co_ctx = &lua_ctx->rlctx->entry_co_ctx;
        lua_ctx->rlctx->cur_co_ctx->co = lua_ctx->co;
        lua_ctx->rlctx->cur_co_ctx->co_ref = lua_ctx->co_ref;
#if (NGX_LUA_USE_ASSERT)
        lua_ctx->rlctx->cur_co_ctx->co_top = 1;
#endif
#ifndef OPENRESTY_LUAJIT
        ngx_http_lua_get_globals_table(lua_ctx->co);
        lua_setfenv(lua_ctx->co, -2);
#endif
        ngx_http_lua_set_req(lua_ctx->co, lua_ctx->ctx.request->r);
        ngx_http_lua_attach_co_ctx_to_L(lua_ctx->co,
                                        lua_ctx->rlctx->cur_co_ctx);
        lua_ctx->co_ctx = lua_ctx->rlctx->cur_co_ctx;

        break;
#endif

#if (0 && NGX_WASM_STREAM)
    case NGX_WASM_LUA_SUBSYS_STREAM:
        /* set lua_ctx->slctx->context properly */
        lua_ctx->slctx->context = NGX_STREAM_LUA_CONTEXT_CONTENT;
        lua_ctx->slctx->cur_co_ctx = &lua_ctx->slctx->entry_co_ctx;
        lua_ctx->slctx->cur_co_ctx->co = lua_ctx->co;
        lua_ctx->slctx->cur_co_ctx->co_ref = lua_ctx->co_ref;
#if (NGX_LUA_USE_ASSERT)
        lua_ctx->slctx->cur_co_ctx->co_top = 1;
#endif
#ifndef OPENRESTY_LUAJIT
        ngx_stream_lua_get_globals_table(lua_ctx->co);
        lua_setfenv(lua_ctx->co, -2);
#endif
        ngx_stream_lua_set_req(co, lua_ctx->ctx.session->s);
        lua_ctx->co_ctx = lua_ctx->slctx->cur_co_ctx;

        break;
#endif

    default:
        /* unreachable */
        ngx_wasm_assert(0);
        break;

    }
}


static ngx_int_t
ngx_wasm_lua_load_code(ngx_wasm_lua_ctx_t *lua_ctx)
{
    ngx_int_t   rc;
    ngx_log_t  *log;

    switch (lua_ctx->subsys) {

#if (NGX_WASM_HTTP)
    case NGX_WASM_LUA_SUBSYS_HTTP:
        log = lua_ctx->ctx.request->r->connection->log;
        rc = ngx_http_lua_cache_loadbuffer(log,
                                           lua_ctx->L, lua_ctx->code,
                                           lua_ctx->code_len,
                                           &lua_ctx->code_ref,
                                           lua_ctx->cache_key,
                                           lua_ctx->code_name);

        break;
#endif

#if (0 && NGX_WASM_STREAM)
    case NGX_WASM_LUA_SUBSYS_STREAM:
        log = lua_ctx->ctx.session->s->connection->log;
        rc = ngx_stream_lua_cache_loadbuffer(log,
                                             lua_ctx->L, lua_ctx->code,
                                             lua_ctx->code_len,
                                             lua_ctx->code_ref,
                                             lua_ctx->cache_key, lua_ctx->name);

        break;
#endif

    default:
        /* unreachable */
        ngx_wasm_assert(0);
        return NGX_ERROR;

    }

    if (rc != NGX_OK) {
        ngx_wasm_log_error(NGX_LOG_ERR, log, 0,
                           "failed loading lua code in wasm lua");
        return rc;
    }

    /*  move code closure to new coroutine  */
    lua_xmove(lua_ctx->L, lua_ctx->co, 1);

    return NGX_OK;
}


ngx_int_t
ngx_wasm_lua_init(ngx_wasm_lua_ctx_t *lua_ctx)
{
    ngx_log_t             *log;
#if (0 && NGX_WASM_STREAM)
    ngx_stream_lua_ctx_t  *ctx;
#endif

    switch (lua_ctx->subsys) {

#if (NGX_WASM_HTTP)
    case NGX_WASM_LUA_SUBSYS_HTTP:
        log = lua_ctx->ctx.request->r->connection->log;
        lua_ctx->L = ngx_http_lua_get_lua_vm(lua_ctx->ctx.request->r, NULL);
        lua_ctx->co = ngx_http_lua_new_thread(lua_ctx->ctx.request->r,
                                              lua_ctx->L, &lua_ctx->co_ref);
        break;
#endif

#if (0 && NGX_WASM_STREAM)
    case NGX_WASM_LUA_SUBSYS_STREAM:
        log = lua_ctx->ctx.session->s->connection->log;
        ctx = ngx_stream_get_module_ctx(lua_ctx->session->s,
                                        ngx_stream_lua_module);
        lua_ctx->L = ngx_stream_lua_get_lua_vm(lua_ctx->session->s, NULL);
        lua_ctx->co = ngx_stream_lua_new_thread(ctx->request, lua_ctx->L,
                                                lua_ctx->co_ref);

        break;
#endif

    default:
        /* unreachable */
        ngx_wasm_assert(0);
        return NGX_ERROR;

    }

    if (!lua_ctx->L) {
        ngx_wasm_log_error(NGX_LOG_WASM_NYI, log, 0,
                           "failed getting lua vm in wasm lua");
        return NGX_ERROR;
    }

    if (!lua_ctx->co) {
        ngx_wasm_log_error(NGX_LOG_WASM_NYI, log, 0,
                           "failed creating lua thread in wasm lua ");
        return NGX_ERROR;
    }

    return ngx_wasm_lua_load_code(lua_ctx);
}


ngx_int_t
ngx_wasm_lua_resume_coroutine(ngx_wasm_lua_ctx_t *lua_ctx)
{
    ngx_int_t              rc;
#if (NGX_WASM_HTTP)
    ngx_http_request_t    *r;
#endif
#if (0 && NGX_WASM_STREAM)
    ngx_stream_session_t  *s;
#endif

    ngx_wasm_lua_set_lua_context(lua_ctx);
    ngx_wasm_lua_init_lua_context(lua_ctx);

    switch (lua_ctx->subsys) {

#if (NGX_WASM_HTTP)
    case NGX_WASM_LUA_SUBSYS_HTTP:
        r = lua_ctx->ctx.request->r;
        rc = ngx_http_lua_run_thread(lua_ctx->L, r, lua_ctx->rlctx, 1);

        return rc;
#endif

#if (0 && NGX_WASM_STREAM)
    case NGX_WASM_LUA_SUBSYS_STREAM:
        s = lua_ctx->ctx.request->s;

        return ngx_stream_lua_run_thread(lua_ctx->L, s, lua_ctx->slctx, 1);
#endif

    default:
        /* unreachable */
        ngx_wasm_assert(0);
        return NGX_ERROR;

    }
}
