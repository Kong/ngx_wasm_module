#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_wasm_socket_tcp.h>
#if (NGX_WASM_LUA)
#include <ngx_wasm_lua_resolver.h>
#endif


#define ngx_wasm_socket_log(s)                                               \
    ((s) && (s)->log) ? (s)->log : ngx_cycle->log


static void ngx_wasm_socket_tcp_err(ngx_wasm_socket_tcp_t *sock,
    const char *fmt, ...);
static void ngx_wasm_socket_resolve_handler(ngx_resolver_ctx_t *ctx);
static ngx_int_t ngx_wasm_socket_tcp_connect_peer(ngx_wasm_socket_tcp_t *sock);
static ngx_int_t ngx_wasm_socket_tcp_get_peer(ngx_peer_connection_t *pc,
    void *data);
static void ngx_wasm_socket_tcp_finalize_read(ngx_wasm_socket_tcp_t *sock);
static void ngx_wasm_socket_tcp_finalize_write(ngx_wasm_socket_tcp_t *sock);
static void ngx_wasm_socket_tcp_handler(ngx_event_t *ev);
static void ngx_wasm_socket_tcp_nop_handler(ngx_wasm_socket_tcp_t *sock);
static void ngx_wasm_socket_tcp_connect_handler(ngx_wasm_socket_tcp_t *sock);
static void ngx_wasm_socket_tcp_send_handler(ngx_wasm_socket_tcp_t *sock);
static void ngx_wasm_socket_tcp_receive_handler(ngx_wasm_socket_tcp_t *sock);
static void ngx_wasm_socket_tcp_init_addr_text(ngx_peer_connection_t *pc);
#if (NGX_SSL)
static ngx_int_t ngx_wasm_socket_tcp_ssl_handshake(ngx_wasm_socket_tcp_t *sock);
static void ngx_wasm_socket_tcp_ssl_handshake_handler(ngx_connection_t *c);
static ngx_int_t ngx_wasm_socket_tcp_ssl_handshake_done(ngx_connection_t *c);
static ngx_int_t ngx_wasm_socket_tcp_ssl_set_server_name(ngx_connection_t *c,
    ngx_wasm_socket_tcp_t *sock);
#endif


static void
ngx_wasm_socket_tcp_err(ngx_wasm_socket_tcp_t *sock,
    const char *fmt, ...)
{
    va_list   args;
    u_char   *p, *last;

    if (sock->err == NULL) {
        sock->err = ngx_pnalloc(sock->pool, NGX_MAX_ERROR_STR);
        if (sock->err == NULL) {
            return;
        }

        p = sock->err;
        last = p + NGX_MAX_ERROR_STR;

        p = ngx_slprintf(p, last, "tcp socket - ");

        if (sock->socket_errno) {
            p = ngx_strerror(sock->socket_errno, p, last - p);
            sock->errlen = p - sock->err;
            return;
        }

        if (fmt)  {
            va_start(args, fmt);
            sock->errlen = ngx_vslprintf(p, last, fmt, args) - sock->err;
            va_end(args);
            return;
        }

        ngx_wasm_assert(0);
    }
}


static void
ngx_wasm_socket_tcp_resume(ngx_wasm_socket_tcp_t *sock)
{
#if (NGX_WASM_HTTP)
    ngx_int_t                 rc;
    ngx_http_wasm_req_ctx_t  *rctx;
#endif

    ngx_log_debug0(NGX_LOG_DEBUG_WASM, sock->log, 0,
                   "wasm tcp socket resuming");

    switch (sock->env.subsys->kind) {
#if (NGX_WASM_HTTP)
    case NGX_WASM_SUBSYS_HTTP:
        rctx = sock->env.ctx.rctx;
        rc = sock->resume_handler(sock);  /* handle sock event */

        dd("sock->resume rc: %ld", rc);

        switch (rc) {
        case NGX_AGAIN:
            ngx_wasm_yield(&rctx->env);
            break;
        case NGX_ERROR:
            ngx_http_wasm_error(rctx);
            break;
        default:
            ngx_wasm_assert(rc == NGX_OK);
            ngx_http_wasm_continue(rctx);
            break;
        }

        ngx_http_wasm_resume(rctx, 1, 1);  /* continue request */
        break;
#endif
    default:
        ngx_wasm_log_error(NGX_LOG_WASM_NYI, sock->log, 0,
                           "NYI - subsystem kind: %d",
                           sock->env.subsys->kind);
        ngx_wasm_assert(0);
        break;
    }
}


