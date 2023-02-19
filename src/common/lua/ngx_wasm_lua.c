#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_wasm_lua.h>
#if (NGX_WASM_HTTP)
#include <ngx_http_lua_cache.h>
#endif


static u_char *
ngx_wasm_lua_cache_key(ngx_pool_t *pool, const char *tag, u_char *src,
    size_t src_len)
{
    size_t   tag_len;
    u_char  *p, *out;

    tag_len = ngx_strlen(tag);

    out = ngx_palloc(pool, tag_len + 2 * MD5_DIGEST_LENGTH + 1);
    if (out == NULL) {
        return NULL;
    }

    p = ngx_copy(out, tag, tag_len);

#if (NGX_WASM_HTTP)
    p = ngx_http_lua_digest_hex(p, src, src_len);
#elif (NGX_WASM_STREAM)
    p = ngx_stream_lua_digest_hex(p, src, src_len);
#else
    ngx_wasm_assert(0);
    ngx_pfree(pool, out);
    return NULL;
#endif

    *p = '\0';

    return out;
}


ngx_wasm_lua_ctx_t *
ngx_wasm_lua_thread_new(const char *tag, const char *src,
    ngx_wasm_subsys_env_t *env, ngx_log_t *log, void *data)
{
    ngx_int_t            rc;
    ngx_pool_t          *pool;
    ngx_wasm_lua_ctx_t  *lctx;

    /* pool */

    pool = ngx_create_pool(512, log);
    if (pool == NULL) {
        return NULL;
    }

    /* lctx */

    lctx = ngx_pcalloc(pool, sizeof(ngx_wasm_lua_ctx_t));
    if (lctx == NULL) {
        goto error;
    }

    lctx->pool = pool;
    lctx->log = log;
    lctx->data = data;

    ngx_memcpy(&lctx->env, env, sizeof(ngx_wasm_subsys_env_t));

    /* Lua VM + thread */

    switch (env->subsys->kind) {
#if (NGX_WASM_HTTP)
    case NGX_WASM_SUBSYS_HTTP:
        ngx_http_request_t *r = env->ctx.rctx->r;

        lctx->L = ngx_http_lua_get_lua_vm(r, NULL);
        lctx->co = ngx_http_lua_new_thread(r, lctx->L, &lctx->co_ref);
        break;
#endif
#if (0 && NGX_WASM_STREAM)
    case NGX_WASM_SUBSYS_STREAM:
        /* TODO: get stream lua r */
        ngx_stream_lua_request_t *sr = NULL;

        lctx->L = ngx_stream_lua_get_lua_vm(sr, NULL);
        lctx->co = ngx_stream_lua_new_thread(sr, lctx->L, lctx->co_ref);
        break;
#endif
    default:
        ngx_wasm_assert(0);
        goto error;
    }

    if (lctx->L == NULL || lctx->co == NULL) {
        goto error;
    }

    /* code */

    lctx->code = src;
    lctx->code_len = ngx_strlen(src);

    /* cache key */

    lctx->cache_key = ngx_wasm_lua_cache_key(lctx->pool,
                                             tag,
                                             (u_char *) lctx->code,
                                             lctx->code_len);
    if (lctx->cache_key == NULL) {
        goto error;
    }

    /* load code */

    switch (env->subsys->kind) {
#if (NGX_WASM_HTTP)
    case NGX_WASM_SUBSYS_HTTP:
        rc = ngx_http_lua_cache_loadbuffer(lctx->log,
                                           lctx->L,
                                           (u_char *) lctx->code,
                                           lctx->code_len,
                                           &lctx->code_ref,
                                           lctx->cache_key,
                                           tag);
        break;
#endif
#if (NGX_WASM_STREAM)
    case NGX_WASM_SUBSYS_STREAM:
        rc = ngx_stream_lua_cache_loadbuffer(lctx->log,
                                             lctx->L,
                                             (u_char *) lctx->code,
                                             lctx->code_len,
                                             lctx->cache_key,
                                             tag);
        break;
#endif
    default:
        /* unreachable */
        ngx_wasm_assert(0);
        goto error;
    }

    if (rc != NGX_OK) {
        goto error;
    }

    /*  move code closure to new coroutine  */
    lua_xmove(lctx->L, lctx->co, 1);

    return lctx;

error:

    ngx_wasm_lua_thread_destroy(lctx);

    return NULL;
}


void
ngx_wasm_lua_thread_destroy(ngx_wasm_lua_ctx_t *lctx)
{
#if (NGX_WASM_HTTP)
    ngx_http_wasm_req_ctx_t  *rctx = lctx->env.ctx.rctx;
#endif

    switch (lctx->env.subsys->kind) {
#if (NGX_WASM_HTTP)
    case NGX_WASM_SUBSYS_HTTP:
        rctx->wasm_lua_ctx = NULL;
        break;
#endif
    default:
        ngx_wasm_assert(0);
        break;
    }

    ngx_destroy_pool(lctx->pool);
}


