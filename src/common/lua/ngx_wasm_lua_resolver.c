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
    "        u_char *ip);                                                 \n"
    "]]                                                                   \n"
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
    "        },                                                           \n"
    "        noSynchronisation = true,                                    \n"
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
    "ngx.log(ngx.DEBUG, 'wasm lua resolved ',                             \n"
    "                   '\"', name, '\" to \"', ip, '\"')                 \n"
    "                                                                     \n"
    "local c_ip = ffi.new('u_char[?]', #ip + 1)                           \n"
    "ffi.copy(c_ip, ip)                                                   \n"
    "                                                                     \n"
    "ffi.C.ngx_wasm_lua_resolver_handler(plctx, c_ip)                     \n";


ngx_int_t
ngx_wasm_lua_resolver_resolve(ngx_resolver_ctx_t *rslv_ctx)
{
    u_char                 *host, *p;
    ngx_int_t               rc;
    ngx_wasm_lua_ctx_t     *lctx;
    ngx_wasm_socket_tcp_t  *sock = rslv_ctx->data;

    /* lctx */

    lctx = ngx_wasm_lua_thread_new(DNS_SOLVING_SCRIPT_NAME,
                                   DNS_SOLVING_SCRIPT,
                                   &sock->env,
                                   sock->log,
                                   rslv_ctx);
    if (lctx == NULL) {
        return NGX_ERROR;
    }

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

    dd("thread run rc: %ld", rc);

    switch (rc) {
    case NGX_AGAIN:
        ngx_wasm_set_resume_handler(&sock->env);
        /* fallthrough */
    case NGX_OK:
        break;
    default:
        goto error;
    }

    return rc;

error:

    ngx_wasm_lua_thread_destroy(lctx);

    return NGX_ERROR;
}


void
ngx_wasm_lua_resolver_handler(ngx_wasm_lua_ctx_t *lctx, u_char *ip)
{
    struct sockaddr_in     *sin;
    ngx_resolver_ctx_t     *rslv_ctx = lctx->data;
    ngx_wasm_socket_tcp_t  *sock = rslv_ctx->data;

    dd("enter");

    rslv_ctx->addr.sockaddr = ngx_pcalloc(sock->pool,
                                          sizeof(struct sockaddr_in));
    if (rslv_ctx->addr.sockaddr == NULL) {
        return;
    }

    rslv_ctx->naddrs = 1;
    rslv_ctx->addrs = &rslv_ctx->addr;
    rslv_ctx->addr.socklen = sizeof(struct sockaddr_in);
    rslv_ctx->addr.sockaddr->sa_family = AF_INET;

    sin = (struct sockaddr_in *) rslv_ctx->addr.sockaddr;
    sin->sin_family = AF_INET;

    inet_pton(AF_INET, (char *) ip, &sin->sin_addr);

    ngx_wasm_lua_thread_destroy(lctx);

    rslv_ctx->handler(rslv_ctx);

    dd("exit");
}