ngx_int_t
ngx_wasm_socket_tcp_init(ngx_wasm_socket_tcp_t *sock,
    ngx_str_t *host, unsigned tls, ngx_str_t *sni, ngx_wasm_subsys_env_t *env)
{
    u_char            *p, *last;
    static ngx_str_t   uds_prefix = ngx_string("unix:");

    ngx_memzero(sock, sizeof(ngx_wasm_socket_tcp_t));

    ngx_memcpy(&sock->env, env, sizeof(ngx_wasm_subsys_env_t));

    switch (sock->env.subsys->kind) {
#if (NGX_WASM_HTTP)
    case NGX_WASM_SUBSYS_HTTP:
        sock->free_bufs = env->ctx.rctx->free_bufs;
        sock->busy_bufs = env->ctx.rctx->busy_bufs;
        break;
#endif
#if (NGX_WASM_STREAM)
    case NGX_WASM_SUBSYS_STREAM:
        sock->free_bufs = env->ctx.sctx->free_bufs;
        sock->busy_bufs = env->ctx.sctx->busy_bufs;
        break;
#endif
    default:
        ngx_wasm_log_error(NGX_LOG_WASM_NYI, sock->log, 0,
                           "NYI - subsystem kind: %d",
                           sock->env.subsys->kind);
        ngx_wasm_assert(0);
        return NGX_ERROR;
    }

    sock->pool = env->connection->pool;  /* alias */
    sock->log = env->connection->log;    /* alias */

    sock->host.len = host->len;
    sock->host.data = ngx_pstrdup(sock->pool, host);
    if (sock->host.data == NULL) {
        return NGX_ERROR;
    }

    ngx_memzero(&sock->url, sizeof(ngx_url_t));

#if (NGX_SSL)
    sock->ssl_conf = tls ? env->ssl_conf : NULL;
    sock->url.default_port = tls ? 443 : 80;

    if (tls && sni && sni->len) {
        sock->sni = sni;
    }
#else
    sock->url.default_port = 80;
#endif
    sock->url.url = sock->host;
    sock->url.port = 0;

    if (host->len >= uds_prefix.len
        && ngx_memcmp(host->data, uds_prefix.data, uds_prefix.len) == 0)
    {

#if (!NGX_HAVE_UNIX_DOMAIN)
        ngx_wasm_log_error(NGX_LOG_ERR, sock->log, 0,
                           "host at \"%V\" requires unix domain socket support",
                           host);
        return NGX_ERROR;
#endif

    } else {
        /* extract port */

        p = host->data;
        last = p + host->len;

        if (*p == '[') {
            /* IPv6 */
            p = ngx_strlchr(p, last, ']');
            if (p == NULL) {
                p = host->data;
            }
        }

        p = ngx_strlchr(p, last, ':');
        if (p) {
            sock->url.port = ngx_atoi(p + 1, last - p);
        }
    }

    sock->url.no_resolve = 1;

    if (ngx_parse_url(sock->pool, &sock->url) != NGX_OK) {
        ngx_wasm_socket_tcp_err(sock, "%s", sock->url.err);
        return NGX_ERROR;
    }

#if 0
    dd("ngx_parse_url done (host: %*s, port: %d, uri: %*s, url: %*s)",
        (int) sock->url.host.len, sock->url.host.data,
        sock->url.port,
        (int) sock->url.uri.len, sock->url.uri.data,
        (int) sock->url.url.len, sock->url.url.data);
#endif

    return NGX_OK;
}


ngx_int_t
ngx_wasm_socket_tcp_connect(ngx_wasm_socket_tcp_t *sock)
{
    ngx_int_t                             rc;
    ngx_resolver_ctx_t                   *rslv_ctx = NULL, rslv_tmp;
    ngx_wasm_socket_tcp_dns_resolver_pt   resolver_pt = ngx_resolve_name;

#if (NGX_WASM_HTTP)
    ngx_msec_t                            conn_timeout;
    ngx_msec_t                            send_timeout, recv_timeout;
    ngx_resolver_t                       *resolver = NULL;
    ngx_wasm_core_conf_t                 *wcf;
    ngx_http_request_t                   *r;
    ngx_http_wasm_req_ctx_t              *rctx;
    ngx_http_core_loc_conf_t             *clcf;
    ngx_http_wasm_loc_conf_t             *loc = NULL;
#endif

#if (NGX_WASM_STREAM)
    ngx_stream_core_srv_conf_t           *ssrvcf;
    ngx_stream_session_t                 *s;
#endif

    dd("enter");

    if (sock->errlen) {
        return NGX_ERROR;
    }

    if (sock->connected) {
#if (NGX_SSL)
        if (sock->ssl_conf) {
            return ngx_wasm_socket_tcp_ssl_handshake(sock);
        }
#endif
        return NGX_OK;
    }

#if (NGX_WASM_HTTP)
    rctx = sock->env.ctx.rctx;
    r = rctx->r;

    if (sock->env.subsys->kind == NGX_WASM_SUBSYS_HTTP) {
        if (rctx->fake_request) {
            wcf = ngx_wasm_core_cycle_get_conf(ngx_cycle);

            sock->buffer_size = wcf->socket_buffer_size;
            sock->buffer_reuse = wcf->socket_buffer_reuse;

            conn_timeout = wcf->connect_timeout;
            send_timeout = wcf->send_timeout;
            recv_timeout = wcf->recv_timeout;

        } else {
            ngx_wasm_assert(r->loc_conf);
            loc = ngx_http_get_module_loc_conf(r, ngx_http_wasm_module);

            sock->buffer_size = loc->socket_buffer_size;
            sock->buffer_reuse = loc->socket_buffer_reuse;

            conn_timeout = loc->connect_timeout;
            send_timeout = loc->send_timeout;
            recv_timeout = loc->recv_timeout;
        }

        if (!sock->connect_timeout) {
            sock->connect_timeout = conn_timeout;
        }

        if (!sock->send_timeout) {
            sock->send_timeout = send_timeout;
        }

        if (!sock->read_timeout) {
            sock->read_timeout = recv_timeout;
        }
    }

    if (rctx->pwm_lua_resolver) {
#if (NGX_WASM_LUA)
        resolver_pt = ngx_wasm_lua_resolver_resolve;
#else
        ngx_wasm_log_error(NGX_LOG_WARN, sock->log, 0,
                           "\"proxy_wasm_lua_resolver\" is enabled but build "
                           "lacks lua support, using default resolver");
#endif
    }
#endif

    ngx_wasm_set_resume_handler(&sock->env);

    if (sock->url.addrs && sock->url.addrs[0].sockaddr) {
        sock->resolved.sockaddr = sock->url.addrs[0].sockaddr;
        sock->resolved.socklen = sock->url.addrs[0].socklen;
        sock->resolved.host = sock->url.addrs[0].name;
        sock->resolved.naddrs = 1;

        ngx_log_debug1(NGX_LOG_DEBUG_WASM, sock->log, 0,
                       "wasm tcp socket no resolving: %V",
                       &sock->resolved.host);

        rc = ngx_wasm_socket_tcp_connect_peer(sock);
#if (NGX_SSL)
        if (rc == NGX_OK) {
            ngx_wasm_assert(sock->connected);

            if (sock->ssl_conf) {
                return ngx_wasm_socket_tcp_ssl_handshake(sock);
            }
        }
#endif
        return rc;
    }

    ngx_log_debug1(NGX_LOG_DEBUG_WASM, sock->log, 0,
                   "wasm tcp socket resolving: %V",
                   &sock->host);

    sock->resolved.host = sock->host;
    sock->resolved.port = sock->url.port;

    /* resolve */

    ngx_memzero(&rslv_tmp, sizeof(ngx_resolver_ctx_t));

    rslv_tmp.name = sock->url.host;

    switch (sock->env.subsys->kind) {
#if (NGX_WASM_HTTP)
    case NGX_WASM_SUBSYS_HTTP:
        if (!rctx->fake_request) {
            clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);

            resolver = clcf->resolver;
            rslv_tmp.timeout = clcf->resolver_timeout;
        }

        if (resolver == NULL || !resolver->connections.nelts) {
            /* fallback to default resolver */
            wcf = ngx_wasm_core_cycle_get_conf(ngx_cycle);

            ngx_log_debug0(NGX_LOG_DEBUG_WASM, sock->log, 0,
                           "wasm tcp socket using default resolver");

            resolver = wcf->resolver;
            rslv_tmp.timeout = wcf->resolver_timeout;
        }

        rslv_ctx = ngx_resolve_start(resolver, &rslv_tmp);
        break;
#endif
#if (NGX_WASM_STREAM)
    case NGX_WASM_SUBSYS_STREAM:
        s = sock->env.ctx.sctx->s;
        ssrvcf = ngx_stream_get_module_srv_conf(s, ngx_stream_core_module);

        rslv_tmp.timeout = ssrvcf->resolver_timeout;
        rslv_ctx = ngx_resolve_start(ssrvcf->resolver, &rslv_tmp);
        break;
#endif
    default:
        ngx_wasm_assert(0);
        return NGX_ERROR;
    }

    if (rslv_ctx == NULL) {
        ngx_wasm_socket_tcp_err(sock, "failed starting resolver");
        return NGX_ERROR;
    }

    ngx_wasm_assert(rslv_ctx != NGX_NO_RESOLVER);

    rslv_ctx->name = rslv_tmp.name;
    rslv_ctx->timeout = rslv_tmp.timeout;
    rslv_ctx->handler = ngx_wasm_socket_resolve_handler;
    rslv_ctx->data = sock;

    ngx_log_debug0(NGX_LOG_DEBUG_WASM, sock->log, 0,
                   "wasm tcp socket resolving...");

    rc = resolver_pt(rslv_ctx);
    if (rc != NGX_OK && rc != NGX_AGAIN) {
        ngx_log_debug0(NGX_LOG_DEBUG_WASM, sock->log, 0,
                       "wasm tcp socket resolver failed before query");
        return NGX_ERROR;
    }

    return NGX_AGAIN;
}


