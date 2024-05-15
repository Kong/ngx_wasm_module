#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_wasm_lua.h>
#if (NGX_WASM_HTTP)
#include <ngx_http_lua_cache.h>
#include <ngx_http_lua_uthread.h>
#endif
#if (NGX_WASM_STREAM)
#include <ngx_stream_lua_uthread.h>
#endif


static ngx_int_t ngx_http_wasm_lua_resume_handler(ngx_http_request_t *r);


static const char  *WASM_LUA_ENTRY_SCRIPT_NAME = "wasm_lua_entry_chunk";
static const char  *WASM_LUA_ENTRY_SCRIPT = ""
    "while true do\n"
#if (DDEBUG)
    "    ngx.log(ngx.DEBUG, 'entry lua thread waking up')\n"
#endif
#if (NGX_DEBUG)
    "    ngx.sleep(0.1)\n" /* greater than 0, must not be a delayed event */
#else
    "    ngx.sleep(30)\n"
#endif
    "end\n";


static void
destroy_thread(ngx_wasm_lua_ctx_t *lctx)
{
    ngx_http_lua_co_ctx_t  *coctx;

    ngx_log_debug2(NGX_LOG_DEBUG_WASM, ngx_cycle->log, 0,
                   "wasm freeing lua%sthread (lctx: %p)",
                   lctx->entry ? " entry " : " user ", lctx);

    coctx = lctx->co_ctx;

    if (coctx && coctx->cleanup) {
        coctx->cleanup(coctx);
        coctx->cleanup = NULL;
    }

    ngx_pfree(lctx->pool, lctx->cache_key);
    ngx_pfree(lctx->pool, lctx);
}


static ngx_inline unsigned
entry_thread_empty(ngx_wasm_subsys_env_t *env)
{
    ngx_wasm_lua_ctx_t  *entry_lctx = env->entry_lctx;

    ngx_wa_assert(entry_lctx);

    return ngx_queue_empty(&entry_lctx->sub_ctxs);
}


static void
thread_cleanup_handler(void *data)
{
    ngx_wasm_lua_ctx_t  *lctx = data;

    dd("enter");

    if (lctx->entry && lctx->ev && lctx->ev->timer_set) {
        dd("delete pending timer event");
        ngx_event_del_timer(lctx->ev);
    }

    destroy_thread(lctx);
}


static ngx_int_t
entry_thread_start(ngx_wasm_subsys_env_t *env)
{
    ngx_int_t                 rc;
    ngx_wasm_lua_ctx_t       *entry_lctx = env->entry_lctx;
    ngx_http_wasm_req_ctx_t  *rctx = env->ctx.rctx;
    ngx_http_request_t       *r = rctx->r;
    ngx_http_lua_ctx_t       *ctx;

    ctx = ngx_http_get_module_ctx(r, ngx_http_lua_module);
    if (ctx == NULL) {
        ctx = ngx_http_lua_create_ctx(r);
        if (ctx == NULL) {
            return NGX_ERROR;
        }
    }

    if (entry_lctx == NULL) {
        /**
         * In OpenResty, all uthreads *must* be attached to a parent coroutine,
         * so we create a "fake" one simulating a *_by_lua_block context.
         */
        dd("creating entry thread");

        entry_lctx = ngx_wasm_lua_thread_new(WASM_LUA_ENTRY_SCRIPT_NAME,
                                             WASM_LUA_ENTRY_SCRIPT,
                                             env, env->connection->log,
                                             NULL, NULL, NULL);
        if (entry_lctx == NULL) {
            return NGX_ERROR;
        }

        env->entry_lctx = entry_lctx;

        rc = ngx_wasm_lua_thread_run(entry_lctx);
        ngx_wa_assert(rc != NGX_OK && rc != NGX_DONE);
        /* NGX_ERROR, NGX_AGAIN */
        if (rc == NGX_ERROR) {
            return NGX_ERROR;
        }
    }

    return NGX_OK;
}


