#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_http_proxy_wasm.h>
#include <ngx_http_proxy_wasm_dispatch.h>


static ngx_str_t *ngx_http_proxy_wasm_dispatch_strerror(
    ngx_http_proxy_wasm_dispatch_err_e err);
static void ngx_http_proxy_wasm_dispatch_err(
    ngx_http_proxy_wasm_dispatch_t *call);
static void ngx_http_proxy_wasm_dispatch_handler(ngx_event_t *ev);
static ngx_chain_t *ngx_http_proxy_wasm_dispatch_request(
    ngx_http_proxy_wasm_dispatch_t *call);
static ngx_int_t ngx_http_proxy_wasm_dispatch_resume_handler(
    ngx_wasm_socket_tcp_t *sock);


static char  ngx_http_proxy_wasm_dispatch_version_11[] = "HTTP/1.1" CRLF;
static char  ngx_http_proxy_wasm_connection[] = "Connection: close" CRLF;
static char  ngx_http_proxy_wasm_host[] = "Host: ";


static ngx_str_t  ngx_http_proxy_wasm_dispatch_errlist[] = {
    ngx_null_string,
    ngx_string("no :method"),
    ngx_string("no :path"),
    ngx_string("no :authority"),
    ngx_string("bad step"),
    ngx_string("no memory"),
    ngx_string("marshalling error"),
    ngx_string("unknown"),
};


static ngx_str_t *
ngx_http_proxy_wasm_dispatch_strerror(ngx_http_proxy_wasm_dispatch_err_e err)
{
    ngx_str_t  *msg;

    msg = ((ngx_uint_t) err < NGX_HTTP_PROXY_WASM_DISPATCH_ERR_UNKNOWN)
              ? &ngx_http_proxy_wasm_dispatch_errlist[err]
              : &ngx_http_proxy_wasm_dispatch_errlist[NGX_HTTP_PROXY_WASM_DISPATCH_ERR_UNKNOWN];

    return msg;
}


static void
ngx_http_proxy_wasm_dispatch_err(ngx_http_proxy_wasm_dispatch_t *call)
{
#if 0
    const char *fmt, ...)
{
    va_list                       args;
#endif
    u_char                       *p, *last;
    ngx_str_t                    *errmsg;
    ngx_wasm_socket_tcp_t        *sock;
    ngx_proxy_wasm_filter_ctx_t  *fctx;
    ngx_http_wasm_req_ctx_t      *rctx;
    u_char                        errbuf[NGX_MAX_ERROR_STR];

    sock = &call->sock;
    fctx = call->fctx;
    rctx = call->rctx;

    p = errbuf;
    last = p + NGX_MAX_ERROR_STR;

    p = ngx_slprintf(p, last, "dispatch failed");

    if (sock->errlen) {
        p = ngx_slprintf(p, last, " (%*.s)",
                         (int) sock->errlen, sock->err);
    }

    if (call->error) {
        errmsg = ngx_http_proxy_wasm_dispatch_strerror(call->error);
        p = ngx_slprintf(p, last, ": %V", errmsg);
    }

#if 0
    if (fmt)  {
        va_start(args, fmt);
        p = ngx_vslprintf(p, last, fmt, args);
        va_end(args);
    }
#endif

    fctx->ecode = NGX_PROXY_WASM_ERR_DISPATCH_FAILED;

    if (rctx->yield) {
        ngx_wasm_log_error(NGX_LOG_ERR, fctx->log, 0,
                           "%*s", p - (u_char *) &errbuf, &errbuf);

    } else {
        /* running */
        ngx_wavm_instance_trap_printf(fctx->ictx->instance, "%*s",
                                      p - (u_char *) &errbuf, &errbuf);
    }

    ngx_http_proxy_wasm_dispatch_destroy(call);

    fctx->call = NULL;
    rctx->yield = 0;
}