static void
ngx_wasm_socket_resolve_handler(ngx_resolver_ctx_t *ctx)
{
    ngx_uint_t              i;
    u_char                 *p;
    socklen_t               socklen;
    struct sockaddr        *sockaddr;
    ngx_wasm_socket_tcp_t  *sock = ctx->data;

    ngx_log_debug0(NGX_LOG_DEBUG_WASM, sock->log, 0,
                   "wasm tcp socket resolve handler");

    if (ctx->state || !ctx->naddrs) {
#if (NGX_WASM_LUA)
        if (ctx->state == NGX_WASM_LUA_RESOLVE_ERR) {
            ngx_wasm_socket_tcp_err(sock, "lua resolver failed");
            goto error;
        }
#endif

        ngx_wasm_socket_tcp_err(sock, "resolver error: %s",
                                ngx_resolver_strerror(ctx->state));
        goto error;
    }

#if (NGX_DEBUG)
    {
        u_char      text[NGX_SOCKADDR_STRLEN];
        ngx_str_t   addr;
        in_port_t   port;
        ngx_uint_t  i;

        addr.data = text;

        for (i = 0; i < ctx->naddrs; i++) {
            port = ngx_inet_get_port(ctx->addrs[i].sockaddr);
            addr.len = ngx_sock_ntop(ctx->addrs[i].sockaddr,
                                     ctx->addrs[i].socklen, text,
                                     NGX_SOCKADDR_STRLEN, 0);

            if (port) {
                ngx_log_debug2(NGX_LOG_DEBUG_WASM, sock->log, 0,
                               "wasm tcp socket name was resolved to %V:%d",
                               &addr, port);

            } else {
                ngx_log_debug1(NGX_LOG_DEBUG_WASM, sock->log, 0,
                               "wasm tcp socket name was resolved to %V",
                               &addr);
            }
        }
    }
#endif

    i = ctx->naddrs == 1
        ? 0
        : ngx_random() % ctx->naddrs;

    socklen = ctx->addrs[i].socklen;
    sockaddr = ngx_palloc(sock->pool, socklen);
    if (sockaddr == NULL) {
        goto error;
    }

    ngx_memcpy(sockaddr, ctx->addrs[i].sockaddr, socklen);

    if (ngx_inet_get_port(sockaddr) == 0) {
        switch (sockaddr->sa_family) {
#if (NGX_HAVE_INET6)
        case AF_INET6:
            ((struct sockaddr_in6 *) sockaddr)->sin6_port =
                htons(sock->resolved.port);
            break;
#endif
        default: /* AF_INET */
            ((struct sockaddr_in *) sockaddr)->sin_port =
                htons(sock->resolved.port);
            break;
        }
    }

    p = ngx_pnalloc(sock->pool, NGX_SOCKADDR_STRLEN);
    if (p == NULL) {
        goto error;
    }

    sock->resolved.naddrs = 1;
    sock->resolved.sockaddr = sockaddr;
    sock->resolved.socklen = socklen;
    sock->resolved.host.len = ngx_sock_ntop(sockaddr, socklen, p,
                                            NGX_SOCKADDR_STRLEN, 1);
    sock->resolved.host.data = p;

    ngx_resolve_name_done(ctx);

    sock->resolved.ctx = NULL;

    /* connect */

    ngx_wasm_socket_tcp_connect_peer(sock);

    return;

error:

    ngx_resolve_name_done(ctx);

    ngx_wasm_socket_tcp_resume(sock);
}