static ngx_inline unsigned
thread_is_dead(ngx_wasm_lua_ctx_t *lctx)
{
    ngx_wasm_subsys_env_t  *env = lctx->env;

    switch (env->subsys->kind) {
#if (NGX_WASM_HTTP)
    case NGX_WASM_SUBSYS_HTTP:
        return !ngx_http_lua_coroutine_alive(lctx->co_ctx);
#endif
#if (NGX_WASM_STREAM)
    case NGX_WASM_SUBSYS_STREAM:
        return !ngx_stream_lua_coroutine_alive(lctx->co_ctx);
#endif
    default:
        ngx_wasm_bad_subsystem(env);
        return 0;
    }
}


static u_char *
thread_cache_key(ngx_pool_t *pool, const char *tag, u_char *src,
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

    /* both subsystems produce an identical md5 hash, use any */
#if (NGX_WASM_HTTP)
    p = ngx_http_lua_digest_hex(p, src, src_len);
#elif (NGX_WASM_STREAM)
    p = ngx_stream_lua_digest_hex(p, src, src_len);
#else
    ngx_wa_assert(0);
    ngx_pfree(pool, out);
    return NULL;
#endif

    *p = '\0';

    return out;
}


static ngx_int_t
thread_init(ngx_wasm_lua_ctx_t *lctx)
{
    ngx_wasm_subsys_env_t  *env = lctx->env;

    switch (env->subsys->kind) {
#if (NGX_WASM_HTTP)
    case NGX_WASM_SUBSYS_HTTP:
    {
        ngx_http_wasm_req_ctx_t  *rctx = env->ctx.rctx;
        ngx_http_request_t       *r = rctx->r;
        ngx_http_lua_ctx_t       *ctx;
        ngx_http_lua_co_ctx_t    *coctx;

        ctx = ngx_http_get_module_ctx(r, ngx_http_lua_module);
        /* existing or created by entry_thread_start */
        ngx_wa_assert(ctx);
        /* preserve ngx_wasm_lua_content_wev_handler */
        ngx_wa_assert(!ctx->entered_content_phase);

        lctx->ctx.rlctx = ctx;

        /* coctx */

        if (!ngx_http_lua_entry_thread_alive(ctx)) {
            /* initializing the fake entry_ctx */
            ngx_wa_assert(lctx->entry);

            ctx->context = NGX_HTTP_LUA_CONTEXT_TIMER;
            coctx = &ctx->entry_co_ctx;

        } else {
            coctx = ngx_http_lua_create_co_ctx(r, ctx);
            if (coctx == NULL) {
                return NGX_ERROR;
            }
        }

        coctx->co = lctx->co;
        coctx->co_ref = lctx->co_ref;
        coctx->co_status = NGX_HTTP_LUA_CO_RUNNING;
        coctx->is_uthread = 1;
#if (NGX_LUA_USE_ASSERT)
        coctx->co_top = 1;
#endif

        if (!lctx->entry) {
            coctx->parent_co_ctx = &ctx->entry_co_ctx;
        }

        lctx->co_ctx = coctx;

        ngx_http_lua_set_req(lctx->co, r);
        ngx_http_lua_attach_co_ctx_to_L(lctx->co, lctx->co_ctx);
        break;
    }
#endif
#if (NGX_WASM_STREAM)
    case NGX_WASM_SUBSYS_STREAM:
        break;
#endif
    default:
        ngx_wasm_bad_subsystem(env);
        return NGX_ERROR;
    }

    return NGX_OK;
}