ngx_http_proxy_wasm_dispatch_t *
ngx_http_proxy_wasm_dispatch(ngx_proxy_wasm_filter_ctx_t *fctx,
    ngx_http_wasm_req_ctx_t *rctx, ngx_str_t *host,
    ngx_proxy_wasm_marshalled_map_t *headers,
    ngx_proxy_wasm_marshalled_map_t *trailers,
    ngx_str_t *body, ngx_msec_t timeout)
{
    static uint32_t                  callout_ids = 0;
    size_t                           i;
    ngx_buf_t                       *buf;
    ngx_event_t                     *ev;
    ngx_table_elt_t                 *elts, *elt;
    ngx_wasm_socket_tcp_t           *sock;
    ngx_wasm_socket_tcp_env_t        sock_env;
    ngx_http_request_t              *r;
    ngx_http_proxy_wasm_dispatch_t  *call = NULL;

    r = rctx->r;
    sock = NULL;

    /* alloc */

    call = ngx_calloc(sizeof(ngx_http_proxy_wasm_dispatch_t),
                      r->connection->log);
    if (call == NULL) {
        return NULL;
    }

    call->rctx = rctx;
    call->fctx = fctx;

    switch (fctx->parent->step) {
        case NGX_PROXY_WASM_STEP_LOG:
        case NGX_PROXY_WASM_STEP_DONE:
            /* r->pool was released */
            call->error = NGX_HTTP_PROXY_WASM_DISPATCH_ERR_BAD_STEP;
            goto error;
        default:
            break;
    }

    call->pool = ngx_create_pool(512, r->connection->log);
    if (call->pool == NULL) {
        call->error = NGX_HTTP_PROXY_WASM_DISPATCH_ERR_NOMEM;
        goto error;
    }

    call->id = callout_ids++,
    call->timeout = timeout;

    /* host */

    call->host.len = host->len;
    call->host.data = ngx_pnalloc(call->pool, host->len + 1);
    if (call->host.data == NULL) {
        call->error = NGX_HTTP_PROXY_WASM_DISPATCH_ERR_NOMEM;
        goto error;
    }

    ngx_memcpy(call->host.data, host->data, host->len);
    call->host.data[call->host.len] = '\0';

    /* headers/trailers */

    if (ngx_proxy_wasm_pairs_unmarshal(&call->headers, call->pool,
                                       headers) != NGX_OK
        || ngx_proxy_wasm_pairs_unmarshal(&call->trailers, call->pool,
                                          trailers) != NGX_OK)
    {
        call->error = NGX_HTTP_PROXY_WASM_DISPATCH_ERR_MARSHALLING;
        goto error;
    }

    elts = call->headers.elts;

    for (i = 0; i < call->headers.nelts; i++) {
        elt = &elts[i];

#if 0
        dd("%.*s: %.*s",
           (int) elt->key.len, elt->key.data,
           (int) elt->value.len, elt->value.data);
#endif

        if (elt->key.data[0] == ':') {

            if (ngx_strncmp(elt->key.data, ":method", 7) == 0) {
                call->method.len = elt->value.len;
                call->method.data = elt->value.data;

            } else if (ngx_strncmp(elt->key.data, ":path", 5) == 0) {
                call->uri.len = elt->value.len;
                call->uri.data = elt->value.data;

            } else if (ngx_strncmp(elt->key.data, ":authority", 10) == 0) {
                call->authority.len = elt->value.len;
                call->authority.data = elt->value.data;

            }
            else if (ngx_strncmp(elt->key.data, ":scheme", 7) == 0) {
#if (NGX_SSL)
                if (ngx_strncmp(elt->value.data, "https", 5) == 0) {
                    call->enable_ssl = 1;
                    dd("tls enabled");

                } else if (ngx_strncmp(elt->value.data, "http", 4) == 0) {
                    call->enable_ssl = 0;
                    dd("tls disabled");

                } else {
                    ngx_wasm_log_error(NGX_LOG_WARN, r->connection->log, 0,
                                       "unknown scheme \"%V\"",
                                       &elt->key);
                }
#else
                ngx_wasm_log_error(NGX_LOG_WARN, r->connection->log, 0,
                                   "scheme ignored - NGX_SSL not enabled");
#endif
            }
            else {
                ngx_wasm_log_error(NGX_LOG_WASM_NYI, r->connection->log, 0,
                                   "NYI - dispatch_http_call header \"%V\"",
                                   &elt->key);
            }

            elt->hash = 0;
            continue;
        }

        elt->hash = 1;
    }

    if (!call->method.len) {
        call->error = NGX_HTTP_PROXY_WASM_DISPATCH_ERR_BAD_METHOD;
        goto error;

    } else if (!call->uri.len) {
        call->error = NGX_HTTP_PROXY_WASM_DISPATCH_ERR_BAD_PATH;
        goto error;

    } else if (!call->authority.len) {
        call->error = NGX_HTTP_PROXY_WASM_DISPATCH_ERR_BAD_AUTHORITY;
        goto error;
    }

    /* body */

    if (body && body->len) {
        call->req_body_len = body->len;
        call->req_body = ngx_wasm_chain_get_free_buf(r->connection->pool,
                                                     &rctx->free_bufs,
                                                     body->len, buf_tag, 1);
        if (call->req_body == NULL) {
            goto error;
        }

        buf = call->req_body->buf;
        buf->last = ngx_copy(buf->last, body->data, body->len);
    }

    /* init */

    sock = &call->sock;

    sock_env.connection = r->connection;
    sock_env.buf_tag = buf_tag;
    sock_env.kind = NGX_WASM_SOCKET_TCP_KIND_HTTP;
    sock_env.ctx.request = rctx;

    if (ngx_wasm_socket_tcp_init(sock, &call->host, &sock_env,
#if (NGX_SSL)
                                 call->enable_ssl ? 443 : 80
#else
                                 80
#endif
    ) != NGX_OK) {
        goto error;
    }

    sock->read_timeout = call->timeout;
    sock->send_timeout = call->timeout;
    sock->connect_timeout = call->timeout;

    call->http_reader.pool = r->connection->pool;  /* longer lifetime than call */
    call->http_reader.log = r->connection->log;
    call->http_reader.rctx = rctx;
    call->http_reader.sock = sock;

    /* dispatch */

    ev = ngx_calloc(sizeof(ngx_event_t), r->connection->log);
    if (ev == NULL) {
        goto error;
    }

    ev->handler = ngx_http_proxy_wasm_dispatch_handler;
    ev->data = call;
    ev->log = r->connection->log;

    ngx_post_event(ev, &ngx_posted_events);

    fctx->call = call;

    return call;

error:

    if (call) {
        ngx_http_proxy_wasm_dispatch_err(call);
    }

    return NULL;
}


