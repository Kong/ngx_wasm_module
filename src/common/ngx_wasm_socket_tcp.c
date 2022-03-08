#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_wasm_socket_tcp.h>


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
ngx_wasm_socket_tcp_init(ngx_wasm_socket_tcp_t *sock, ngx_str_t *host)
{
    sock->host.len = host->len;
    sock->host.data = ngx_pstrdup(sock->pool, host);
    if (sock->host.data == NULL) {
        return NGX_ERROR;
    }

    if (!sock->read_timeout) {
        sock->read_timeout = NGX_WASM_SOCKET_TCP_DEFAULT_TIMEOUT;
    }

    if (!sock->send_timeout) {
        sock->send_timeout = NGX_WASM_SOCKET_TCP_DEFAULT_TIMEOUT;
    }

    if (!sock->connect_timeout) {
        sock->connect_timeout = NGX_WASM_SOCKET_TCP_DEFAULT_TIMEOUT;
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
ngx_wasm_socket_tcp_connect(ngx_wasm_socket_tcp_t *sock,
    ngx_http_wasm_req_ctx_t *rctx)
{
    ngx_resolver_ctx_t        *rslv_ctx, rslv_tmp;
#ifdef NGX_WASM_HTTP
    ngx_http_core_loc_conf_t  *clcf;
    ngx_http_request_t        *r = rctx->r;
#endif

    if (sock->errlen) {
        return NGX_ERROR;
    }

    if (sock->connected) {
        return NGX_OK;
    }

    sock->rctx = rctx;
    sock->resolved = ngx_pcalloc(sock->pool, sizeof(ngx_http_upstream_resolved_t));
    if (sock->resolved == NULL) {
        return NGX_ERROR;
    }

    if (sock->url.addrs && sock->url.addrs[0].sockaddr) {
        sock->resolved->sockaddr = sock->url.addrs[0].sockaddr;
        sock->resolved->socklen = sock->url.addrs[0].socklen;
        sock->resolved->host = sock->url.addrs[0].name;
        sock->resolved->naddrs = 1;

        return ngx_wasm_socket_tcp_connect_peer(sock);
    }

    sock->resolved->host = sock->host;
    sock->resolved->port = sock->url.default_port;

    /* resolve */

#ifdef NGX_WASM_HTTP
    clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);
#endif

    rslv_tmp.name = sock->url.host;
    rslv_ctx = ngx_resolve_start(clcf->resolver, &rslv_tmp);
    if (rslv_ctx == NULL) {
        ngx_wasm_socket_tcp_err(sock, "failed starting resolver");
        return NGX_ERROR;

    } else if (rslv_ctx == NGX_NO_RESOLVER) {
        ngx_wasm_socket_tcp_err(sock, "no resolver defined to resolve \"%*.s\"",
                                (int) sock->url.host.len, sock->url.host.data);
        return NGX_ERROR;
    }

    rslv_ctx->name = sock->url.host;
    rslv_ctx->handler = ngx_wasm_socket_resolve_handler;
    rslv_ctx->timeout = clcf->resolver_timeout;
    rslv_ctx->data = sock;

    if (ngx_resolve_name(rslv_ctx) != NGX_OK) {
        ngx_log_debug0(NGX_LOG_DEBUG_WASM, sock->log, 0,
                       "wasm tcp socket fail to run resolver immediately");
        return NGX_ERROR;
    }

    ngx_log_debug0(NGX_LOG_DEBUG_ALL, sock->log, 0,
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

    ngx_log_debug0(NGX_LOG_DEBUG_ALL, sock->rctx->connection->log, 0,
                   "wasm tcp socket resolve handler");

    if (ctx->state) {
        ngx_wasm_log_error(NGX_LOG_ERR, sock->log, 0,
                           "tcp socket resolver error: %s",
                           ngx_resolver_strerror(ctx->state));
        goto error;
    }

#if 0
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
            htons(sock->resolved->port);
        break;
#endif
    default: /* AF_INET */
        ((struct sockaddr_in *) sockaddr)->sin_port =
            htons(sock->resolved->port);
        break;
    }

    p = ngx_pnalloc(sock->pool, NGX_SOCKADDR_STRLEN);
    if (p == NULL) {
        goto error;
    }

    sock->resolved->naddrs = 1;
    sock->resolved->sockaddr = sockaddr;
    sock->resolved->socklen = socklen;
    sock->resolved->host.len = ngx_sock_ntop(sockaddr, socklen, p,
                                             NGX_SOCKADDR_STRLEN, 1);
    sock->resolved->host.data = p;

    ngx_resolve_name_done(ctx);

    sock->resolved->ctx = NULL;

    /* connect */

    ngx_wasm_socket_tcp_connect_peer(sock);

    return;

error:

    ngx_resolve_name_done(ctx);
}


static ngx_int_t
ngx_wasm_socket_tcp_connect_peer(ngx_wasm_socket_tcp_t *sock)
{
    ngx_int_t               rc;
    ngx_connection_t       *c;
    ngx_peer_connection_t  *pc;
#ifdef NGX_WASM_HTTP
    ngx_http_request_t     *r = sock->rctx->r;
#endif

    ngx_log_debug0(NGX_LOG_DEBUG_ALL, r->connection->log, 0,
                   "wasm tcp socket connecting");

    ngx_wasm_assert(!sock->connected);

    if (!sock->resolved->sockaddr) {
        ngx_wasm_socket_tcp_err(sock, "tcp socket resolver failed");
        return NGX_ERROR;
    }

    pc = &sock->peer;
    pc->log = sock->log;
    pc->get = ngx_wasm_socket_tcp_get_peer;
    pc->sockaddr = sock->resolved->sockaddr;
    pc->socklen = sock->resolved->socklen;
    pc->name = &sock->resolved->host;

    rc = ngx_event_connect_peer(pc);

    if (rc == NGX_ERROR) {
        return NGX_ERROR;

    } else if (rc == NGX_BUSY) {
        ngx_wasm_socket_tcp_err(sock, "no live connection");
        return NGX_BUSY;

    } if (rc == NGX_DECLINED) {
        sock->socket_errno = ngx_socket_errno;
        ngx_wasm_socket_tcp_err(sock, NULL);
        return NGX_ERROR;
    }

    ngx_wasm_assert(rc == NGX_OK || rc == NGX_AGAIN);

    sock->write_event_handler = ngx_wasm_socket_tcp_connect_handler;
    sock->read_event_handler = ngx_wasm_socket_tcp_connect_handler;

    c = pc->connection;

    if (c->pool == NULL) {
        c->pool = ngx_create_pool(128, r->connection->log);
        if (c->pool == NULL) {
            return NGX_ERROR;
        }
    }

    c->log = r->connection->log;
    c->pool->log = c->log;
    c->read->handler = ngx_wasm_socket_tcp_handler;
    c->write->handler = ngx_wasm_socket_tcp_handler;
    c->read->log = c->log;
    c->write->log = c->log;
    c->data = sock;
    c->sendfile &= r->connection->sendfile;

    if (rc == NGX_OK) {
        sock->connected = 1;

    } else if (rc == NGX_AGAIN) {
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
    b = cl->buf;

    for ( ;; ) {

        n = c->send(c, b->pos, b->last - b->pos);

        if (n >= 0) {
            b->pos += n;

            if (b->pos == b->last) {
                ngx_log_debug0(NGX_LOG_DEBUG_WASM, sock->log, 0,
                               "wasm tcp socket sent all the data");

                if (c->write->timer_set) {
                    ngx_del_timer(c->write);
                }

                if (ngx_handle_write_event(c->write, 0) != NGX_OK) {
                    return NGX_ERROR;
                }

                return NGX_OK;
            }

            ngx_log_debug0(NGX_LOG_DEBUG_WASM, sock->log, 0,
                           "wasm tcp socket more data to send");

            continue;
        }

        ngx_wasm_assert(n == NGX_ERROR || n == NGX_AGAIN);

        continue;
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


#ifdef NGX_WASM_HTTP
ngx_int_t
ngx_wasm_socket_read_http_response(ngx_wasm_socket_tcp_t *sock,
    ssize_t bytes)
{
    ngx_log_debug0(NGX_LOG_DEBUG_WASM, sock->log, 0,
                   "wasm tcp socket reading http response");

    return ngx_wasm_read_http_response(&sock->buffer, sock->buf_in, bytes,
                                       sock->reader_ctx);
}
#endif


ngx_int_t
ngx_wasm_socket_tcp_read(ngx_wasm_socket_tcp_t *sock,
    ngx_wasm_socket_tcp_reader_pt reader, void *reader_ctx)
{
    off_t              size;
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
        cl = ngx_wasm_chain_get_free_buf(sock->pool, &sock->rctx->free_bufs,
                                         12, buf_tag); // TODO: move
        if (cl == NULL) {
            return NGX_ERROR;
        }

        sock->bufs_in = cl;
        sock->buf_in = sock->bufs_in;
        sock->buffer = *sock->buf_in->buf;
    }

    sock->reader = reader;
    sock->reader_ctx = reader_ctx;
    sock->read_event_handler = ngx_wasm_socket_tcp_receive_handler;

    b = &sock->buffer;
    c = sock->peer.connection;
    rev = c->read;

    for ( ;; ) {

        size = b->last - b->pos;

        dd("b: %p: %.*s (size: %ld, eof: %d)",
           b, size, b->pos, size, (int) sock->eof);

        if (size || sock->eof) {

            rc = sock->reader(sock, size);
            if (rc == NGX_ERROR) {
                return NGX_ERROR;
            }

            if (rc == NGX_OK) {
                ngx_log_debug0(NGX_LOG_DEBUG_WASM, sock->log, 0,
                               "wasm tcp socket reading done");

                if (ngx_handle_read_event(rev, 0) != NGX_OK) {
                    return NGX_ERROR;
                }

                return NGX_OK;
            }

            ngx_wasm_assert(rc == NGX_AGAIN);

            continue;
        }

        if (rev->active && !rev->ready) {
            dd("rev not ready");
            rc = NGX_AGAIN;
            break;
        }

        size = b->end - b->last;

        dd("bytes left in buffer: %ld (pos: %p, end: %p",
           size, b->pos, b->end);

        if (size == 0) {
            cl = ngx_wasm_chain_get_free_buf(sock->pool, &sock->rctx->free_bufs,
                                             12, buf_tag); // TODO: move
            if (cl == NULL) {
                return NGX_ERROR;
            }

            sock->buf_in->next = cl;
            sock->buf_in = cl;
            sock->buffer = *sock->buf_in->buf;

            b = &sock->buffer;

            size = b->end - b->last;
        }

        ngx_log_debug1(NGX_LOG_DEBUG_WASM, sock->log, 0,
                       "wasm tcp socket trying to receive data (max: %O)",
                       size);

        n = c->recv(c, b->last, size);

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
                           "wasm tcp socket closed");
            sock->eof = 1;
            continue;
        }

        b->last += n;
    }

    ngx_wasm_assert(rc == NGX_OK || rc == NGX_AGAIN);

    if (ngx_handle_read_event(rev, 0) != NGX_OK) {
        return NGX_ERROR;
    }

    if (rev->active) {
        ngx_add_timer(rev, sock->read_timeout);

    } else if (rev->timer_set) {
        ngx_wasm_assert(rc == NGX_OK);

        ngx_del_timer(rev);
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

    ngx_log_debug0(NGX_LOG_DEBUG_WASM, sock->log, 0,
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
#if 0
    ngx_chain_t       *cl, *ln;
#endif
    ngx_connection_t  *c = sock->peer.connection;

    ngx_wasm_socket_tcp_close(sock);

    if (sock->host.data) {
        ngx_pfree(sock->pool, sock->host.data);
    }

    if (sock->resolved) {
        ngx_pfree(sock->pool, sock->resolved);
    }

    if (c && c->pool) {
        ngx_destroy_pool(c->pool);
    }

#if 0
    ngx_log_debug1(NGX_LOG_DEBUG_ALL, sock->log, 0,
                   "sock free: %p", sock->free);

    if (sock->free) {
        for (cl = sock->free; cl; /* void */) {
            ln = cl;
            cl = cl->next;
            ngx_pfree(sock->pool, ln->buf->start);
            ngx_free_chain(sock->pool, ln);
        }

        sock->free = NULL;
    }

    ngx_log_debug2(NGX_LOG_DEBUG_ALL, sock->log, 0,
                   "sock busy: %p %i", sock->busy, sock->nbusy);

    if (sock->busy) {
        for (cl = sock->busy; cl; /* void */) {
            ln = cl;
            cl = cl->next;
            ngx_pfree(sock->pool, ln->buf->start);
            ngx_free_chain(sock->pool, ln);
        }

        sock->busy = NULL;
        sock->nbusy = 0;
    }
#endif
}


static void
ngx_wasm_socket_tcp_finalize_read(ngx_wasm_socket_tcp_t *sock)
{
    ngx_connection_t  *c;

    if (sock->read_closed) {
        return;
    }

    sock->read_closed = 1;

#if 0
    if (ctx && u->bufs_in) {
        ll = &u->bufs_in;
        for (cl = u->bufs_in; cl; cl = cl->next) {
            dd("bufs_in chain: %p, next %p", cl, cl->next);
            cl->buf->pos = cl->buf->last;
            ll = &cl->next;
        }

        dd("ctx: %p", ctx);
        dd("free recv bufs: %p", ctx->free_recv_bufs);
        *ll = ctx->free_recv_bufs;
        ctx->free_recv_bufs = u->bufs_in;
        u->bufs_in = NULL;
        u->buf_in = NULL;
        ngx_memzero(&u->buffer, sizeof(ngx_buf_t));
    }
#endif

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
    ngx_connection_t       *c = ev->data, *connection;
    ngx_wasm_socket_tcp_t  *sock = c->data;

    ngx_log_debug1(NGX_LOG_DEBUG_ALL, ev->log, 0,
                   "wasm tcp socket handler (wev: %d)",
                   (int) ev->write);

    if (sock->closed) {
        return;
    }

    connection = sock->rctx->connection;

    if (ev->write) {
        sock->write_event_handler(sock);

    } else {
        sock->read_event_handler(sock);
    }

    sock->resume(sock);

    ngx_http_run_posted_requests(connection);
}


static void
ngx_wasm_socket_tcp_nop_handler(ngx_wasm_socket_tcp_t *sock)
{
    ngx_log_debug0(NGX_LOG_DEBUG_ALL, sock->log, 0,
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
        ngx_log_debug2(NGX_LOG_DEBUG_ALL, sock->log, 0,
                       "wasm tcp socket timed out connecting to \"%V:%ud\"",
                       &c->addr_text, ngx_inet_get_port(sock->peer.sockaddr));

        ngx_wasm_socket_tcp_err(sock, "tcp socket timed out "
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

    ngx_log_debug2(NGX_LOG_DEBUG_ALL, sock->log, 0,
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

    ngx_log_debug2(NGX_LOG_DEBUG_ALL, sock->log, 0,
                   "wasm tcp socket send handler for \"%V:%ud\"",
                   &c->addr_text, ngx_inet_get_port(sock->peer.sockaddr));

    if (c->write->timedout) {
        ngx_log_debug2(NGX_LOG_DEBUG_ALL, sock->log, 0,
                       "wasm tcp socket timed out writing to \"%V:%ud\"",
                       &c->addr_text, ngx_inet_get_port(sock->peer.sockaddr));

        ngx_wasm_socket_tcp_err(sock, "tcp socket write timed out (\"%V:%ud\")",
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

    ngx_log_debug2(NGX_LOG_DEBUG_ALL, sock->log, 0,
                   "wasm tcp socket receive handler for \"%V:%ud\"",
                   &c->addr_text, ngx_inet_get_port(sock->peer.sockaddr));

    if (c->read->timedout) {
        ngx_log_debug2(NGX_LOG_DEBUG_ALL, sock->log, 0,
                       "wasm tcp socket timed out reading from \"%V:%ud\"",
                       &c->addr_text, ngx_inet_get_port(sock->peer.sockaddr));

        ngx_wasm_socket_tcp_err(sock, "tcp socket read timed out (\"%V:%ud\")",
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
