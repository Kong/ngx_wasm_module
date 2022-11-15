#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_wasm_lua.h>
#include <ngx_wasm_lua_resolver.h>

#if (NGX_WASM_HTTP)
#include <ngx_http_proxy_wasm_dispatch.h>
#endif


typedef struct {
    void  *resolver_ctx;
    char  *name;
    int    port;
} ngx_wasm_lua_ffi_resolver_ctx_t;


static const u_char *DNS_SOLVING_SCRIPT = (u_char *) ""
    "local data = ...                                                       \n"
    "local ffi = require ('ffi')                                            \n"
    "local client                                                           \n"
    "if dns_client then                                                     \n"
    "    ngx.log(ngx.DEBUG, 'lua resolver using existing dns_client')       \n"
    "    client = dns_client                                                \n"
    "else                                                                   \n"
    "    client = require ('resty.dns.client')                              \n"
    "    client.init({noSynchronisation = true})                            \n"
    "end                                                                    \n"
    "                                                                       \n"
    "ffi.cdef[[                                                             \n"
    "  typedef struct {                                                     \n"
    "      void  *resolver_ctx;                                             \n"
    "      char  *name;                                                     \n"
    "      int    port;                                                     \n"
    "  } ngx_wasm_lua_ffi_resolver_ctx_t;                                   \n"
    "  void ngx_wasm_lua_resolver_handler(                                  \n"
    "    ngx_wasm_lua_ffi_resolver_ctx_t *ctx, const char *ip, int port);   \n"
    "]]                                                                     \n"
    "                                                                       \n"
    "local ctx = ffi.cast('ngx_wasm_lua_ffi_resolver_ctx_t *', data)        \n"
    "local name = ffi.string(ctx.name)                                      \n"
    "                                                                       \n"
    "local ip, port = client.toip(name, ctx.port)                           \n"
    "ffi.C.ngx_wasm_lua_resolver_handler(data, ip, port)                    \n";


static size_t         DNS_SOLVING_SCRIPT_LEN;
static const u_char  *DNS_SOLVING_SCRIPT_KEY;
static const char    *DNS_SOLVING_SCRIPT_NAME = "wasm_lua_resolver";


static ngx_inline void
ngx_wasm_lua_resolver_set_resume_handler(ngx_wasm_socket_tcp_t *sock)
{
#if (NGX_WASM_HTTP)
    ngx_http_request_t       *r;
    ngx_http_wasm_req_ctx_t  *rctx;

    if (sock->kind == NGX_WASM_SOCKET_TCP_KIND_HTTP) {
        rctx = sock->env.ctx.request;
        r = rctx->r;

        if (rctx->entered_content_phase || rctx->fake_request) {
            r->write_event_handler = ngx_http_wasm_content_wev_handler;

        } else {
            r->write_event_handler = ngx_http_core_run_phases;
        }
    }
#endif
}


static u_char *
ngx_wasm_lua_resolver_cache_key(ngx_pool_t *pool, const char *tag,
    const u_char *src, size_t src_len)
{
    u_char  *p, *out;
    size_t   tag_len;

    tag_len = ngx_strlen(tag);

    out = ngx_pnalloc(pool, tag_len);
    if (out == NULL) {
        ngx_wasm_log_error(NGX_LOG_ERR, ngx_cycle->log, 0,
                           "failed allocating memory for code cache key in "
                           "lua_resolver");
        return NULL;
    }

    p = ngx_copy(out, tag, tag_len);

#if (NGX_WASM_HTTP)
    p = ngx_http_lua_digest_hex(p, src, src_len);
#elif (0 && NGX_WASM_STREAM)
    p = ngx_stream_lua_digest_hex(p, src, src_len);
#endif

    *p = '\0';

    return out;
}


void
ngx_wasm_lua_resolver_handler(ngx_wasm_lua_ffi_resolver_ctx_t *ctx,
    const char *ip, int port)
{
    struct sockaddr_in     *sin;
    ngx_resolver_ctx_t     *resolver_ctx = ctx->resolver_ctx;
    ngx_wasm_socket_tcp_t  *sock = resolver_ctx->data;

    resolver_ctx->addr.sockaddr = ngx_pcalloc(sock->pool,
                                              sizeof(struct sockaddr_in));
    if (!resolver_ctx->addr.sockaddr) {
        ngx_wasm_log_error(NGX_LOG_ERR, sock->log, 0,
                           "failed allocating sockaddr in lua_resolver");
        return;
    }

    sin = (struct sockaddr_in *) resolver_ctx->addr.sockaddr;

    resolver_ctx->naddrs = 1;
    resolver_ctx->addrs = &resolver_ctx->addr;
    resolver_ctx->addr.socklen = sizeof(struct sockaddr_in);
    resolver_ctx->addr.sockaddr->sa_family = AF_INET;

    sin->sin_family = AF_INET;
    sin->sin_port = port;
    inet_pton(AF_INET, ip, &sin->sin_addr);

    ngx_wasm_lua_resolver_set_resume_handler(sock);

    resolver_ctx->handler(resolver_ctx);
}