static ngx_inline ngx_int_t
thread_handle_rc(ngx_wasm_lua_ctx_t *lctx, ngx_int_t rc)
{
    ngx_wasm_subsys_env_t  *env = lctx->env;
    ngx_wasm_lua_ctx_t     *entry_lctx = env->entry_lctx;

    dd("enter (rc: %ld, lctx: %p)", rc, lctx);

    if (!thread_is_dead(lctx)) {
        if (rc == NGX_DONE) {
            /**
             * The ctx->resume_handler can return NGX_DONE when the thread
             * remains in a yielding state, because some Nginx internals expect
             * NGX_DONE while yielding; we confidently override it since the
             * thread is not dead.
             */
            rc = NGX_AGAIN;
        }

    } else {
        /* thread is dead, determine state by checking its return value placed
         * on the stack by OpenResty's uthread implementation */
        ngx_wa_assert(lua_isboolean(lctx->co, 1));

        lctx->co_ctx->co_status = NGX_HTTP_LUA_CO_DEAD;

        rc = lua_toboolean(lctx->co, 1)
             ? NGX_OK      /* thread terminated successfully */
             : NGX_ERROR;  /* thread error */
    }

    dd("rc at switch: %ld", rc);

    switch (rc) {
    case NGX_AGAIN:
        dd("wasm lua thread yield");
        ngx_wa_assert(lctx->yielded);

        if (lctx->entry) {
            /* find the pending sleep timer to cancel at pool cleanup */
            sentinel = ngx_event_timer_rbtree.sentinel;
            root = ngx_event_timer_rbtree.root;

            if (root != sentinel) {
                for (node = ngx_rbtree_min(root, sentinel);
                     node;
                     node = ngx_rbtree_next(&ngx_event_timer_rbtree, node))
                {
                    ev = ngx_rbtree_data(node, ngx_event_t, timer);

                    if (ev->data == entry_lctx->co_ctx) {
                        entry_lctx->ev = ev;
                        break;
                    }
                }
            }
        }

        ngx_wasm_yield(env);
        break;
    case NGX_OK:
        dd("wasm lua thread finished");
        ngx_wa_assert(thread_is_dead(lctx));

        if (!lctx->entry) {
            lctx->finished = 1;
            ngx_queue_remove(&lctx->q);

            if (entry_thread_empty(env)) {
                /* last yielding thread finished */
                ngx_wasm_continue(env);
            }

            if (lctx->success_handler) {
                (void) lctx->success_handler(lctx);
            }
        }

        break;
    case NGX_ERROR:
        dd("wasm lua thread error");
        ngx_wasm_error(env);

        if (!lctx->entry) {
            lctx->finished = 1;
            ngx_queue_remove(&lctx->q);

            if (lctx->error_handler) {
                /* error_handler can override the rc (e.g. lua resolver errors
                 * are ignored by the request flow */
                rc = lctx->error_handler(lctx);
            }
        }

        break;
    default:
        ngx_wasm_log_error(NGX_LOG_WASM_NYI, lctx->log, 0,
                           "unexpected lua resume handler rc: %d", rc);
        ngx_wa_assert(0);
        rc = NGX_ERROR;
        break;
    }

    ngx_wa_assert(rc == NGX_OK
                  || rc == NGX_AGAIN
                  || rc == NGX_ERROR);

#if 0
    /* destroy_thread moved to pool cleanup for extra resiliency */
    if (lctx->finished) {
        destroy_thread(lctx);
    }
#endif

    return rc;
}


static ngx_int_t
thread_resume(ngx_wasm_lua_ctx_t *lctx)
{
    ngx_int_t               rc = NGX_ERROR;
    ngx_wasm_subsys_env_t  *env = lctx->env;

    ngx_log_debug4(NGX_LOG_DEBUG_WASM, lctx->log, 0,
                   "wasm resuming lua%sthread "
                   "(lctx: %p, L: %p, co: %p)",
                   lctx->entry ? " entry " : " user ",
                   lctx, lctx->L, lctx->co);

    switch (env->subsys->kind) {
#if (NGX_WASM_HTTP)
    case NGX_WASM_SUBSYS_HTTP:
    {
        ngx_http_request_t  *r = env->ctx.rctx->r;
        ngx_http_lua_ctx_t  *ctx = lctx->ctx.rlctx;

        ngx_wa_assert(ctx == ngx_http_get_module_ctx(r, ngx_http_lua_module));

        rc = ctx->resume_handler(r);
        break;
    }
#endif
#if (NGX_WASM_STREAM)
    case NGX_WASM_SUBSYS_STREAM:
        break;
#endif
    default:
        ngx_wasm_bad_subsystem(env);
        return NGX_ERROR;
    }

    dd("lua%sthread resume handler rc: %ld",
       lctx->entry ? " entry " : " user ", rc);

    return thread_handle_rc(lctx, rc);
}