static ngx_int_t
ngx_wasm_socket_tcp_connect_peer(ngx_wasm_socket_tcp_t *sock)
{
    ngx_int_t               rc;
    ngx_connection_t       *c;
    ngx_peer_connection_t  *pc;

    ngx_log_debug0(NGX_LOG_DEBUG_WASM, sock->log, 0,
                   "wasm tcp socket connecting...");

    ngx_wasm_assert(!sock->connected);
    ngx_wasm_assert(sock->resolved.sockaddr);

    pc = &sock->peer;
    pc->log = sock->log;
    pc->get = ngx_wasm_socket_tcp_get_peer;
    pc->sockaddr = sock->resolved.sockaddr;
    pc->socklen = sock->resolved.socklen;
    pc->name = &sock->resolved.host;

    rc = ngx_event_connect_peer(pc);
    if (rc == NGX_ERROR) {
        return NGX_ERROR;

    } else if (rc == NGX_BUSY) {
        ngx_wasm_socket_tcp_err(sock, "no live connection");
        return NGX_BUSY;

    } else if (rc == NGX_DECLINED) {
        sock->socket_errno = ngx_socket_errno;
        ngx_wasm_socket_tcp_err(sock, NULL);
        return NGX_ERROR;
    }

    ngx_wasm_assert(rc == NGX_OK || rc == NGX_AGAIN);

    sock->write_event_handler = ngx_wasm_socket_tcp_connect_handler;
    sock->read_event_handler = ngx_wasm_socket_tcp_connect_handler;

    c = pc->connection;

    if (c->pool == NULL) {
        c->pool = sock->pool;
#if 0
        /* disabled: inherited from sock->env.pool for reusing the socket */
        c->pool = ngx_create_pool(128, sock->log);
        if (c->pool == NULL) {
            return NGX_ERROR;
        }
#endif
    }

    c->log = sock->log;
    c->pool->log = c->log;
    c->read->handler = ngx_wasm_socket_tcp_handler;
    c->write->handler = ngx_wasm_socket_tcp_handler;
    c->read->log = c->log;
    c->write->log = c->log;
    c->data = sock;
    c->sendfile &= sock->env.connection->sendfile;

    if (rc == NGX_OK) {
        ngx_wasm_socket_tcp_connect_handler(sock);

    } else if (rc == NGX_AGAIN) {
        ngx_wasm_set_resume_handler(&sock->env);

        ngx_add_timer(c->write, sock->connect_timeout);
    }

    return rc;
}


#if (NGX_SSL)
static ngx_int_t
ngx_wasm_socket_tcp_ssl_handshake(ngx_wasm_socket_tcp_t *sock)
{
    ngx_int_t               rc;
    ngx_connection_t       *c;
    ngx_peer_connection_t  *pc;

    if (sock->errlen) {
        return NGX_ERROR;
    }

    if (sock->ssl_ready) {
        return NGX_OK;
    }

    pc = &sock->peer;
    c = pc->connection;

    if (ngx_ssl_create_connection(&sock->ssl_conf->ssl, c,
                                  NGX_SSL_BUFFER|NGX_SSL_CLIENT)
        != NGX_OK)
    {
        return NGX_ERROR;
    }

    dd("tls connection created");

    rc = ngx_wasm_socket_tcp_ssl_set_server_name(c, sock);
    if (rc == NGX_ERROR) {
        return NGX_ERROR;
    }

    rc = ngx_ssl_handshake(c);

    dd("ssl handshake rc: %ld", rc);

    if (rc == NGX_OK) {
        rc = ngx_wasm_socket_tcp_ssl_handshake_done(c);
        ngx_wasm_assert(rc != NGX_AGAIN);

    } else if (rc == NGX_AGAIN) {
        ngx_add_timer(c->write, sock->connect_timeout);
        c->ssl->handler = ngx_wasm_socket_tcp_ssl_handshake_handler;
    }

    return rc;
}


static void
ngx_wasm_socket_tcp_ssl_handshake_handler(ngx_connection_t *c)
{
    ngx_wasm_socket_tcp_t  *sock;
    ngx_int_t               rc;

    sock = c->data;

    rc = ngx_wasm_socket_tcp_ssl_handshake_done(c);
    if (rc != NGX_AGAIN) {
        goto resume;
    }

    if (c->write->timedout) {
        ngx_wasm_socket_tcp_err(sock, "tls handshake timed out");
        goto resume;
    }

    ngx_wasm_socket_tcp_err(sock, "tls handshake failed");

resume:

    ngx_wasm_socket_tcp_resume(sock);
}


static ngx_int_t
ngx_wasm_socket_tcp_ssl_handshake_done(ngx_connection_t *c)
{
    ngx_int_t               rc;
    ngx_wasm_socket_tcp_t  *sock = c->data;

    if (!c->ssl->handshaked) {
        return NGX_AGAIN;
    }

    if (sock->ssl_conf->verify_cert) {
        ngx_log_debug2(NGX_LOG_DEBUG_HTTP, c->log, 0,
                       "wasm tcp socket verifying tls certificate for \"%V\" "
                       "(sni: \"%V\")",
                       &sock->host, &sock->ssl_server_name);

        rc = SSL_get_verify_result(c->ssl->connection);
        if (rc != X509_V_OK) {
            ngx_wasm_socket_tcp_err(sock,
                                    "tls certificate verify error: (%l:%s)",
                                    rc, X509_verify_cert_error_string(rc));
            return NGX_ERROR;
        }

    } else if (sock->ssl_conf->no_verify_warn) {
        ngx_wasm_log_error(NGX_LOG_WARN, sock->log, 0,
                           "tls certificate not verified");
    }

    if (sock->ssl_conf->verify_host) {
        ngx_log_debug2(NGX_LOG_DEBUG_HTTP, c->log, 0,
                       "wasm tcp socket checking tls certificate "
                       "CN for \"%V\" (sni: \"%V\")",
                       &sock->host, &sock->ssl_server_name);

        if (ngx_ssl_check_host(c, &sock->ssl_server_name) != NGX_OK) {
            ngx_wasm_socket_tcp_err(sock,
                                    "tls certificate CN "
                                    "does not match \"%V\" sni",
                                    &sock->ssl_server_name);
            return NGX_ERROR;
        }

    } else if (sock->ssl_conf->no_verify_warn) {
        ngx_wasm_log_error(NGX_LOG_WARN, sock->log, 0,
                           "tls certificate host not verified");
    }

    dd("tls handshake completed");

    c->read->handler = ngx_wasm_socket_tcp_handler;
    c->write->handler = ngx_wasm_socket_tcp_handler;

    sock->ssl_ready = 1;

    return NGX_OK;
}