static ngx_int_t
ngx_wasm_lua_thread_init(ngx_wasm_lua_ctx_t *lctx)
{
    switch (lctx->env.subsys->kind) {
#if (NGX_WASM_HTTP)
    case NGX_WASM_SUBSYS_HTTP:
        ngx_http_request_t  *r = lctx->env.ctx.rctx->r;
        ngx_http_lua_ctx_t  *ctx;

        ctx = ngx_http_get_module_ctx(r, ngx_http_lua_module);
        if (ctx == NULL) {
            ctx = ngx_http_lua_create_ctx(r);
            if (ctx == NULL) {
                return NGX_ERROR;
            }

        } else {
            ngx_http_lua_reset_ctx(r, lctx->L, ctx);
        }

        lctx->ctx.rlctx = ctx;
        break;
#endif
#if (0 && NGX_WASM_STREAM)
    case NGX_WASM_SUBSYS_STREAM:
        /* TODO: get stream lua r */
        ngx_stream_lua_request_t  *r = NULL;
        ngx_stream_lua_ctx_t      *ctx;

        ctx = ngx_stream_get_module_ctx(r, ngx_stream_lua_module);
        if (ctx == NULL) {
            ctx = ngx_stream_lua_create_ctx(r);
            if (ctx == NULL) {
                return NGX_ERROR;
            }

        } else {
            ngx_stream_lua_reset_ctx(r, lctx->L, ctx);
        }

        lctx->ctx.slctx = ctx;
        break;
#endif
    default:
        /* unreachable */
        ngx_wasm_assert(0);
        return NGX_ERROR;
    }

    return NGX_OK;
}


ngx_int_t
ngx_wasm_lua_thread_run(ngx_wasm_lua_ctx_t *lctx)
{
    ngx_wasm_subsys_env_t  *env = &lctx->env;

    ngx_log_debug3(NGX_LOG_DEBUG_WASM, lctx->log, 0,
                   "wasm lua resolver resuming thread "
                   "(lctx: %p, L: %p, co: %p)",
                   lctx, lctx->L, lctx->co);

    if (ngx_wasm_lua_thread_init(lctx) != NGX_OK) {
        goto error;
    }

    /* lua ctx */

    switch (env->subsys->kind) {

#if (NGX_WASM_HTTP)
    case NGX_WASM_SUBSYS_HTTP:
        ngx_http_request_t  *r = env->ctx.rctx->r;
        ngx_http_lua_ctx_t  *ctx = lctx->ctx.rlctx;

        ctx->context = NGX_HTTP_LUA_CONTEXT_TIMER;
        ctx->cur_co_ctx = &ctx->entry_co_ctx;
        ctx->cur_co_ctx->co = lctx->co;
        ctx->cur_co_ctx->co_ref = lctx->co_ref;
#if (NGX_LUA_USE_ASSERT)
        ctx->cur_co_ctx->co_top = 1;
#endif

#ifndef OPENRESTY_LUAJIT
        ngx_http_lua_get_globals_table(lctx->co);
        lua_setfenv(lctx->co, -2);
#endif

        ngx_http_lua_set_req(lctx->co, r);
        ngx_http_lua_attach_co_ctx_to_L(lctx->co, ctx->cur_co_ctx);

        lctx->co_ctx = ctx->cur_co_ctx;
        lctx->env.ctx.rctx->wasm_lua_ctx = lctx;

        return ngx_http_lua_run_thread(lctx->L, r, ctx, lctx->nargs);
#endif

#if (0 && NGX_WASM_STREAM)
    case NGX_WASM_SUBSYS_STREAM:
        /* TODO: get stream lua r */
        ngx_stream_lua_request_t  *r = NULL;
        ngx_stream_lua_ctx_t      *ctx = lctx->rctx.slctx;

        ctx->context = NGX_STREAM_LUA_CONTEXT_TIMER;
        ctx->cur_co_ctx = &ctx->entry_co_ctx;
        ctx->cur_co_ctx->co = lctx->co;
        ctx->cur_co_ctx->co_ref = lctx->co_ref;
#if (NGX_LUA_USE_ASSERT)
        ctx->cur_co_ctx->co_top = 1;
#endif

#ifndef OPENRESTY_LUAJIT
        ngx_stream_lua_get_globals_table(lctx->co);
        lua_setfenv(lctx->co, -2);
#endif

        ngx_stream_lua_set_req(co, s);

        lctx->co_ctx = ctx->cur_co_ctx;

        return ngx_stream_lua_run_thread(lctx->L, s, ctx, lctx->nargs);
#endif

    default:
        /* unreachable */
        ngx_wasm_assert(0);
        break;

    }

error:

    ngx_wasm_lua_thread_destroy(lctx);

    return NGX_ERROR;
}