void
ngx_http_proxy_wasm_dispatch_destroy(ngx_http_proxy_wasm_dispatch_t *call)
{
    ngx_chain_t              *cl;
    ngx_wasm_socket_tcp_t    *sock;
    ngx_http_wasm_req_ctx_t  *rctx;

    sock = &call->sock;
    rctx = call->rctx;

    ngx_wasm_socket_tcp_destroy(sock);

    if (call->host.data) {
        ngx_pfree(call->pool, call->host.data);
        call->host.data = NULL;
    }

    if (call->req_body) {
        for (cl = call->req_body; cl; cl = cl->next) {
            dd("body chain: %p, next %p", cl, cl->next);
            cl->buf->pos = cl->buf->last;
        }

        rctx->free_bufs = call->req_body;
    }

    if (call->pool) {
        ngx_destroy_pool(call->pool);  /* reader->pool */
        call->pool = NULL;
    }

    ngx_free(call);
}


static void
ngx_http_proxy_wasm_dispatch_handler(ngx_event_t *ev)
{
    ngx_int_t                        rc;
    ngx_http_proxy_wasm_dispatch_t  *call = ev->data;
    ngx_http_wasm_req_ctx_t         *rctx = call->rctx;
    ngx_wasm_socket_tcp_t           *sock = &call->sock;

    ngx_free(ev);

    sock->resume = ngx_http_proxy_wasm_dispatch_resume_handler;
    sock->data = call;

    rc = sock->resume(sock);
    if (rc == NGX_ERROR) {
        ngx_http_wasm_resume(rctx, 1, 1);
    }
}


