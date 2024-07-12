/**
 * DNS resolution for Proxy-Wasm filters running in Kong Gateway.
 *
 * ngx_wasmx_module handles DNS resolution via Nginx's configured `resolver`
 * (http{}, wasm{}, or default from ngx_wasm.h). However, Kong Gateway's DNS
 * resolution is handled by the Gateway's Lua-land resolver: resty.dns.client.
 *
 * To ensure DNS resolution consistency between Kong Gateway core/plugins and
 * Proxy-Wasm filters, this file leverages our Wasm/Lua bridge to invoke the
 * Gateway's Lua-land resolver in lieu of the configured Nginx resolver when
 * built against OpenResty and enabled via `proxy_wasm_lua_resolver`.
 */

#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_wasm_lua_resolver.h>
#if (NGX_WASM_HTTP)
#include <ngx_http_proxy_wasm_dispatch.h>
#endif


static const char  *DNS_SOLVING_SCRIPT_NAME = "wasm_lua_resolver_chunk";
static const char  *DNS_SOLVING_SCRIPT = ""
    "local plctx, pname, pnameserver, timeout = ...                       \n"
    "local ffi = require('ffi')                                           \n"
    "local fmt = string.format                                            \n"
    "                                                                     \n"
    "ffi.cdef[[                                                           \n"
    "    typedef unsigned char  u_char;                                   \n"
    "    typedef void           ngx_wasm_lua_ctx_t;                       \n"
    "                                                                     \n"
    "    void ngx_wasm_lua_resolver_handler(ngx_wasm_lua_ctx_t *lctx,     \n"
    "        u_char *ip, unsigned int port, unsigned ipv6);               \n"
    "]]                                                                   \n"
    "                                                                     \n"
    "ngx.log(ngx.DEBUG, 'wasm lua resolver thread')                       \n"
    "                                                                     \n"
    "local client                                                         \n"
    "local name = ffi.string(pname)                                       \n"
    "local nameserver = ffi.string(pnameserver)                           \n"
    "                                                                     \n"
    "if dns_client then                                                   \n"
    "    ngx.log(ngx.DEBUG, 'wasm lua resolver using existing dns_client')\n"
    "                                                                     \n"
    "    client = dns_client                                              \n"
    "                                                                     \n"
    "else                                                                 \n"
    "    ngx.log(ngx.DEBUG, 'wasm lua resolver creating new dns_client')  \n"
    "                                                                     \n"
    "    client = require('resty.dns.client')                             \n"
    "    local ok, err = client.init({                                    \n"
    "        hosts = {},                                                  \n"
    "        resolvConf = {                                               \n"
    "            'nameserver '      .. nameserver,                        \n"
    "            'options timeout:' .. timeout,                           \n"
#if 1
    "            'options attempts:1',                                    \n"
    "        },                                                           \n"
    "        order = { 'A' },                                             \n"
#else
    "       },                                                            \n"
#endif
    "    })                                                               \n"
    "    if not ok then                                                   \n"
    "        error(fmt('wasm lua failed initializing dns_client ' ..      \n"
    "                  'while resolving \"%s\": %s', name, err))          \n"
    "    end                                                              \n"
    "end                                                                  \n"
    "                                                                     \n"
    "ngx.log(ngx.DEBUG, 'wasm lua resolving \"', name, '\"')              \n"
    "                                                                     \n"
    "local ip, err = client.toip(name)                                    \n"
    "if ip == nil then                                                    \n"
    "    error(fmt('wasm lua failed resolving \"%s\": %s', name, err))    \n"
    "end                                                                  \n"
    "                                                                     \n"
    "local ipv6 = false                                                   \n"
    "local port = type(err) == 'number' and err or 0                      \n"
    "                                                                     \n"
    "if string.sub(ip, 1, 1) == '[' then                                  \n"
    "    ipv6 = true                                                      \n"
    "    ip = string.sub(ip, 2, -2)                                       \n"
    "end                                                                  \n"
    "                                                                     \n"
    "ngx.log(ngx.DEBUG, fmt('wasm lua resolved %q to \"%s\"',             \n"
    "                       name, port > 0 and ip .. ':' .. port or ip))  \n"
    "                                                                     \n"
    "local c_ip = ffi.new('u_char[?]', #ip + 1)                           \n"
    "ffi.copy(c_ip, ip)                                                   \n"
    "                                                                     \n"
    "ffi.C.ngx_wasm_lua_resolver_handler(plctx, c_ip, port, ipv6)         \n";


static ngx_int_t
ngx_wasm_lua_resolver_error_handler(ngx_wasm_lua_ctx_t *lctx)
{
    ngx_resolver_ctx_t  *rslv_ctx = lctx->data;

    dd("enter");

#if (DDEBUG)
    if (!lctx->cancelled) {
        ngx_wa_assert(lctx->co_ctx->co_status == NGX_HTTP_LUA_CO_DEAD);
    }

    ngx_wa_assert(!rslv_ctx->naddrs);
#endif

    if (lctx->yielded || lctx->cancelled) {
        /* re-enter normal resolver path (handler) if we yielded */
        rslv_ctx->state = NGX_WASM_LUA_RESOLVE_ERR;

        rslv_ctx->handler(rslv_ctx);

        dd("exit");

        /* resolver error/timeout/NXDOMAIN do not produce errors for
         * pwm_lua_resolver to behave like native resolver errors (do not
         * interrupt request) */
        return NGX_OK;
    }

    dd("exit");

    return NGX_ERROR;
}