/* Modified from `ngx_http_upstream_ssl_name` */
static ngx_int_t
ngx_wasm_socket_tcp_ssl_set_server_name(ngx_connection_t *c,
    ngx_wasm_socket_tcp_t *sock)
{
    size_t      len;
    ngx_int_t   rc;
    ngx_str_t  *name;
    u_char     *p, *last, *sni = NULL;

    if (sock->sni) {
        name = sock->sni;

    } else  {
        name = &sock->host;
    }

    /**
     * ssl name here may contain port, notably if derived from $proxy_host
     * or $http_host; we have to strip it
     */

    p = name->data;
    len = name->len;
    last = name->data + name->len;

    if (p) {
        if (*p == '[') {
            p = ngx_strlchr(p, last, ']');
            if (p == NULL) {
                p = name->data;
            }
        }

        p = ngx_strlchr(p, last, ':');
        if (p) {
            len = p - name->data;
        }
    }

    /* as per RFC 6066, literal IPv4 and IPv6 addresses are not permitted */

    if (len == 0
        || (name->data && *name->data == '[')
        || ngx_inet_addr(name->data, len) != INADDR_NONE)
    {
        ngx_wasm_socket_tcp_err(sock,
                                "could not derive tls sni from host (\"%V\")",
                                &sock->host);
        goto error;
    }

    /**
     * SSL_set_tlsext_host_name() needs a null-terminated string,
     * hence we explicitly null-terminate name here
     */

    sni = ngx_pnalloc(sock->pool, len + 1);
    if (sni == NULL) {
        goto error;
    }

    (void) ngx_cpystrn(sni, name->data, len + 1);

    sock->ssl_server_name.len = len;
    sock->ssl_server_name.data = sni;

    if (SSL_set_tlsext_host_name(c->ssl->connection, (char *) sni)
        == 0)
    {
        ngx_ssl_error(NGX_LOG_ERR, c->log, 0,
                      "SSL_set_tlsext_host_name(\"%s\") failed", sni);
        goto error;
    }

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, c->log, 0,
                   "wasm tcp socket upstream tls server name: \"%s\"", sni);

    rc = NGX_OK;

    goto done;

error:

    if (sni) {
        ngx_pfree(sock->pool, sni);
    }

    sock->ssl_server_name.len = 0;
    sock->ssl_server_name.data = NULL;

    rc = NGX_ERROR;

done:

    return rc;
}
#endif


static ngx_int_t
ngx_wasm_socket_tcp_get_peer(ngx_peer_connection_t *pc, void *data)
{
    return NGX_OK;
}


ngx_int_t
ngx_wasm_socket_tcp_send(ngx_wasm_socket_tcp_t *sock, ngx_chain_t *cl)
{
    ngx_int_t          n;
    ngx_buf_t         *b;
    ngx_connection_t  *c;

    if (!sock->connected) {
        ngx_wasm_socket_tcp_err(sock, "not connected");
        return NGX_ERROR;
    }

    c = sock->peer.connection;

    for ( ;; ) {

        b = cl->buf;
        n = c->send(c, b->pos, ngx_buf_size(b));

        dd("send rc: %d (wev ready: %d)", (int) n, (int) c->write->ready);

        if (n >= 0) {
            b->pos += n;

            if (b->pos == b->last) {
                cl = cl->next;

                if (cl == NULL) {
                    ngx_log_debug0(NGX_LOG_DEBUG_WASM, sock->log, 0,
                                   "wasm tcp socket sent all the data");

                    if (c->write->timer_set) {
                        ngx_del_timer(c->write);
                    }

                    ngx_chain_update_chains(sock->pool,
                                            &sock->free_bufs, &sock->busy_bufs,
                                            &cl, sock->env.buf_tag);

                    sock->write_event_handler = ngx_wasm_socket_tcp_nop_handler;

                    if (ngx_handle_write_event(c->write, 0) != NGX_OK) {
                        return NGX_ERROR;
                    }

                    return NGX_OK;
                }
            }

            ngx_log_debug0(NGX_LOG_DEBUG_WASM, sock->log, 0,
                           "wasm tcp socket more data to send");
            continue;
        }

        ngx_wasm_assert(n == NGX_ERROR || n == NGX_AGAIN);
        break;
    }

    if (n == NGX_ERROR) {
        c->error = 1;
        sock->socket_errno = ngx_socket_errno;
        ngx_wasm_socket_tcp_err(sock, NULL);
        return NGX_ERROR;
    }

    ngx_wasm_assert(n == NGX_AGAIN);

    sock->write_event_handler = ngx_wasm_socket_tcp_send_handler;

    ngx_add_timer(c->write, sock->send_timeout);

    if (ngx_handle_write_event(c->write, 0) != NGX_OK) {
        return NGX_ERROR;
    }

    ngx_wasm_set_resume_handler(&sock->env);

    return NGX_AGAIN;
}


#if 0
ngx_int_t
ngx_wasm_socket_reader_read_all(ngx_wasm_socket_tcp_t *sock, ssize_t bytes)
{
    ngx_log_debug0(NGX_LOG_DEBUG_WASM, sock->log, 0,
                   "wasm tcp socket reading all");

    return ngx_wasm_read_all(&sock->buffer, sock->buf_in, bytes);
}


ngx_int_t
ngx_wasm_socket_reader_read_line(ngx_wasm_socket_tcp_t *sock, ssize_t bytes)
{
    ngx_log_debug0(NGX_LOG_DEBUG_WASM, sock->log, 0,
                   "wasm tcp socket reading line");

    return ngx_wasm_read_line(&sock->buffer, sock->buf_in, bytes);
}
#endif