static ngx_int_t
ngx_http_proxy_wasm_dispatch_set_header(ngx_http_request_t *r,
    ngx_str_t *key, ngx_str_t *value)
{
    return ngx_http_wasm_set_req_header(r, key, value,
                                        NGX_HTTP_WASM_HEADERS_APPEND);
}


static ngx_chain_t *
ngx_http_proxy_wasm_dispatch_request(ngx_http_proxy_wasm_dispatch_t *call)
{
    size_t                    i, len = 0;
    ngx_chain_t              *nl;
    ngx_buf_t                *b;
    ngx_list_part_t          *part;
    ngx_table_elt_t          *elt, *elts;
    ngx_http_wasm_req_ctx_t  *rctx;
    ngx_http_request_t       *fake_r;
    ngx_http_request_t       *r;

    if (call->req_out) {
        return call->req_out;
    }

    rctx = call->rctx;
    fake_r = &call->fake_r;
    r = rctx->r;

    fake_r->signature = NGX_WASM_MODULE;
    fake_r->connection = rctx->connection;
    fake_r->pool = r->pool;

    if (ngx_list_init(&fake_r->headers_in.headers, fake_r->pool, 10,
                      sizeof(ngx_table_elt_t))
        != NGX_OK)
    {
        return NULL;
    }

    /* headers */

    elts = (ngx_table_elt_t *) call->headers.elts;

    for (i = 0; i < call->headers.nelts; i++) {
        elt = &elts[i];

        if (elt->hash == 0) {
            continue;
        }

        if (ngx_http_proxy_wasm_dispatch_set_header(fake_r,
                                                    &elt->key, &elt->value)
            != NGX_OK)
        {
            return NULL;
        }
    }

    part = &fake_r->headers_in.headers.part;
    elts = part->elts;

    for (i = 0; /* void */; i++) {

        if (i >= part->nelts) {
            if (part->next == NULL) {
                break;
            }

            part = part->next;
            elts = part->elts;
            i = 0;
        }

        elt = &elts[i];

        len += elt->key.len + sizeof(": ") - 1 + elt->value.len
               + sizeof(CRLF) - 1;
    }

    fake_r->headers_in.content_length_n = call->req_body_len;

    /**
     * GET /dispatched HTTP/1.1
     * Host:
     * Connection:
     * Content-Length:
     */

    len += call->method.len + 1 + call->uri.len + 1
           + sizeof(ngx_http_proxy_wasm_dispatch_version_11) - 1;

    len += sizeof(ngx_http_proxy_wasm_host) - 1 + call->authority.len
           + sizeof(CRLF) - 1;

    len += sizeof(ngx_http_proxy_wasm_connection) - 1;

    if (fake_r->headers_in.content_length == NULL
        && fake_r->headers_in.content_length_n >= 0)
    {
        len += sizeof("Content-Length: ") - 1 + NGX_OFF_T_LEN
               + sizeof(CRLF) - 1;
    }

    len += sizeof(CRLF) - 1;

    /* headers buffer */

    nl = ngx_wasm_chain_get_free_buf(r->connection->pool,
                                     &rctx->free_bufs, len,
                                     buf_tag, 1);
    if (nl == NULL) {
        return NULL;
    }

    b = nl->buf;

    /**
     * GET /dispatched HTTP/1.1
     * Host:
     * Connection:
     * Content-Length:
     */
    b->last = ngx_cpymem(b->last, call->method.data, call->method.len);
    *b->last++ = ' ';

    b->last = ngx_cpymem(b->last, call->uri.data, call->uri.len);
    *b->last++ = ' ';

    b->last = ngx_cpymem(b->last, ngx_http_proxy_wasm_dispatch_version_11,
                         sizeof(ngx_http_proxy_wasm_dispatch_version_11) - 1);

    b->last = ngx_cpymem(b->last, ngx_http_proxy_wasm_host,
                         sizeof(ngx_http_proxy_wasm_host) - 1);
    b->last = ngx_cpymem(b->last, call->authority.data, call->authority.len);
    *b->last++ = CR;
    *b->last++ = LF;

    b->last = ngx_cpymem(b->last, ngx_http_proxy_wasm_connection,

                         sizeof(ngx_http_proxy_wasm_connection) - 1);

    if (fake_r->headers_in.content_length == NULL
        && fake_r->headers_in.content_length_n >= 0)
    {
        b->last = ngx_sprintf(b->last, "Content-Length: %O" CRLF,
                              fake_r->headers_in.content_length_n);
    }

    /* headers */

    part = &fake_r->headers_in.headers.part;
    elts = part->elts;

    for (i = 0; /* void */; i++) {

        if (i >= part->nelts) {
            if (part->next == NULL) {
                break;
            }

            part = part->next;
            elts = part->elts;
            i = 0;
        }

        elt = &elts[i];

        b->last = ngx_cpymem(b->last, elt->key.data, elt->key.len);
        *b->last++ = ':';
        *b->last++ = ' ';

        b->last = ngx_cpymem(b->last, elt->value.data, elt->value.len);
        *b->last++ = CR;
        *b->last++ = LF;
    }

    *b->last++ = CR;
    *b->last++ = LF;

    /* body */

    if (call->req_body_len) {
        nl->next = call->req_body;
    }

    call->req_out = nl;

    return nl;
}