static ngx_int_t
ngx_wasm_lua_resolver_success_handler(ngx_wasm_lua_ctx_t *lctx)
{
    ngx_resolver_ctx_t  *rslv_ctx = lctx->data;

    dd("enter (rslv_ctx:%p)", rslv_ctx);

    /* resolution should have succeeded */
    ngx_wa_assert(rslv_ctx->naddrs);

    rslv_ctx->handler(rslv_ctx);

    dd("exit");

    return NGX_OK;
}


ngx_int_t
ngx_wasm_lua_resolver_resolve(ngx_resolver_ctx_t *rslv_ctx)
{
    ngx_int_t               rc;
    u_char                 *host, *p;
    ngx_wasm_lua_ctx_t     *lctx;
    ngx_wasm_socket_tcp_t  *sock = rslv_ctx->data;

    /* lctx */

    dd("resolving with rslv_ctx: %p", rslv_ctx);

    lctx = ngx_wasm_lua_thread_new(DNS_SOLVING_SCRIPT_NAME,
                                   DNS_SOLVING_SCRIPT,
                                   sock->env,
                                   sock->log,
                                   rslv_ctx,
                                   ngx_wasm_lua_resolver_success_handler,
                                   ngx_wasm_lua_resolver_error_handler);
    if (lctx == NULL) {
        return NGX_ERROR;
    }

    sock->lctx = lctx;

    /* args */

    host = ngx_palloc(lctx->pool, rslv_ctx->name.len + 1);
    if (host == NULL) {
        goto error;
    }

    p = ngx_cpymem(host, rslv_ctx->name.data, rslv_ctx->name.len);
    *p++ = '\0';

    /* args: lctx, host, nameserver, timeout */

    lctx->nargs = 4;
    lua_pushlightuserdata(lctx->co, lctx);
    lua_pushstring(lctx->co, (char *) host);
    lua_pushstring(lctx->co, (char *) NGX_WASM_DEFAULT_RESOLVER_IP);
    lua_pushnumber(lctx->co, NGX_WASM_DEFAULT_RESOLVER_TIMEOUT / 1000);

    /* run */

    rc = ngx_wasm_lua_thread_run(lctx);
    if (rc == NGX_ERROR) {
        goto error;
    }

    dd("exit (rc: %ld)", rc);

    ngx_wa_assert(rc == NGX_OK || rc == NGX_AGAIN);

    return rc;

error:

    dd("error exit");

    ngx_free(rslv_ctx);

    return NGX_ERROR;
}


void
ngx_wasm_lua_resolver_handler(ngx_wasm_lua_ctx_t *lctx, u_char *ip,
    unsigned int port, unsigned ipv6)
{
#if (NGX_HAVE_INET6)
    struct sockaddr_in6    *sin6;
#endif
    struct sockaddr_in     *sin;
    ngx_resolver_ctx_t     *rslv_ctx = lctx->data;
    ngx_wasm_socket_tcp_t  *sock = rslv_ctx->data;
    u_short                 sa_family = AF_INET;

    dd("enter");

    if (ipv6) {
#if (NGX_HAVE_INET6)
        sa_family = AF_INET6;
#else
        goto error;
#endif
    }

    switch (sa_family) {
#if (NGX_HAVE_INET6)
    case AF_INET6:
        sin6 = ngx_pcalloc(sock->pool, sizeof(struct sockaddr_in6));
        if (sin6 == NULL) {
            goto error;
        }

        sin6->sin6_family = AF_INET6;

        if (ngx_inet6_addr(ip, ngx_strlen(ip), sin6->sin6_addr.s6_addr)
            != NGX_OK)
        {
            ngx_pfree(sock->pool, sin6);
            goto error;
        }

        rslv_ctx->addr.socklen = sizeof(struct sockaddr_in6);
        rslv_ctx->addr.sockaddr = (struct sockaddr *) sin6;
        rslv_ctx->addr.sockaddr->sa_family = AF_INET6;
        break;
#endif
    default: /* AF_INET */
        sin = ngx_pcalloc(sock->pool, sizeof(struct sockaddr_in));
        if (sin == NULL) {
            goto error;
        }

        sin->sin_family = AF_INET;
        sin->sin_addr.s_addr = ngx_inet_addr(ip, ngx_strlen(ip));
        if (sin->sin_addr.s_addr == INADDR_NONE) {
            ngx_pfree(sock->pool, sin);
            goto error;
        }

        rslv_ctx->addr.socklen = sizeof(struct sockaddr_in);
        rslv_ctx->addr.sockaddr = (struct sockaddr *) sin;
        rslv_ctx->addr.sockaddr->sa_family = AF_INET;
        break;
    }

    ngx_inet_set_port(rslv_ctx->addr.sockaddr, port);

    rslv_ctx->naddrs = 1;
    rslv_ctx->addrs = &rslv_ctx->addr;

    dd("exit");

    /* end of the lua thread */

    return;

error:

    rslv_ctx->state = NGX_ERROR;

    rslv_ctx->handler(rslv_ctx);

    dd("exit");

    /* end of the lua thread, the handler only updated the socket
     * error */
}
