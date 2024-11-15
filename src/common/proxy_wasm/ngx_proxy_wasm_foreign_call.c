#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#if (NGX_WASM_LUA)
#include <ngx_wasm_lua_resolver.h>
#endif
#include <ngx_proxy_wasm_foreign_call.h>


void
ngx_proxy_wasm_foreign_call_destroy(ngx_proxy_wasm_foreign_call_t *call)
{
    ngx_pfree(call->pwexec->pool, call);
}


#if (NGX_WASM_HTTP)
#if (NGX_WASM_LUA)
static void
ngx_proxy_wasm_foreign_callback(ngx_proxy_wasm_dispatch_op_t *dop)
{
    ngx_proxy_wasm_step_e           step;
    ngx_proxy_wasm_exec_t          *pwexec;
    ngx_proxy_wasm_foreign_call_t  *call;

    ngx_wa_assert(dop->type == NGX_PROXY_WASM_DISPATCH_FOREIGN_CALL);

    call = dop->call.foreign;
    pwexec = call->pwexec;

    /* set current call for host functions */
    pwexec->foreign_call = call;

    /* save step */
    step = pwexec->parent->step;

    ngx_queue_remove(&dop->q);

    pwexec->parent->phase = ngx_wasm_phase_lookup(&ngx_http_wasm_subsystem,
                                                  NGX_WASM_BACKGROUND_PHASE);

    /**
     * Potential trap ignored as it is already logged and no futher handling is
     * needed.
     */
    (void) ngx_proxy_wasm_run_step(pwexec,
                                   NGX_PROXY_WASM_STEP_FOREIGN_CALLBACK);


    /* reset step */
    pwexec->parent->step = step;

    /* remove current call now that callback was invoked */
    pwexec->foreign_call = NULL;

    if (ngx_proxy_wasm_dispatch_ops_total(pwexec)) {
        ngx_log_debug0(NGX_LOG_DEBUG_WASM, pwexec->log, 0,
                       "proxy_wasm more dispatch operations pending...");

        ngx_wasm_yield(&call->rctx->env);
        ngx_proxy_wasm_ctx_set_next_action(pwexec->parent,
                                           NGX_PROXY_WASM_ACTION_PAUSE);

    } else {
        ngx_log_debug0(NGX_LOG_DEBUG_WASM, pwexec->log, 0,
                       "proxy_wasm last foreign function callback handled");

        ngx_wasm_continue(&call->rctx->env);
        ngx_proxy_wasm_ctx_set_next_action(pwexec->parent,
                                           NGX_PROXY_WASM_ACTION_CONTINUE);

        ngx_proxy_wasm_resume(pwexec->parent, pwexec->parent->phase, step);
    }

    ngx_proxy_wasm_foreign_call_destroy(call);
}


static void
ngx_proxy_wasm_hfuncs_resolve_lua_handler(ngx_resolver_ctx_t *rslv_ctx)
{
#if (NGX_HAVE_INET6)
    struct sockaddr_in6            *sin6;
#endif
    struct sockaddr_in             *sin;
    u_char                         *p;
    u_short                         sa_family = AF_INET;
    ngx_str_t                       args;
    ngx_buf_t                      *b;
    ngx_wasm_socket_tcp_t          *sock = rslv_ctx->data;
    ngx_wasm_lua_ctx_t             *lctx = sock->lctx;
    ngx_proxy_wasm_dispatch_op_t   *dop = sock->data;
    ngx_proxy_wasm_foreign_call_t  *call = dop->call.foreign;
    u_char                          buf[rslv_ctx->name.len +
#if (NGX_HAVE_INET6)
                                        sizeof(struct in6_addr) + 1];
#else
                                        sizeof(struct in_addr) + 1];
#endif

    ngx_memzero(buf, sizeof(buf));
    p = buf;

    if (rslv_ctx->addr.socklen == sizeof(struct sockaddr_in6)) {
        sa_family = AF_INET6;
    }

    if (rslv_ctx->state || !rslv_ctx->naddrs) {
        p++;  /* buffer's 1st byte is address length; 0 if address not found */
        goto not_found;
    }

    ngx_wa_assert(rslv_ctx->naddrs == 1);

    switch (sa_family) {
#if (NGX_HAVE_INET6)
    case AF_INET6:
          sin6 = (struct sockaddr_in6 *) rslv_ctx->addr.sockaddr;
          *(p++) = sizeof(struct in6_addr);
          p = ngx_cpymem(p, &sin6->sin6_addr, sizeof(struct in6_addr));
          break;
#endif
    default: /* AF_INET */
          sin = (struct sockaddr_in *) rslv_ctx->addr.sockaddr;
          *(p++) = sizeof(struct in_addr);
          p = ngx_cpymem(p, &sin->sin_addr, sizeof(struct in_addr));
    }

not_found:

    p = ngx_cpymem(p, rslv_ctx->name.data, rslv_ctx->name.len);
    args.data = buf;
    args.len = p - buf;

    call->args_out = ngx_wasm_chain_get_free_buf(call->pwexec->pool,
                                                 &call->rctx->free_bufs,
                                                 args.len, buf_tag, 1);
    if (call->args_out == NULL) {
        goto error;
    }

    b = call->args_out->buf;
    b->last = ngx_cpymem(b->last, args.data, args.len);

    if (lctx->yielded) {
        ngx_proxy_wasm_foreign_callback(dop);

        if (rslv_ctx->state == NGX_WASM_LUA_RESOLVE_ERR) {
            ngx_wasm_resume(&call->rctx->env);
        }
    }

