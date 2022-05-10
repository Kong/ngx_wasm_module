#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_wasm_socket_tcp.h>


#define ngx_wasm_socket_log(s)                                               \
    ((s) && (s)->log) ? (s)->log : ngx_cycle->log


static ngx_inline void ngx_wasm_socket_tcp_set_resume_handler(
    ngx_wasm_socket_tcp_t *sock);
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


static ngx_inline void
ngx_wasm_socket_tcp_set_resume_handler(ngx_wasm_socket_tcp_t *sock)
{
#if (NGX_WASM_HTTP)
    ngx_http_request_t       *r;
    ngx_http_wasm_req_ctx_t  *rctx;

    if (sock->kind == NGX_WASM_SOCKET_TCP_KIND_HTTP) {
        rctx = sock->env.ctx.request;
        r = rctx->r;

        if (rctx->entered_content_phase) {
            r->write_event_handler = ngx_http_wasm_content_wev_handler;

        } else {
            r->write_event_handler = ngx_http_core_run_phases;
        }
    }
#endif
}


static void
ngx_wasm_socket_tcp_err(ngx_wasm_socket_tcp_t *sock,
    const char *fmt, ...)
{
    va_list   args;
    u_char   *p;

    if (sock->err == NULL) {
        sock->err = ngx_pnalloc(sock->pool, NGX_MAX_ERROR_STR);
        if (sock->err == NULL) {
            return;
        }

        if (sock->socket_errno) {
            p = ngx_strerror(sock->socket_errno, sock->err, NGX_MAX_ERROR_STR);
            sock->errlen = p - sock->err;
            return;
        }

        if (fmt)  {
            va_start(args, fmt);
            sock->errlen = ngx_vsnprintf(sock->err, NGX_MAX_ERROR_STR,
                                         fmt, args) - sock->err;
            va_end(args);
        }
    }
}