#if (NGX_WASM_HTTP)
ngx_int_t
ngx_wasm_socket_read_http_response(ngx_wasm_socket_tcp_t *sock,
    ssize_t bytes, void *ctx)
{
    ngx_wasm_http_reader_ctx_t  *in_ctx = ctx;
    ngx_http_request_t          *r;
    ngx_http_wasm_req_ctx_t     *rctx;

    r = &in_ctx->fake_r;
    rctx = in_ctx->rctx;

    if (!r->signature) {
        ngx_memzero(r, sizeof(ngx_http_request_t));

        r->pool = in_ctx->pool;
        r->signature = NGX_HTTP_MODULE;
        r->connection = rctx->connection;
        r->request_start = NULL;
        r->header_in = NULL;

        if (ngx_list_init(&r->headers_out.headers, r->pool, 20,
                          sizeof(ngx_table_elt_t))
            != NGX_OK)
        {
            return NGX_ERROR;
        }

        if (ngx_list_init(&r->headers_out.trailers, r->pool, 4,
                          sizeof(ngx_table_elt_t))
            != NGX_OK)
        {
            return NGX_ERROR;
        }

        if (ngx_http_upstream_create(r) != NGX_OK) {
            return NGX_ERROR;
        }

        r->upstream->conf = &in_ctx->uconf;

        if (ngx_list_init(&r->upstream->headers_in.headers, r->pool, 4,
                          sizeof(ngx_table_elt_t))
            != NGX_OK)
        {
            return NGX_ERROR;
        }

        r->headers_out.content_length_n = -1;
        r->headers_out.last_modified_time = -1;
    }

    if (bytes) {
        ngx_log_debug1(NGX_LOG_DEBUG_WASM, sock->log, 0,
                       "wasm tcp socket resuming http response reading "
                       "with %d bytes to parse", bytes);

        return ngx_wasm_read_http_response(&sock->buffer, sock->buf_in, bytes,
                                           (ngx_wasm_http_reader_ctx_t *) ctx);
    }

    ngx_log_debug0(NGX_LOG_DEBUG_WASM, sock->log, 0,
                   "wasm tcp socket no bytes to parse");

    return NGX_OK;
}
#endif


ngx_int_t
ngx_wasm_socket_tcp_read(ngx_wasm_socket_tcp_t *sock,
    ngx_wasm_socket_tcp_reader_pt reader, void *reader_ctx)
{
    off_t              size, avail;
    ssize_t            n;
    ngx_int_t          rc;
    ngx_buf_t         *b;
    ngx_chain_t       *cl;
    ngx_event_t       *rev;
    ngx_connection_t  *c;

    if (!sock->connected) {
        ngx_wasm_socket_tcp_err(sock, "not connected");
        return NGX_ERROR;
    }

    if (sock->bufs_in == NULL) {
        cl = ngx_wasm_chain_get_free_buf(sock->pool, &sock->free_bufs,
                                         sock->buffer_size, sock->env.buf_tag,
                                         sock->buffer_reuse);
        if (cl == NULL) {
            return NGX_ERROR;
        }

        sock->bufs_in = cl;
        sock->buf_in = sock->bufs_in;
        sock->buffer = *sock->buf_in->buf;
    }

    sock->read_event_handler = ngx_wasm_socket_tcp_receive_handler;

    b = &sock->buffer;
    c = sock->peer.connection;
    rev = c->read;

    for ( ;; ) {

        size = ngx_buf_size(b);
#if (DDEBUG)
        avail = b->end - b->last;
#endif

        dd("pre-reader buf: \"%.*s\" (size: %lu, avail: %lu, eof: %d)",
           (int) size, b->pos, size, avail, (int) sock->eof);

        if (size || sock->eof) {

            rc = reader(sock, size, reader_ctx);
            if (rc == NGX_ERROR) {
                ngx_wasm_socket_tcp_err(sock, "parser error");
                return NGX_ERROR;
            }

            if (rc == NGX_OK) {
                ngx_log_debug0(NGX_LOG_DEBUG_WASM, sock->log, 0,
                               "wasm tcp socket reading done");

                sock->read_event_handler = ngx_wasm_socket_tcp_nop_handler;

                if (ngx_handle_read_event(rev, 0) != NGX_OK) {
                    return NGX_ERROR;
                }

                return NGX_OK;
            }

            ngx_wasm_assert(rc == NGX_AGAIN);

            if (b->pos < b->last) {
                dd("more to read, continue");
                continue;
            }
        }

        if (rev->active && !rev->ready) {
            dd("rev not ready");
            rc = NGX_AGAIN;
            break;
        }

#if (DDEBUG)
        size = ngx_buf_size(b);
#endif
        avail = b->end - b->last;

        dd("post-reader buf: \"%.*s\" (size: %lu, avail: %lu, eof: %d)",
           (int) size, b->pos, size, avail, (int) sock->eof);

        if (avail == 0) {
            cl = ngx_wasm_chain_get_free_buf(sock->pool,
                                             &sock->free_bufs,
                                             sock->buffer_size,
                                             sock->env.buf_tag,
                                             sock->buffer_reuse);
            if (cl == NULL) {
                return NGX_ERROR;
            }

            sock->buf_in->next = cl;
            sock->buf_in = cl;
            sock->buffer = *sock->buf_in->buf;

            b = &sock->buffer;

            avail = b->end - b->last;
        }

        ngx_log_debug1(NGX_LOG_DEBUG_WASM, sock->log, 0,
                       "wasm tcp socket trying to receive data (max: %O)",
                       avail);

        n = c->recv(c, b->last, avail);

        dd("recv rc: %d (rev ready: %d)", (int) n, (int) c->read->ready);

        if (n == NGX_ERROR) {
            sock->socket_errno = ngx_socket_errno;
            ngx_wasm_socket_tcp_err(sock, NULL);
            return NGX_ERROR;
        }

        if (n == NGX_AGAIN) {
            rc = NGX_AGAIN;
            break;
        }

        if (n == 0) {
            ngx_log_debug0(NGX_LOG_DEBUG_WASM, sock->log, 0,
                           "wasm tcp socket eof");
            sock->eof = 1;
            continue;
        }

        b->last += n;
    }

    ngx_wasm_assert(rc == NGX_OK || rc == NGX_AGAIN);

    if (ngx_handle_read_event(rev, 0) != NGX_OK) {
        return NGX_ERROR;
    }

    if (rc == NGX_OK) {
        if (rev->timer_set) {
            ngx_del_timer(rev);
        }

    } else if (rc == NGX_AGAIN) {
        ngx_wasm_set_resume_handler(&sock->env);

        if (rev->active) {
            ngx_add_timer(rev, sock->read_timeout);
        }
    }

    return rc;
}


