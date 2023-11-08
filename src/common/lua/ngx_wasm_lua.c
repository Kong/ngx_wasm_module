#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_wasm_lua.h>
#if (NGX_WASM_HTTP)
#include <ngx_http_lua_cache.h>
#endif


static ngx_inline unsigned
ngx_wasm_lua_thread_is_dead(ngx_wasm_lua_ctx_t *lctx)
{
    ngx_wasm_subsys_env_t  *env = &lctx->env;

    switch (env->subsys->kind) {
#if (NGX_WASM_HTTP)
    case NGX_WASM_SUBSYS_HTTP:
        if (lctx->co_ctx->co_status == NGX_HTTP_LUA_CO_DEAD) {
            return 1;
        }

        break;
#endif
#if (NGX_WASM_STREAM)
    case NGX_WASM_SUBSYS_STREAM:
        if (lctx->co_ctx->co_status == NGX_STREAM_LUA_CO_DEAD) {
            return 1;
        }

        break;
#endif
    default:
        ngx_wasm_log_error(NGX_LOG_WASM_NYI, lctx->log, 0,
                           "NYI - subsystem kind: %d",
                           env->subsys->kind);
        ngx_wasm_assert(0);
        break;
    }

    return 0;
}


static u_char *
ngx_wasm_lua_thread_cache_key(ngx_pool_t *pool, const char *tag, u_char *src,
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


static void
ngx_wasm_lua_thread_destroy(ngx_wasm_lua_ctx_t *lctx)
{
    ngx_wasm_subsys_env_t  *env = &lctx->env;

    dd("enter");

    ngx_wasm_assert(env);

    switch (env->subsys->kind) {
#if (NGX_WASM_HTTP)
    case NGX_WASM_SUBSYS_HTTP:
    {
        ngx_http_wasm_req_ctx_t  *rctx = env->ctx.rctx;
        ngx_http_lua_ctx_t       *ctx = lctx->ctx.rlctx;

        rctx->wasm_lua_ctx = NULL;

        if (ctx) {
            /* prevent ngx_http_lua_run_thread from running the
             * 'done' label, which sends the last_buf chain link */
            ctx->entered_content_phase = 0;
        }

        break;
    }
#endif
    default:
        ngx_wasm_assert(0);
        break;
    }

    ngx_pfree(lctx->pool, lctx->cache_key);

    ngx_destroy_pool(lctx->pool);
}


ngx_wasm_lua_ctx_t *
ngx_wasm_lua_thread_new(const char *tag, const char *src,
    ngx_wasm_subsys_env_t *env, ngx_log_t *log, void *data,
    ngx_wasm_lua_handler_pt success_handler,
    ngx_wasm_lua_handler_pt error_handler)
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
    lctx->success_handler = success_handler;
    lctx->error_handler = error_handler;

    ngx_memcpy(&lctx->env, env, sizeof(ngx_wasm_subsys_env_t));

    /* Lua VM + thread */

    switch (env->subsys->kind) {
#if (NGX_WASM_HTTP)
    case NGX_WASM_SUBSYS_HTTP:
    {
        ngx_http_request_t *r = env->ctx.rctx->r;

        lctx->L = ngx_http_lua_get_lua_vm(r, NULL);
        lctx->co = ngx_http_lua_new_thread(r, lctx->L, &lctx->co_ref);

        ngx_http_lua_set_req(lctx->co, r);

        break;
    }
#endif
#if (0 && NGX_WASM_STREAM)
    case NGX_WASM_SUBSYS_STREAM:
    {
        /* TODO: get stream lua r */
        ngx_stream_lua_request_t *sr = NULL;

        lctx->L = ngx_stream_lua_get_lua_vm(sr, NULL);
        lctx->co = ngx_stream_lua_new_thread(sr, lctx->L, lctx->co_ref);

        ngx_stream_lua_set_req(lctx->co, sr);

        break;
    }
#endif
    default:
        ngx_wasm_log_error(NGX_LOG_WASM_NYI, lctx->log, 0,
                           "NYI - subsystem kind: %d",
                           env->subsys->kind);
        ngx_wasm_assert(0);
        goto error;
    }

    if (lctx->L == NULL || lctx->co == NULL) {
        goto error;
    }

    /* code */

    lctx->code = src;
    lctx->code_len = ngx_strlen(src);
    lctx->cache_key = ngx_wasm_lua_thread_cache_key(lctx->pool,
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


static ngx_int_t
ngx_wasm_lua_thread_init(ngx_wasm_lua_ctx_t *lctx)
{
    ngx_wasm_subsys_env_t  *env = &lctx->env;

    switch (env->subsys->kind) {
#if (NGX_WASM_HTTP)
    case NGX_WASM_SUBSYS_HTTP:
    {
        ngx_http_wasm_req_ctx_t  *rctx = env->ctx.rctx;
        ngx_http_request_t       *r = rctx->r;
        ngx_http_lua_ctx_t       *ctx;

        ctx = ngx_http_get_module_ctx(r, ngx_http_lua_module);
        if (ctx == NULL) {
            ctx = ngx_http_lua_create_ctx(r);
            if (ctx == NULL) {
                return NGX_ERROR;
            }

        } else {
            ngx_http_lua_reset_ctx(r, lctx->L, ctx);
        }

        /* preserve ngx_wasm_lua_content_wev_handler */
        ngx_wasm_assert(!ctx->entered_content_phase);

        lctx->ctx.rlctx = ctx;
        break;
    }
#endif
#if (0 && NGX_WASM_STREAM)
    case NGX_WASM_SUBSYS_STREAM:
    {
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
    }
#endif
    default:
        ngx_wasm_assert(0);
        return NGX_ERROR;
    }

    ngx_wasm_set_resume_handler(env);

    return NGX_OK;
}


static ngx_inline ngx_int_t
ngx_wasm_lua_thread_handle_rc(ngx_wasm_lua_ctx_t *lctx, ngx_int_t rc)
{
    ngx_wasm_subsys_env_t  *env = &lctx->env;

#if (NGX_WASM_HTTP)
    if (rc == NGX_HTTP_INTERNAL_SERVER_ERROR) {
        rc = NGX_ERROR;
    }
#endif

    switch (rc) {
    case NGX_DONE:
        rc = NGX_AGAIN;
        /* fallthrough */
    case NGX_AGAIN:
        if (!ngx_wasm_lua_thread_is_dead(lctx)) {
            lctx->yielded = 1;
            ngx_wasm_set_resume_handler(env);
        }

        break;
    case NGX_OK:
        ngx_wasm_assert(ngx_wasm_lua_thread_is_dead(lctx));

        if (lctx->success_handler) {
#if (DDEBUG)
            rc = lctx->success_handler(lctx);
            dd("lua success handler rc: %ld", rc);
#else
            (void) lctx->success_handler(lctx);
#endif
        }

        rc = NGX_DONE;
        ngx_wasm_lua_thread_destroy(lctx);
        break;
    case NGX_ERROR:
        if (lctx->error_handler) {
            (void) lctx->error_handler(lctx);
        }

        ngx_wasm_lua_thread_destroy(lctx);
        break;
    default:
        ngx_wasm_log_error(NGX_LOG_WASM_NYI, lctx->log, 0,
                           "NYI - lua resume handler rc: %d", rc);
        ngx_wasm_assert(0);
        rc = NGX_ERROR;
        break;
    }

    ngx_wasm_assert(rc == NGX_DONE
                    || rc == NGX_AGAIN
                    || rc == NGX_ERROR);

    return rc;
}


/**
 * Return values:
 * NGX_DONE:   lua thread terminated
 * NGX_AGAIN:  lua thread yielded
 * NGX_ERROR:  error
 */
ngx_int_t
ngx_wasm_lua_thread_run(ngx_wasm_lua_ctx_t *lctx)
{
    ngx_int_t               rc;
    ngx_wasm_subsys_env_t  *env = &lctx->env;

    ngx_log_debug3(NGX_LOG_DEBUG_WASM, lctx->log, 0,
                   "wasm running lua thread "
                   "(lctx: %p, L: %p, co: %p)",
                   lctx, lctx->L, lctx->co);

    if (ngx_wasm_lua_thread_init(lctx) != NGX_OK) {
        goto error;
    }

    /* lua ctx */

    switch (env->subsys->kind) {
#if (NGX_WASM_HTTP)
    case NGX_WASM_SUBSYS_HTTP:
    {
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

        rc = ngx_http_lua_run_thread(lctx->L, r, ctx, lctx->nargs);
        break;
    }
#endif
#if (0 && NGX_WASM_STREAM)
    case NGX_WASM_SUBSYS_STREAM:
    {
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

        rc = ngx_stream_lua_run_thread(lctx->L, s, ctx, lctx->nargs);
        break;
    }
#endif
    default:
        ngx_wasm_assert(0);
        goto error;
    }

    dd("lua thread run rc: %ld", rc);

    return ngx_wasm_lua_thread_handle_rc(lctx, rc);

error:

    ngx_wasm_lua_thread_destroy(lctx);

    return NGX_ERROR;
}


/**
 * Return values:
 * NGX_DONE:   lua thread terminated
 * NGX_AGAIN:  lua thread yielded
 * NGX_ERROR:  error
 */
ngx_int_t
ngx_wasm_lua_thread_resume(ngx_wasm_lua_ctx_t *lctx)
{
    ngx_int_t               rc;
    ngx_wasm_subsys_env_t  *env = &lctx->env;

    if (ngx_wasm_lua_thread_is_dead(lctx)) {
        return NGX_DONE;
    }

    ngx_log_debug3(NGX_LOG_DEBUG_WASM, lctx->log, 0,
                   "wasm resuming lua thread "
                   "(lctx: %p, L: %p, co: %p)",
                   lctx, lctx->L, lctx->co);

    /* lua ctx */

    switch (env->subsys->kind) {
#if (NGX_WASM_HTTP)
    case NGX_WASM_SUBSYS_HTTP:
    {
        ngx_http_request_t  *r = env->ctx.rctx->r;
        ngx_http_lua_ctx_t  *ctx = lctx->ctx.rlctx;

        ngx_wasm_assert(ctx == ngx_http_get_module_ctx(r, ngx_http_lua_module));

        ngx_wasm_set_resume_handler(env);
        rc = ctx->resume_handler(r);
        break;
    }
#endif
    default:
        ngx_wasm_assert(0);
        return NGX_ERROR;
    }

    dd("lua resume handler rc: %ld", rc);

    return ngx_wasm_lua_thread_handle_rc(lctx, rc);
}