static ngx_wasm_lua_ffi_resolver_ctx_t *
ngx_wasm_lua_resolver_create_ctx(ngx_wasm_socket_tcp_t *sock,
    ngx_resolver_ctx_t *resolver_ctx)
{
    size_t                            name_len;
    char                             *p;
    ngx_wasm_lua_ffi_resolver_ctx_t  *ffi_ctx;

    ffi_ctx = ngx_palloc(sock->pool, sizeof(ngx_wasm_lua_ffi_resolver_ctx_t));
    if (!ffi_ctx) {
        return NULL;
    }

    p = (char *) resolver_ctx->name.data;
    name_len = (strchrnul(p, ':') - p);
    resolver_ctx->name.data[name_len] = '\0';

    ffi_ctx->resolver_ctx = resolver_ctx;
    ffi_ctx->name = (char *) resolver_ctx->name.data;
    ffi_ctx->port = sock->resolved.port;

    return ffi_ctx;
}


static ngx_int_t
ngx_wasm_lua_resolver_create_lua_ctx(ngx_wasm_socket_tcp_t *sock,
                                     ngx_wasm_lua_ctx_t **out)
{
    (*out) = ngx_palloc(sock->pool, sizeof(ngx_wasm_lua_ctx_t));
    if (!(*out)) {
        return NGX_ERROR;
    }

    (*out)->code_name = DNS_SOLVING_SCRIPT_NAME;
    (*out)->code = DNS_SOLVING_SCRIPT;
    (*out)->code_len = DNS_SOLVING_SCRIPT_LEN;
    (*out)->cache_key = DNS_SOLVING_SCRIPT_KEY;

    switch (sock->kind) {

#if (NGX_WASM_HTTP)
    case NGX_WASM_SOCKET_TCP_KIND_HTTP:
        (*out)->subsys = NGX_WASM_LUA_SUBSYS_HTTP;
        (*out)->ctx.request = sock->env.ctx.request;

        return NGX_OK;
#endif

#if (0 && NGX_WASM_STREAM)
    case NGX_WASM_SOCKET_TCP_KIND_STREAM:
        (*out)->subsys = NGX_WASM_LUA_SUBSYS_STREAM;
        (*out)->ctx.session = sock->env.ctx.session;

        return NGX_OK;
#endif

    default:
        ngx_wasm_assert(0);
        return NGX_ERROR;

    }
}


void
ngx_wasm_lua_resolver_init(ngx_conf_t *cf)
{
    DNS_SOLVING_SCRIPT_LEN = ngx_strlen((char *) DNS_SOLVING_SCRIPT);
    DNS_SOLVING_SCRIPT_KEY = ngx_wasm_lua_resolver_cache_key(
        cf->pool, DNS_SOLVING_SCRIPT_NAME, DNS_SOLVING_SCRIPT,
        DNS_SOLVING_SCRIPT_LEN);
}


ngx_int_t
ngx_wasm_lua_resolver_resolve(ngx_resolver_ctx_t *resolver_ctx)
{
    ngx_int_t                         rc;
    ngx_log_t                        *log;
    ngx_wasm_socket_tcp_t            *sock;
    ngx_wasm_lua_ctx_t               *lua_ctx;
    ngx_wasm_lua_ffi_resolver_ctx_t  *ffi_ctx;

    sock = (ngx_wasm_socket_tcp_t *) resolver_ctx->data;
    log = sock->log;

    rc = ngx_wasm_lua_resolver_create_lua_ctx(sock, &lua_ctx);
    if (rc != NGX_OK) {
        ngx_wasm_log_error(NGX_LOG_ERR, log, 0, "failed creating lua ctx");
        return NGX_ERROR;
    }

    rc = ngx_wasm_lua_init(lua_ctx);
    if (rc != NGX_OK) {
        ngx_wasm_log_error(NGX_LOG_ERR, log, 0, "failed initializing lua ctx");
        return NGX_ERROR;
    }

    ffi_ctx = ngx_wasm_lua_resolver_create_ctx(sock, resolver_ctx);
    if (ffi_ctx == NULL) {
        ngx_wasm_log_error(NGX_LOG_ERR, log, 0,
                           "failed creating lua resolver ctx");
        return NGX_ERROR;
    }

    lua_pushlightuserdata(lua_ctx->co, (void *) ffi_ctx);

    ngx_log_debug1(NGX_LOG_DEBUG_WASM, log, 0,
                   "lua resolver resuming thread (co: %p)", lua_ctx->co);

    rc = ngx_wasm_lua_resume_coroutine(lua_ctx);

    ngx_wasm_lua_resolver_set_resume_handler(sock);

    return rc;
}