void
ngx_wasm_socket_tcp_close(ngx_wasm_socket_tcp_t *sock)
{
    ngx_connection_t  *c;

    if (sock->closed) {
        return;
    }

    c = sock->peer.connection;

    ngx_log_debug0(NGX_LOG_DEBUG_WASM, ngx_wasm_socket_log(sock), 0,
                   "wasm tcp socket closing");

    ngx_wasm_socket_tcp_finalize_read(sock);
    ngx_wasm_socket_tcp_finalize_write(sock);

    if (c) {
#if (NGX_SSL)
        if (c->ssl) {
            c->ssl->no_wait_shutdown = 1;
            (void) ngx_ssl_shutdown(c);
        }
#endif

        ngx_close_connection(c);
    }

    sock->connected = 0;
    sock->closed = 1;
}


void
ngx_wasm_socket_tcp_destroy(ngx_wasm_socket_tcp_t *sock)
{
    ngx_chain_t       *cl, *ln;
    ngx_connection_t  *c = sock->peer.connection;

    dd("enter");

    ngx_wasm_socket_tcp_close(sock);

    if (sock->host.data) {
        ngx_pfree(sock->pool, sock->host.data);
        sock->host.data = NULL;
    }

#if (NGX_SSL)
    if (sock->ssl_server_name.data) {
        ngx_pfree(sock->pool, sock->ssl_server_name.data);
        sock->ssl_server_name.data = NULL;
    }
#endif

    if (c && c->pool) {
#if 0
        /* disabled: c->pool is inherited from sock->env.pool */
        ngx_destroy_pool(c->pool);
#endif
        c->pool = NULL;
    }

#if 0
    if (sock->free_large_bufs) {
        ngx_log_debug1(NGX_LOG_DEBUG_WASM, ngx_wasm_socket_log(sock), 0,
                       "wasm tcp socket free: %p", sock->free_large_bufs);

        for (cl = sock->free_large_bufs; cl; /* void */) {
            ln = cl;
            cl = cl->next;

            ngx_pfree(sock->pool, ln->buf->start);
            ngx_free_chain(sock->pool, ln);
        }

        sock->free_large_bufs = NULL;
    }
#endif

    if (sock->busy_large_bufs) {
        ngx_log_debug2(NGX_LOG_DEBUG_WASM, ngx_wasm_socket_log(sock), 0,
                       "wasm tcp socket busy: %p %i",
                       sock->busy_large_bufs, sock->lbusy);

        for (cl = sock->busy_large_bufs; cl; /* void */) {
            ln = cl;
            cl = cl->next;

            ngx_pfree(sock->pool, ln->buf->start);
            ngx_free_chain(sock->pool, ln);
        }

        sock->busy_large_bufs = NULL;
        sock->lbusy = 0;
    }
}


static void
ngx_wasm_socket_tcp_finalize_read(ngx_wasm_socket_tcp_t *sock)
{
    ngx_connection_t  *c;
    ngx_chain_t       *cl;

    if (sock->read_closed) {
        return;
    }

    sock->read_closed = 1;

    if (sock->bufs_in) {
        for (cl = sock->bufs_in; cl; cl = cl->next) {
            dd("bufs_in chain: %p, next %p", cl, cl->next);
            cl->buf->pos = cl->buf->last;
        }

        sock->free_bufs = sock->bufs_in;

        sock->bufs_in = NULL;
        sock->buf_in = NULL;

        ngx_memzero(&sock->buffer, sizeof(ngx_buf_t));
    }

    c = sock->peer.connection;

    if (c) {
        if (c->read->timer_set) {
            ngx_del_timer(c->read);
        }

        if (c->read->active || c->read->disabled) {
            ngx_del_event(c->read, NGX_READ_EVENT, NGX_CLOSE_EVENT);
        }

        if (c->read->posted) {
            ngx_delete_posted_event(c->read);
        }

        c->read->closed = 1;
    }
}


static void
ngx_wasm_socket_tcp_finalize_write(ngx_wasm_socket_tcp_t *sock)
{
    ngx_connection_t  *c;

    if (sock->write_closed) {
        return;
    }

    sock->write_closed = 1;

    c = sock->peer.connection;

    if (c) {
        if (c->write->timer_set) {
            ngx_del_timer(c->write);
        }

        if (c->write->active || c->write->disabled) {
            ngx_del_event(c->write, NGX_WRITE_EVENT, NGX_CLOSE_EVENT);
        }

        if (c->write->posted) {
            ngx_delete_posted_event(c->write);
        }

        c->write->closed = 1;
    }
}


/* handlers */


static void
ngx_wasm_socket_tcp_handler(ngx_event_t *ev)
{
    ngx_connection_t       *c = ev->data;
    ngx_wasm_socket_tcp_t  *sock = c->data;

    ngx_log_debug1(NGX_LOG_DEBUG_WASM, ev->log, 0,
                   "wasm tcp socket handler (wev: %d)",
                   (int) ev->write);

    if (sock->closed) {
        ngx_log_debug0(NGX_LOG_DEBUG_WASM, ev->log, 0,
                       "wasm tcp socket was closed");
        return;
    }

    if (ev->write) {
        sock->write_event_handler(sock);

    } else {
        sock->read_event_handler(sock);
    }

    ngx_wasm_socket_tcp_resume(sock);

    dd("exit");
}


static void
ngx_wasm_socket_tcp_nop_handler(ngx_wasm_socket_tcp_t *sock)
{
#if (DDEBUG)
    ngx_log_debug0(NGX_LOG_DEBUG_WASM, sock->log, 0,
                   "wasm tcp socket nop handler");
#endif
}