ngx_wasm_lua_ctx_t *
ngx_wasm_lua_thread_new(const char *tag, const char *src,
    ngx_wasm_subsys_env_t *env, ngx_log_t *log, void *data,
    ngx_wasm_lua_handler_pt success_handler,
    ngx_wasm_lua_handler_pt error_handler)
{
    ngx_int_t            rc;
    ngx_pool_t          *pool;
    ngx_pool_cleanup_t  *cln;
    ngx_wasm_lua_ctx_t  *lctx;

    pool = env->ctx.rctx->r->pool;

    /* lctx */

    lctx = ngx_pcalloc(pool, sizeof(ngx_wasm_lua_ctx_t));
    if (lctx == NULL) {
        return NULL;
    }

    lctx->pool = pool;
    lctx->log = log;
    lctx->env = env;
    lctx->data = data;
    lctx->success_handler = success_handler;
    lctx->error_handler = error_handler;

    if (src == WASM_LUA_ENTRY_SCRIPT) {
        lctx->entry = 1;
        ngx_queue_init(&lctx->sub_ctxs);
    }

    cln = ngx_pool_cleanup_add(lctx->pool, 0);
    if (cln == NULL) {
        goto error;
    }

    cln->handler = thread_cleanup_handler;
    cln->data = lctx;

    ngx_log_debug2(NGX_LOG_DEBUG_WASM, lctx->log, 0,
                   "wasm creating new lua%sthread (lctx: %p)",
                   lctx->entry ? " entry " : " user ", lctx);

    /* Lua VM + thread */

    switch (env->subsys->kind) {
#if (NGX_WASM_HTTP)
    case NGX_WASM_SUBSYS_HTTP:
    {
        ngx_http_request_t  *r = env->ctx.rctx->r;

        lctx->L = ngx_http_lua_get_lua_vm(r, NULL);
        lctx->co = ngx_http_lua_new_thread(r, lctx->L, &lctx->co_ref);
        break;
    }
#endif
#if (NGX_WASM_STREAM)
    case NGX_WASM_SUBSYS_STREAM:
        break;
#endif
    default:
        ngx_wasm_bad_subsystem(env);
        goto error;
    }

    if (lctx->L == NULL || lctx->co == NULL) {
        goto error;
    }
    /* code */

    lctx->code = src;
    lctx->code_len = ngx_strlen(src);
    lctx->cache_key = thread_cache_key(lctx->pool, tag,
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
        ngx_wasm_bad_subsystem(env);
        goto error;
    }

    if (rc != NGX_OK) {
        goto error;
    }

    /*  move code closure to new coroutine  */
    lua_xmove(lctx->L, lctx->co, 1);

    return lctx;

error:

    if (cln == NULL) {
        destroy_thread(lctx);
    }

    return NULL;
}


/**
 * Return values:
 * NGX_OK:     lua thread finished
 * NGX_AGAIN:  lua thread yielding
 * NGX_ERROR:  error
 */
ngx_int_t
ngx_wasm_lua_thread_run(ngx_wasm_lua_ctx_t *lctx)
{
    ngx_int_t               rc = NGX_ERROR;
    ngx_wasm_subsys_env_t  *env = lctx->env;
    ngx_wasm_lua_ctx_t     *entry_lctx;

    ngx_log_debug4(NGX_LOG_DEBUG_WASM, lctx->log, 0,
                   "wasm running lua%sthread (lctx: %p, L: %p, co: %p)",
                   lctx->entry ? " entry " : " user ",
                   lctx, lctx->L, lctx->co);

    if (env->entry_lctx == NULL) {
        rc = entry_thread_start(env);
        if (rc != NGX_OK) {
            return NGX_ERROR;
        }
    }

    entry_lctx = env->entry_lctx;

    if (thread_init(lctx) != NGX_OK) {
        return NGX_ERROR;
    }

    /* lua ctx */

    switch (env->subsys->kind) {
#if (NGX_WASM_HTTP)
    case NGX_WASM_SUBSYS_HTTP:
    {
        ngx_http_request_t       *r = env->ctx.rctx->r;
        ngx_http_lua_ctx_t       *ctx = lctx->ctx.rlctx;
        ngx_http_wasm_req_ctx_t  *rctx;

        ctx->cur_co_ctx = lctx->co_ctx;

        if (!lctx->entry) {
            ctx->uthreads++;
        }

        rc = ngx_http_lua_run_thread(lctx->L, r, ctx, lctx->nargs);
        if (rc == NGX_AGAIN) {
            rctx = ngx_http_get_module_ctx(r, ngx_http_wasm_module);
            rctx->resume_handler = ngx_http_wasm_lua_resume_handler;
        }

        break;
    }
#endif
#if (NGX_WASM_STREAM)
    case NGX_WASM_SUBSYS_STREAM:
        break;
#endif
    default:
        ngx_wasm_bad_subsystem(env);
        return NGX_ERROR;
    }

    dd("lua_run_thread rc: %ld", rc);

    if (rc == NGX_AGAIN && !lua_isboolean(lctx->co, 1)) {
        lctx->yielded = 1;
    }

    if (entry_lctx && !lctx->entry) {
        ngx_queue_insert_tail(&entry_lctx->sub_ctxs, &lctx->q);
    }

    return thread_handle_rc(lctx, rc);
}