static ngx_int_t
ngx_http_proxy_wasm_dispatch_resume_handler(ngx_wasm_socket_tcp_t *sock)
{
    size_t                           i;
    ngx_int_t                        rc = NGX_ERROR;
    ngx_uint_t                       n_headers;
    ngx_chain_t                     *nl;
    ngx_list_part_t                 *part;
    ngx_wavm_instance_t             *instance;
    ngx_http_proxy_wasm_dispatch_t  *call = sock->data;
    ngx_http_wasm_req_ctx_t         *rctx = call->rctx;
    ngx_http_request_t              *r = rctx->r;
    ngx_proxy_wasm_filter_ctx_t     *fctx = call->fctx;
    ngx_proxy_wasm_filter_t         *filter = fctx->filter;

    dd("enter");

    ngx_wasm_assert(&call->sock == sock);

    if (sock->err) {
        goto error;
    }

    switch (call->state) {

    case NGX_HTTP_PROXY_WASM_DISPATCH_START:

        ngx_log_debug0(NGX_LOG_DEBUG_ALL, sock->log, 0,
                       "proxy_wasm http dispatch connecting");

        call->state = NGX_HTTP_PROXY_WASM_DISPATCH_CONNECTING;

        /* fallthrough */

    case NGX_HTTP_PROXY_WASM_DISPATCH_CONNECTING:

        rc = ngx_wasm_socket_tcp_connect(sock);
        if (rc == NGX_ERROR) {
            goto error;
        }

        if (rc == NGX_AGAIN) {
            break;
        }

        ngx_wasm_assert(rc == NGX_OK);

        call->state = NGX_HTTP_PROXY_WASM_DISPATCH_SSL_HANDSHAKING;

        /* fallthrough */

    case NGX_HTTP_PROXY_WASM_DISPATCH_SSL_HANDSHAKING:

#if (NGX_SSL)
        if (call->enable_ssl) {
            rc = ngx_wasm_socket_tcp_ssl_handshake(sock);
            if (rc == NGX_ERROR) {
                goto error;
            }

            if (rc == NGX_AGAIN) {
                break;
            }

            ngx_wasm_assert(rc == NGX_OK);
        }
#endif

        call->state = NGX_HTTP_PROXY_WASM_DISPATCH_SENDING;

        /* fallthrough */

    case NGX_HTTP_PROXY_WASM_DISPATCH_SENDING:

        nl = ngx_http_proxy_wasm_dispatch_request(call);
        if (nl == NULL) {
            goto error;
        }

        ngx_log_debug0(NGX_LOG_DEBUG_ALL, sock->log, 0,
                       "proxy_wasm http dispatch sending request");

        rc = ngx_wasm_socket_tcp_send(sock, nl);

        if (rc == NGX_ERROR) {
            goto error;
        }

        if (rc == NGX_AGAIN) {
            break;
        }

        ngx_chain_update_chains(r->connection->pool,
                                &rctx->free_bufs, &rctx->busy_bufs,
                                &nl, buf_tag);

        ngx_wasm_assert(rc == NGX_OK);

        call->state = NGX_HTTP_PROXY_WASM_DISPATCH_RECEIVING;

        /* fallthrough */

    case NGX_HTTP_PROXY_WASM_DISPATCH_RECEIVING:

        rc = ngx_wasm_socket_tcp_read(sock, ngx_wasm_socket_read_http_response,
                                      &call->http_reader);
        if (rc == NGX_ERROR) {
            goto error;
        }

        if (rc == NGX_AGAIN) {
            break;
        }

        ngx_wasm_assert(rc == NGX_OK);

        call->state = NGX_HTTP_PROXY_WASM_DISPATCH_RECEIVED;

        /* fallthrough */

    case NGX_HTTP_PROXY_WASM_DISPATCH_RECEIVED:

        ngx_wasm_socket_tcp_close(sock);

        part = &call->http_reader.fake_r.upstream->headers_in.headers.part;

        for (i = 0, n_headers = 0; /* void */; i++, n_headers++) {
            if (i >= part->nelts) {
                if (part->next == NULL) {
                    break;
                }

                part = part->next;
                i = 0;
            }

            /* void */
        }

        /**
         * TODO: move to reuse proxy_wasm_resume logic
         *   - new step: dispatch_cb
         */

        ngx_log_debug3(NGX_LOG_DEBUG_ALL, sock->log, 0,
                       "proxy_wasm http dispatch response received "
                       "(fctx->id: %d, token_id: %d, n_headers: %d)",
                       fctx->id, call->id, n_headers);

        instance = ngx_proxy_wasm_fctx2instance(fctx);

        if (instance->trapped) {
            fctx->ecode = NGX_PROXY_WASM_ERR_INSTANCE_TRAPPED;

            ngx_proxy_wasm_log_error(NGX_LOG_ERR, fctx->log, fctx->ecode,
                                     "proxy_wasm \"%V\" filter (%l/%l) "
                                     "failed resuming after dispatch",
                                     filter->name, filter->index + 1,
                                     *filter->n_filters);

            rc = NGX_ABORT;
            goto error;
        }

        fctx->ictx->current_ctx = fctx;

        rc = ngx_wavm_instance_call_funcref(instance,
                                            filter->proxy_on_http_call_response,
                                            NULL, fctx->id, call->id,
                                            n_headers,
                                            call->http_reader.body_len, 0);
        if (rc == NGX_ABORT) {
            fctx->ecode = NGX_PROXY_WASM_ERR_INSTANCE_TRAPPED;
            goto error;
        }

        ngx_http_proxy_wasm_dispatch_destroy(call);

        fctx->call = NULL;
        rctx->yield = 0;

        /* resume main handled by wasm callback */
        break;

    default:
        ngx_wasm_log_error(NGX_LOG_WASM_NYI, fctx->log, 0,
                           "NYI - dispatch state: %d", call->state);
        rc = NGX_ERROR;
        goto error;

    }

    ngx_wasm_assert(rc == NGX_AGAIN || rc == NGX_OK);

    goto done;

error:

    if (rc == NGX_ABORT) {
        /* catch trap for tcp socket resume retval */
        rc = NGX_ERROR;
    }

    ngx_http_proxy_wasm_dispatch_err(call);

    ngx_wasm_assert(rc == NGX_ERROR);

done:

    dd("exit rc: %ld", rc);

    return rc;
}