ngx_int_t
ngx_wasm_socket_tcp_init(ngx_wasm_socket_tcp_t *sock,
    ngx_str_t *host, ngx_wasm_socket_tcp_env_t *env)
{
    ngx_memzero(sock, sizeof(ngx_wasm_socket_tcp_t));

    sock->env.connection = env->connection;
    sock->env.buf_tag = env->buf_tag;
    sock->kind = env->kind;

    switch (sock->kind) {
#if (NGX_WASM_HTTP)
    case NGX_WASM_SOCKET_TCP_KIND_HTTP:
        sock->env.ctx.request = env->ctx.request;
        sock->free_bufs = env->ctx.request->free_bufs;
        sock->busy_bufs = env->ctx.request->busy_bufs;
        break;
#endif
#if (NGX_WASM_STREAM)
    case NGX_WASM_SOCKET_TCP_KIND_STREAM:
        sock->env.ctx.session = env->ctx.session;
        sock->free_bufs = env->ctx.session->free_bufs;
        sock->busy_bufs = env->ctx.session->busy_bufs;
        break;
#endif
    default:
        ngx_wasm_log_error(NGX_LOG_WASM_NYI, sock->log, 0,
                           "NYI - socket kind: %d", sock->kind);
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

    sock->url.url = sock->host;
    sock->url.default_port = 80;
    sock->url.no_resolve = 1;

    if (ngx_parse_url(sock->pool, &sock->url) != NGX_OK) {
        ngx_wasm_socket_tcp_err(sock, "%s", sock->url.err);
        return NGX_ERROR;
    }

#if 0
    dd("ngx_parse_url done (host: %*s, port: %*s, uri: %*s, url: %*s)",
        (int) sock->url.host.len, sock->url.host.data,
        (int) sock->url.port_text.len, sock->url.port_text.data,
        (int) sock->url.uri.len, sock->url.uri.data,
        (int) sock->url.url.len, sock->url.url.data);
#endif

    return NGX_OK;
}


ngx_int_t
ngx_wasm_socket_tcp_connect(ngx_wasm_socket_tcp_t *sock)
{
    ngx_resolver_ctx_t          *rslv_ctx = NULL, rslv_tmp;
#if (NGX_WASM_HTTP)
    ngx_http_core_loc_conf_t    *clcf;
    ngx_http_wasm_loc_conf_t    *loc;
    ngx_http_request_t          *r;
#endif
#if (NGX_WASM_STREAM)
    ngx_stream_core_srv_conf_t  *ssrvcf;
    ngx_stream_session_t        *s;
#endif

    if (sock->errlen) {
        return NGX_ERROR;
    }

    if (sock->connected) {
        return NGX_OK;
    }

#if (NGX_WASM_HTTP)
    if (sock->kind == NGX_WASM_SOCKET_TCP_KIND_HTTP) {
        r = sock->env.ctx.request->r;
        loc = ngx_http_get_module_loc_conf(r, ngx_http_wasm_module);

        sock->buffer_size = loc->socket_buffer_size;
        sock->buffer_reuse = loc->socket_buffer_reuse;

        if (!sock->connect_timeout) {
            sock->connect_timeout = loc->connect_timeout;
        }

        if (!sock->send_timeout) {
            sock->send_timeout = loc->send_timeout;
        }

        if (!sock->read_timeout) {
            sock->read_timeout = loc->recv_timeout;
        }
    }
#endif

    ngx_wasm_socket_tcp_set_resume_handler(sock);

    if (sock->url.addrs && sock->url.addrs[0].sockaddr) {
        sock->resolved.sockaddr = sock->url.addrs[0].sockaddr;
        sock->resolved.socklen = sock->url.addrs[0].socklen;
        sock->resolved.host = sock->url.addrs[0].name;
        sock->resolved.naddrs = 1;

        return ngx_wasm_socket_tcp_connect_peer(sock);
    }

    sock->resolved.host = sock->host;
    sock->resolved.port = sock->url.default_port;

    /* resolve */

    ngx_memzero(&rslv_tmp, sizeof(ngx_resolver_ctx_t));

    rslv_tmp.name = sock->url.host;

    switch (sock->kind) {
#if (NGX_WASM_HTTP)
    case NGX_WASM_SOCKET_TCP_KIND_HTTP:
        r = sock->env.ctx.request->r;
        clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);

        rslv_tmp.timeout = clcf->resolver_timeout;
        rslv_ctx = ngx_resolve_start(clcf->resolver, &rslv_tmp);
        break;
#endif
#if (NGX_WASM_STREAM)
    case NGX_WASM_SOCKET_TCP_KIND_STREAM:
        s = sock->env.ctx.session->s;
        ssrvcf = ngx_stream_get_module_srv_conf(s, ngx_stream_core_module);

        rslv_tmp.timeout = ssrvcf->resolver_timeout;
        rslv_ctx = ngx_resolve_start(ssrvcf->resolver, &rslv_tmp);
        break;
#endif
    default:
        ngx_wasm_log_error(NGX_LOG_WASM_NYI, sock->log, 0,
                           "NYI - socket kind: %d", sock->kind);
        return NGX_ERROR;
    }

    if (rslv_ctx == NULL) {
        ngx_wasm_socket_tcp_err(sock, "failed starting resolver");
        return NGX_ERROR;

    } else if (rslv_ctx == NGX_NO_RESOLVER) {
        ngx_wasm_socket_tcp_err(sock, "no resolver defined to resolve \"%*.s\"",
                                (int) sock->url.host.len, sock->url.host.data);
        return NGX_ERROR;
    }

    rslv_ctx->name = rslv_tmp.name;
    rslv_ctx->timeout = rslv_tmp.timeout;
    rslv_ctx->handler = ngx_wasm_socket_resolve_handler;
    rslv_ctx->data = sock;

    if (ngx_resolve_name(rslv_ctx) != NGX_OK) {
        ngx_log_debug0(NGX_LOG_DEBUG_WASM, sock->log, 0,
                       "wasm tcp socket failed running resolver immediately");
        return NGX_ERROR;
    }

    ngx_log_debug0(NGX_LOG_DEBUG_WASM, sock->log, 0,
                   "wasm tcp socket resolving...");

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
        ngx_wasm_socket_tcp_err(sock, "tcp socket - resolver error: %s",
                                ngx_resolver_strerror(ctx->state));
        goto error;
    }