/**
 * Return values:
 * NGX_OK:     no lua thread to run
 * NGX_AGAIN:  lua thread yielding
 * NGX_ERROR:  error
 */
ngx_int_t
ngx_wasm_lua_resume(ngx_wasm_subsys_env_t *env)
{
    ngx_int_t               rc;
    ngx_queue_t            *q;
    ngx_http_lua_ctx_t     *ctx;
    ngx_http_lua_co_ctx_t  *coctx;
    ngx_wasm_lua_ctx_t     *lctx = NULL;
    ngx_wasm_lua_ctx_t     *entry_lctx = env->entry_lctx;

    dd("enter");

    if (entry_thread_empty(env)) {
        return NGX_OK;
    }

    ctx = ngx_http_get_module_ctx(env->ctx.rctx->r, ngx_http_lua_module);
    if (ctx == NULL) {
        return NGX_ERROR;
    }

    coctx = ctx->cur_co_ctx;
    if (coctx == NULL) {
        dd("no current context");
        return NGX_OK;
    }

    if (coctx == entry_lctx->co_ctx) {
        /* the entry thread is resuming */
        lctx = entry_lctx;

    } else {
        /* one of our user threads is resuming */

        for (q = ngx_queue_head(&entry_lctx->sub_ctxs);
             q != ngx_queue_sentinel(&entry_lctx->sub_ctxs);
             q = ngx_queue_next(q))
        {
            lctx = ngx_queue_data(q, ngx_wasm_lua_ctx_t, q);

            if (lctx->co_ctx == coctx) {
                break;
            }

            lctx = NULL;
        }

        if (lctx == NULL) {
            ngx_wasm_log_error(NGX_LOG_CRIT, env->connection->log, 0,
                               "wasm lua bridge could not find resuming "
                               "coroutine context");
            return NGX_ERROR;
        }

        ngx_wa_assert(lctx->yielded);
        ngx_wa_assert(coctx != &ctx->entry_co_ctx);
    }

    dd("resuming%slctx: %p", lctx->entry ? " entry " : " user ", lctx);

    rc = thread_resume(lctx);
    if (rc == NGX_ERROR) {
        return NGX_ERROR;
    }

    dd("rc: %ld, state: %d", rc, env->state);

    switch (env->state) {
    case NGX_WASM_STATE_YIELD:
        rc = NGX_AGAIN;
        break;
    case NGX_WASM_STATE_ERROR:
    case NGX_WASM_STATE_CONTINUE:
        rc = NGX_OK;
        break;
    default:
        rc = NGX_ERROR;
        break;
    }

    dd("exit (rc: %ld)", rc);

    return rc;
}


#if (NGX_WASM_HTTP)
static ngx_int_t
ngx_http_wasm_lua_resume_handler(ngx_http_request_t *r)
{
    ngx_int_t                 rc;
    ngx_http_wasm_req_ctx_t  *rctx;

    rctx = ngx_http_get_module_ctx(r, ngx_http_wasm_module);
    if (rctx == NULL) {
        return NGX_ERROR;
    }

    rc = ngx_wasm_lua_resume(&rctx->env);
    if (rc != NGX_AGAIN) {
        rctx->resume_handler = NULL;
    }

    return rc;
}
#endif