error:

    ngx_free(rslv_ctx);
}
#endif  /* NGX_WASM_LUA */


ngx_int_t
ngx_proxy_wasm_foreign_call_resolve_lua(ngx_wavm_instance_t *instance,
    ngx_http_wasm_req_ctx_t *rctx, ngx_str_t *fargs, ngx_wavm_ptr_t *ret_data,
    int32_t *ret_size, wasm_val_t rets[])
{
    ngx_proxy_wasm_exec_t  *pwexec;

    pwexec = ngx_proxy_wasm_instance2pwexec(instance);

#if (!NGX_WASM_LUA)
    return ngx_proxy_wasm_result_trap(pwexec,
                                      "cannot resolve, no lua support",
                                      rets, NGX_WAVM_ERROR);

#else
    size_t                          s;
    ngx_int_t                       rc;
    ngx_buf_t                      *b;
    ngx_resolver_ctx_t             *rslv_ctx;
    ngx_wasm_core_conf_t           *wcf;
    ngx_wasm_socket_tcp_t          *sock;
    ngx_proxy_wasm_dispatch_op_t   *dop;
    ngx_proxy_wasm_foreign_call_t  *call;
    ngx_wavm_ptr_t                  p;

    /* check context */

    switch (pwexec->parent->step) {
    case NGX_PROXY_WASM_STEP_REQ_HEADERS:
    case NGX_PROXY_WASM_STEP_REQ_BODY:
    case NGX_PROXY_WASM_STEP_TICK:
    case NGX_PROXY_WASM_STEP_DISPATCH_RESPONSE:
    case NGX_PROXY_WASM_STEP_FOREIGN_CALLBACK:
        break;
    default:
        return ngx_proxy_wasm_result_trap(pwexec,
                                          "can only call resolve_lua "
                                          "during "
                                          "\"on_request_headers\", "
                                          "\"on_request_body\", "
                                          "\"on_tick\", "
                                          "\"on_dispatch_response\", "
                                          "\"on_foreign_function\"",
                                          rets, NGX_WAVM_BAD_USAGE);
    }

    /* check name */

    if (!fargs->len) {
        return ngx_proxy_wasm_result_trap(pwexec,
                                          "cannot resolve, missing name",
                                          rets, NGX_WAVM_BAD_USAGE);
    }

    call = ngx_pcalloc(pwexec->pool, sizeof(ngx_proxy_wasm_foreign_call_t));
    if (call == NULL) {
        goto error;
    }

    call->pwexec = pwexec;
    call->fcode = NGX_PROXY_WASM_FOREIGN_RESOLVE_LUA;

    /* rctx or fake rctx */

    if (rctx == NULL
        && ngx_http_wasm_create_fake_rctx(pwexec, &rctx) != NGX_OK)
    {
        goto error;
    }

    call->rctx = rctx;

    /* dispatch */

    dop = ngx_pcalloc(pwexec->pool, sizeof(ngx_proxy_wasm_dispatch_op_t));
    if (dop == NULL) {
        goto error;
    }

    dop->type = NGX_PROXY_WASM_DISPATCH_FOREIGN_CALL;
    dop->call.foreign = call;

    sock = ngx_pcalloc(pwexec->pool, sizeof(ngx_wasm_socket_tcp_t));
    if (sock == NULL) {
        goto error;
    }

    sock->env = &call->rctx->env;
    sock->log = pwexec->log;
    sock->pool = pwexec->pool;
    sock->data = dop;

    /* resolve */

    wcf = ngx_wasm_core_cycle_get_conf(ngx_cycle);
    if (wcf == NULL) {
        goto error;
    }

    rslv_ctx = ngx_resolve_start(wcf->resolver, NULL);
    if (rslv_ctx == NULL || rslv_ctx == NGX_NO_RESOLVER) {
        goto error;
    }

    rslv_ctx->name.data = fargs->data;
    rslv_ctx->name.len = fargs->len;
    rslv_ctx->handler = ngx_proxy_wasm_hfuncs_resolve_lua_handler;
    rslv_ctx->data = sock;

    rc = ngx_wasm_lua_resolver_resolve(rslv_ctx);

    switch (rc) {
    case NGX_OK:
        b = call->args_out->buf;
        s = *b->start;

        p = ngx_proxy_wasm_alloc(pwexec, s);
        if (p == 0) {
            goto error;
        }

        if (!ngx_wavm_memory_memcpy(instance->memory, p, b->start + 1, s)) {
            return ngx_proxy_wasm_result_invalid_mem(rets);
        }

        *ret_data = p;
        *ret_size = s;

        return ngx_proxy_wasm_result_ok(rets);

    case NGX_AGAIN:
        ngx_queue_insert_head(&pwexec->dispatch_ops, &dop->q);
        return ngx_proxy_wasm_result_ok(rets);

    default:
        ngx_wa_assert(rc == NGX_ERROR);
        /* rslv_ctx is freed by ngx_wasm_lua_resolver_resolve */
        break;
    }

error:

    return ngx_proxy_wasm_result_trap(pwexec, "failed resolving name",
                                      rets, NGX_WAVM_ERROR);
#endif
}
#endif  /* NGX_WASM_HTTP */