static ngx_int_t
ngx_wasm_socket_tcp_test_connect(ngx_connection_t *c)
{
    int           err;
    socklen_t     len;
#if (NGX_HAVE_KQUEUE)
    ngx_event_t  *ev;

    if (ngx_event_flags & NGX_USE_KQUEUE_EVENT)  {
        dd("pending eof: (%p)%d (%p)%d", c->write, c->write->pending_eof,
           c->read, c->read->pending_eof);

        if (c->write->pending_eof) {
            ev = c->write;

        } else if (c->read->pending_eof) {
            ev = c->read;

        } else {
            ev = NULL;
        }

        if (ev) {
#if 0
            (void) ngx_connection_error(c, ev->kq_errno,
                                        "kevent() reported that "
                                        "connect() failed");
#endif
            return ev->kq_errno;
        }

    } else
#endif
    {
        err = 0;
        len = sizeof(int);

        /*
         * BSDs and Linux return 0 and set a pending error in err
         * Solaris returns -1 and sets errno
         */

        if (getsockopt(c->fd, SOL_SOCKET, SO_ERROR, (void *) &err, &len)
            == -1)
        {
            err = ngx_errno;
        }

        if (err) {
#if 0
            (void) ngx_connection_error(c, err, "connect() failed");
#endif
            return err;
        }
    }

    return NGX_OK;
}


static void
ngx_wasm_socket_tcp_connect_handler(ngx_wasm_socket_tcp_t *sock)
{
    ngx_int_t               rc;
    ngx_connection_t       *c = sock->peer.connection;
    ngx_peer_connection_t  *pc = &sock->peer;

    ngx_wasm_socket_tcp_init_addr_text(pc);

    if (c->write->timedout) {
        ngx_log_debug2(NGX_LOG_DEBUG_WASM, sock->log, 0,
                       "wasm tcp socket timed out connecting to \"%V:%ud\"",
                       &c->addr_text, ngx_inet_get_port(sock->peer.sockaddr));

        ngx_wasm_socket_tcp_err(sock, "timed out connecting to \"%V:%ud\"",
                                &c->addr_text,
                                ngx_inet_get_port(sock->peer.sockaddr));

        sock->timedout = 1;
        return;
    }

    sock->read_event_handler = ngx_wasm_socket_tcp_nop_handler;
    sock->write_event_handler = ngx_wasm_socket_tcp_nop_handler;

    rc = ngx_wasm_socket_tcp_test_connect(c);
    if (rc != NGX_OK) {
        if (rc > 0) {
            sock->socket_errno = (ngx_err_t) rc;
            ngx_wasm_socket_tcp_err(sock, NULL);
        }

        return;
    }

    ngx_log_debug2(NGX_LOG_DEBUG_WASM, sock->log, 0,
                   "wasm tcp socket connected to \"%V:%ud\"",
                   &c->addr_text, ngx_inet_get_port(sock->peer.sockaddr));

    if (c->write->timer_set) {
        ngx_del_timer(c->write);
    }

    if (ngx_handle_write_event(c->write, 0) != NGX_OK) {
        return;
    }

    if (ngx_handle_read_event(c->read, 0) != NGX_OK) {
        return;
    }

    sock->connected = 1;
}


static void
ngx_wasm_socket_tcp_send_handler(ngx_wasm_socket_tcp_t *sock)
{
    ngx_connection_t  *c = sock->peer.connection;

    ngx_log_debug2(NGX_LOG_DEBUG_WASM, sock->log, 0,
                   "wasm tcp socket send handler for \"%V:%ud\"",
                   &c->addr_text, ngx_inet_get_port(sock->peer.sockaddr));

    if (c->write->timedout) {
        ngx_log_debug2(NGX_LOG_DEBUG_WASM, sock->log, 0,
                       "wasm tcp socket timed out writing to \"%V:%ud\"",
                       &c->addr_text, ngx_inet_get_port(sock->peer.sockaddr));

        ngx_wasm_socket_tcp_err(sock, "timed out writing to \"%V:%ud\"",
                                &c->addr_text,
                                ngx_inet_get_port(sock->peer.sockaddr));

        sock->timedout = 1;
        return;
    }
}


static void
ngx_wasm_socket_tcp_receive_handler(ngx_wasm_socket_tcp_t *sock)
{
    ngx_connection_t  *c = sock->peer.connection;

    ngx_log_debug2(NGX_LOG_DEBUG_WASM, sock->log, 0,
                   "wasm tcp socket receive handler for \"%V:%ud\"",
                   &c->addr_text, ngx_inet_get_port(sock->peer.sockaddr));

    if (c->read->timedout) {
        ngx_log_debug2(NGX_LOG_DEBUG_WASM, sock->log, 0,
                       "wasm tcp socket timed out reading from \"%V:%ud\"",
                       &c->addr_text, ngx_inet_get_port(sock->peer.sockaddr));

        ngx_wasm_socket_tcp_err(sock, "timed out reading from \"%V:%ud\"",
                                &c->addr_text,
                                ngx_inet_get_port(sock->peer.sockaddr));

        sock->timedout = 1;
        return;
    }

    if (c->read->timer_set) {
        ngx_del_timer(c->read);
    }
}


static void
ngx_wasm_socket_tcp_init_addr_text(ngx_peer_connection_t *pc)
{
    ngx_connection_t  *c = pc->connection;
    size_t             addr_text_max_len;

    switch (pc->sockaddr->sa_family) {

#if (NGX_HAVE_INET6)
    case AF_INET6:
        addr_text_max_len = NGX_INET6_ADDRSTRLEN;
        break;
#endif

#if (NGX_HAVE_UNIX_DOMAIN)
    case AF_UNIX:
        addr_text_max_len = NGX_UNIX_ADDRSTRLEN;
        break;
#endif

    case AF_INET:
        addr_text_max_len = NGX_INET_ADDRSTRLEN;
        break;

    default:
        addr_text_max_len = NGX_SOCKADDR_STRLEN;
        break;

    }

    c->addr_text.data = ngx_pnalloc(c->pool, addr_text_max_len);
    if (c->addr_text.data == NULL) {
        return;
    }

    c->addr_text.len = ngx_sock_ntop(pc->sockaddr, pc->socklen,
                                     c->addr_text.data,
                                     addr_text_max_len, 0);
}