#if (NGX_DEBUG)
    {
        u_char      text[NGX_SOCKADDR_STRLEN];
        ngx_str_t   addr;
        ngx_uint_t  i;

        addr.data = text;

        for (i = 0; i < ctx->naddrs; i++) {
            addr.len = ngx_sock_ntop(ctx->addrs[i].sockaddr,
                                     ctx->addrs[i].socklen, text,
                                     NGX_SOCKADDR_STRLEN, 0);

            ngx_log_debug1(NGX_LOG_DEBUG_WASM, sock->log, 0,
                           "name was resolved to %V", &addr);
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

    sock->resume(sock);
}


static ngx_int_t
ngx_wasm_socket_tcp_connect_peer(ngx_wasm_socket_tcp_t *sock)
{
    ngx_int_t               rc;
    ngx_connection_t       *c;
    ngx_peer_connection_t  *pc;

    ngx_log_debug0(NGX_LOG_DEBUG_WASM, sock->log, 0,
                   "wasm tcp socket connecting");

    ngx_wasm_assert(!sock->connected);

    if (!sock->resolved.sockaddr) {
        ngx_wasm_socket_tcp_err(sock, "tcp socket - resolver failed");
        return NGX_ERROR;
    }

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
        c->pool = ngx_create_pool(128, sock->log);
        if (c->pool == NULL) {
            return NGX_ERROR;
        }
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
        sock->connected = 1;

    } else if (rc == NGX_AGAIN) {
        ngx_wasm_socket_tcp_set_resume_handler(sock);

        ngx_add_timer(c->write, sock->connect_timeout);
    }

    return rc;
}


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

    ngx_wasm_socket_tcp_set_resume_handler(sock);

    return NGX_AGAIN;
}


#if 0
ngx_int_t
ngx_wasm_socket_reader_read_all(ngx_wasm_so127.0.0.1:$TEST_NGINX_SERVER_PORTcket_tcp_t *sock, ssize_t bytes)
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
                dd("reader error");
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
        ngx_wasm_socket_tcp_set_resume_handler(sock);

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

    ngx_wasm_socket_tcp_close(sock);

    if (sock->host.data) {
        ngx_pfree(sock->pool, sock->host.data);
    }

    if (c && c->pool) {
        ngx_destroy_pool(c->pool);
    }

    if (sock->free) {
        ngx_log_debug1(NGX_LOG_DEBUG_WASM, ngx_wasm_socket_log(sock), 0,
                       "wasm tcp socket free: %p", sock->free);

        ngx_wasm_assert(0);

        for (cl = sock->free; cl; /* void */) {
            ngx_wasm_assert(0);
            ln = cl;
            cl = cl->next;

            ngx_pfree(sock->pool, ln->buf->start);
            ngx_free_chain(sock->pool, ln);
        }

        sock->free = NULL;
    }

    if (sock->busy) {
        ngx_log_debug2(NGX_LOG_DEBUG_WASM, ngx_wasm_socket_log(sock), 0,
                       "wasm tcp socket busy: %p %i",
                       sock->busy, sock->nbusy);

        for (cl = sock->busy; cl; /* void */) {
            ln = cl;
            cl = cl->next;

            ngx_pfree(sock->pool, ln->buf->start);
            ngx_free_chain(sock->pool, ln);
        }

        sock->busy = NULL;
        sock->nbusy = 0;
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
        return;
    }

    if (ev->write) {
        sock->write_event_handler(sock);

    } else {
        sock->read_event_handler(sock);
    }

    sock->resume(sock);
}


static void
ngx_wasm_socket_tcp_nop_handler(ngx_wasm_socket_tcp_t *sock)
{
    ngx_log_debug0(NGX_LOG_DEBUG_WASM, sock->log, 0,
                   "wasm tcp socket nop handler");
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

        ngx_wasm_socket_tcp_err(sock, "tcp socket - timed out "
                                "connecting to \"%V:%ud\"",
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

        ngx_wasm_socket_tcp_err(sock, "tcp socket - timed out "
                                "writing to \"%V:%ud\"",
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

        ngx_wasm_socket_tcp_err(sock, "tcp socket - timed out "
                                "reading from \"%V:%ud\"",
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

#if (NGX_HAVE_UNIX_DOMAIN && 0)
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
