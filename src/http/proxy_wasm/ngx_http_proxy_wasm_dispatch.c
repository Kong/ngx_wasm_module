#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_http_proxy_wasm.h>
#include <ngx_http_proxy_wasm_dispatch.h>


static ngx_str_t *ngx_http_proxy_wasm_dispatch_strerror(
    ngx_http_proxy_wasm_dispatch_err_e err);
static void ngx_http_proxy_wasm_dispatch_err(
    ngx_http_proxy_wasm_dispatch_t *call, unsigned resume);
static void ngx_http_proxy_wasm_dispatch_handler(ngx_event_t *ev);
static ngx_chain_t *ngx_http_proxy_wasm_dispatch_request(
    ngx_http_proxy_wasm_dispatch_t *call);
static ngx_int_t ngx_http_proxy_wasm_dispatch_resume_handler(
    ngx_wasm_socket_tcp_t *sock);


static char  ngx_http_header_version11[] = "HTTP/1.1" CRLF;
static char  ngx_http_host_header[] = "Host";
static char  ngx_http_connection_header[] = "Connection";
static char  ngx_http_cl_header[] = "Content-Length";
static size_t  ngx_http_host_header_len = sizeof(ngx_http_host_header) - 1;
static size_t  ngx_http_connection_header_len =
    sizeof(ngx_http_connection_header) - 1;


static ngx_str_t  ngx_http_proxy_wasm_dispatch_errlist[] = {
    ngx_null_string,
    ngx_string("no :method"),
    ngx_string("no :path"),
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
              : &ngx_http_proxy_wasm_dispatch_errlist[
                     NGX_HTTP_PROXY_WASM_DISPATCH_ERR_UNKNOWN];

    return msg;
}


static ngx_int_t
invoke_on_http_dispatch_response(ngx_proxy_wasm_exec_t *pwexec,
    ngx_http_proxy_wasm_dispatch_t *call)
{
    ngx_proxy_wasm_step_e   step;
    ngx_proxy_wasm_err_e    ecode;

    /**
     * Set current call for subsequent call detection after the step
     * (no yielding).
     */
    pwexec->dispatch_call = call;

    /**
     * Save step: ngx_proxy_wasm_run_step will set pwctx->step (for host
     * calls that need it), but we want to resume to the current step when
     * all calls are finished (i.e. on_request_headers), so we'll save it
     * here and set it back after run_step.
     *
     * This could eventually move to ngx_proxy_wasm_run_step if needed for
     * other "single step invocations".
     */
    step = pwexec->parent->step;

#ifdef NGX_WASM_HTTP
    pwexec->parent->phase = ngx_wasm_phase_lookup(&ngx_http_wasm_subsystem,
                                                  NGX_WASM_BACKGROUND_PHASE);
#endif

    ecode = ngx_proxy_wasm_run_step(pwexec,
                                    NGX_PROXY_WASM_STEP_DISPATCH_RESPONSE);
    if (ecode != NGX_PROXY_WASM_ERR_NONE) {
        /* catch trap for tcp socket resume retval */
        return NGX_ERROR;
    }

    /* reset step */
    pwexec->parent->step = step;

    /* remove current call now that callback was invoked */
    pwexec->dispatch_call = NULL;

    return NGX_OK;
}


static void
ngx_http_proxy_wasm_dispatch_err(ngx_http_proxy_wasm_dispatch_t *call,
    unsigned resume)
{
#if 0
    const char *fmt, ...)
{
    va_list                       args;
#endif
    u_char                   *p, *last;
    ngx_str_t                *errmsg;
    ngx_wasm_socket_tcp_t    *sock;
    ngx_proxy_wasm_exec_t    *pwexec;
    ngx_http_wasm_req_ctx_t  *rctx;
    u_char                    errbuf[NGX_MAX_ERROR_STR];

    sock = &call->sock;
    pwexec = call->pwexec;
    rctx = call->rctx;

    p = errbuf;
    last = p + NGX_MAX_ERROR_STR;

    p = ngx_slprintf(p, last, "dispatch failed");

    if (sock->errlen) {
        ngx_wa_assert(sock->status != NGX_WASM_SOCKET_STATUS_OK);

        p = ngx_slprintf(p, last, ": %*s",
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

    if (!pwexec->ictx->instance->hostcall || rctx->fake_request) {
        if (rctx->pwm_log_dispatch_errors) {
            ngx_wasm_log_error(NGX_LOG_ERR, pwexec->log, 0, "%*s",
                               p - (u_char *) &errbuf, &errbuf);
        }

    } else {
        /* in-vm, executing a hostcall */
        ngx_wavm_instance_trap_printf(pwexec->ictx->instance, "%*s",
                                      p - (u_char *) &errbuf, &errbuf);
    }

    if (resume) {
        (void) invoke_on_http_dispatch_response(pwexec, call);
    }

    ngx_http_proxy_wasm_dispatch_destroy(call);

    pwexec->dispatch_call = NULL;
}


#if 0
static void
ngx_http_proxy_wasm_cleanup_fake_request(void *data)
{
    ngx_http_proxy_wasm_dispatch_t  *call = data;
    ngx_http_wasm_req_ctx_t         *rctx = call->rctx;

    dd("enter");

    ngx_http_proxy_wasm_dispatch_destroy(call);

    if (rctx->fake_request) {
        ngx_pfree(rctx->pool, rctx);
    }
}
#endif


ngx_http_proxy_wasm_dispatch_t *
ngx_http_proxy_wasm_dispatch(ngx_proxy_wasm_exec_t *pwexec,
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
    ngx_wasm_socket_tcp_t           *sock = NULL;
#if 0
    ngx_pool_cleanup_t              *cln;
#endif
    ngx_http_request_t              *r;
    ngx_http_wasm_req_ctx_t         *rctxp = NULL;
    ngx_http_proxy_wasm_dispatch_t  *call = NULL;
    ngx_proxy_wasm_ctx_t            *pwctx = pwexec->parent;
    ngx_proxy_wasm_dispatch_op_t    *dop = NULL;
    unsigned                         enable_ssl = 0;

    /* rctx or fake request */

    if (rctx == NULL) {
        if (ngx_http_wasm_create_fake_rctx(pwexec, &rctxp) != NGX_OK) {
            goto error;
        }

        r = rctxp->r;

    } else {
        r = rctx->r;
    }


    /* alloc */

    call = ngx_calloc(sizeof(ngx_http_proxy_wasm_dispatch_t),
                      r->connection->log);
    if (call == NULL) {
        return NULL;
    }

    call->rctx = rctx ? rctx : rctxp;
    call->ictx = pwexec->ictx;
    call->pwexec = pwexec;

    if (!pwexec->in_tick) {
        switch (pwexec->parent->step) {
        case NGX_PROXY_WASM_STEP_LOG:
        case NGX_PROXY_WASM_STEP_DONE:
            /* r->pool was released */
            call->error = NGX_HTTP_PROXY_WASM_DISPATCH_ERR_BAD_STEP;
            goto error;
        default:
            break;
        }
    }

    rctx = call->rctx;

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

    ngx_log_debug1(NGX_LOG_DEBUG_ALL, r->connection->log, 0,
                   "wasm new dispatch call to \"%V\"", &call->host);

    /* headers/trailers */

    if (ngx_proxy_wasm_pairs_unmarshal(pwexec, &call->headers, headers)
        != NGX_OK
        || ngx_proxy_wasm_pairs_unmarshal(pwexec, &call->trailers, trailers)
           != NGX_OK)
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

            if (ngx_str_eq(elt->key.data, elt->key.len, ":method", -1)) {
                call->method.len = elt->value.len;
                call->method.data = elt->value.data;

            } else if (ngx_str_eq(elt->key.data, elt->key.len, ":path", -1)) {
                call->path.len = elt->value.len;
                call->path.data = elt->value.data;

            } else if (ngx_str_eq(elt->key.data, elt->key.len,
                                  ":authority", -1))
            {
                call->authority.len = elt->value.len;
                call->authority.data = elt->value.data;

            } else if (ngx_str_eq(elt->key.data, elt->key.len, ":scheme", -1)) {
#if (NGX_SSL)
                if (ngx_str_eq(elt->value.data, elt->value.len, "https", -1)) {
                    enable_ssl = 1;
                    dd("tls enabled");

                } else if (ngx_str_eq(elt->value.data, elt->value.len,
                                      "http", -1))
                {
                    dd("tls disabled");

                } else {
                    ngx_wasm_log_error(NGX_LOG_WARN, r->connection->log, 0,
                                       "unknown scheme \"%V\"",
                                       &elt->key);
                }
#else
                ngx_wasm_log_error(NGX_LOG_WARN, r->connection->log, 0,
                                   "proxy_wasm dispatch scheme ignored: "
                                   "not built with tls support");
#endif

            } else {
                ngx_wasm_log_error(NGX_LOG_WASM_NYI, r->connection->log, 0,
                                   "NYI - dispatch_http_call header \"%V\"",
                                   &elt->key);
            }

            elt->hash = 0;
            continue;

        } else if ((elt->key.len == ngx_http_host_header_len
                    && ngx_strncasecmp(elt->key.data,
                                       (u_char *) ngx_http_host_header,
                                       ngx_http_host_header_len) == 0)
                   ||
                   (elt->key.len == ngx_http_connection_header_len
                    && ngx_strncasecmp(elt->key.data,
                                       (u_char *) ngx_http_connection_header,
                                       ngx_http_connection_header_len) == 0))
        {
            /**
             * Avoid duplicating these headers in the request header buffer
             * since they are already hard-coded and would produce invalid
             * requests.
             */
#if (NGX_DEBUG)
            elt->key.data[0] = ngx_toupper(elt->key.data[0]);
            ngx_log_debug1(NGX_LOG_DEBUG_WASM, r->connection->log, 0,
                           "proxy_wasm http dispatch cannot override the "
                           "\"%V\" header, skipping", &elt->key);
#endif
            elt->hash = 0;
            continue;
        }

        elt->hash = 1;
    }

    if (!call->method.len) {
        call->error = NGX_HTTP_PROXY_WASM_DISPATCH_ERR_BAD_METHOD;
        goto error;

    } else if (!call->path.len) {
        call->error = NGX_HTTP_PROXY_WASM_DISPATCH_ERR_BAD_PATH;
        goto error;
    }

    /* body */

    if (body && body->len) {
        call->req_body_len = body->len;
        call->req_body = ngx_wasm_chain_get_free_buf(r->connection->pool,
                                                     &rctx->free_bufs,
                                                     body->len, buf_tag,
                                                     rctx->sock_buffer_reuse);
        if (call->req_body == NULL) {
            goto error;
        }

        buf = call->req_body->buf;
        buf->last = ngx_copy(buf->last, body->data, body->len);
    }

    /* init */

    sock = &call->sock;

    ngx_wa_assert(rctx);

    if (ngx_wasm_socket_tcp_init(sock, &call->host, enable_ssl,
                                 &call->authority, &rctx->env)
        != NGX_OK)
    {
        dd("tcp init error");
        goto error;
    }

    sock->read_timeout = call->timeout;
    sock->send_timeout = call->timeout;
    sock->connect_timeout = call->timeout;

    call->http_reader.pool = r->connection->pool;  /* longer lifetime than call */
    call->http_reader.log = r->connection->log;
    call->http_reader.rctx = call->rctx;
    call->http_reader.sock = sock;

    if (ngx_wasm_http_reader_init(&call->http_reader) != NGX_OK) {
        goto error;
    }

    /* dispatch */

    dop = ngx_pcalloc(pwexec->pool, sizeof(ngx_proxy_wasm_dispatch_op_t));
    if (dop == NULL) {
        goto error;
    }

    dop->type = NGX_PROXY_WASM_DISPATCH_HTTP_CALL;
    dop->call.http = call;

    ev = ngx_calloc(sizeof(ngx_event_t), r->connection->log);
    if (ev == NULL) {
        goto error;
    }

    ev->handler = ngx_http_proxy_wasm_dispatch_handler;
    ev->data = dop;
    ev->log = r->connection->log;

    ngx_post_event(ev, &ngx_posted_events);

    call->ev = ev;

    ngx_queue_insert_head(&pwexec->dispatch_ops, &dop->q);

    ngx_proxy_wasm_ctx_set_next_action(pwctx, NGX_PROXY_WASM_ACTION_PAUSE);

    /* add cleanup */

#if 0
    cln = ngx_pool_cleanup_add(r->pool, 0);
    if (cln == NULL) {
        return NULL;
    }

    cln->handler = ngx_http_proxy_wasm_cleanup_fake_request;
    cln->data = call;
#endif

    return call;

error:

    if (call) {
        ngx_http_proxy_wasm_dispatch_err(call, 0);
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

    dd("enter");

    if (call->ev) {
        ngx_delete_posted_event(call->ev);
        ngx_free(call->ev);
        call->ev = NULL;
    }

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
    ngx_proxy_wasm_dispatch_op_t    *dop = ev->data;
    ngx_http_proxy_wasm_dispatch_t  *call = dop->call.http;
    ngx_http_wasm_req_ctx_t         *rctx = call->rctx;
    ngx_wasm_socket_tcp_t           *sock = &call->sock;

    ngx_free(ev);
    call->ev = NULL;

    sock->resume_handler = ngx_http_proxy_wasm_dispatch_resume_handler;
    sock->data = dop;

    rc = sock->resume_handler(sock);
    dd("sock->resume rc: %ld", rc);
    if (rc != NGX_AGAIN) {
        ngx_http_wasm_resume(rctx);
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
    ngx_wasm_socket_tcp_t    *sock;
    ngx_peer_connection_t    *pc;
    ngx_http_wasm_req_ctx_t  *rctx;
    ngx_http_request_t       *fake_r;
    ngx_http_request_t       *r;

    if (call->req_out) {
        return call->req_out;
    }

    if (!call->authority.len) {
        sock = &call->sock;
        pc = &sock->peer;

        switch (pc->sockaddr->sa_family) {
#if (NGX_HAVE_UNIX_DOMAIN)
        case AF_UNIX:
            call->authority.len = 9;
            call->authority.data = (u_char *) "localhost";
            break;
#endif
        default:
            call->authority.len = sock->host.len;
            call->authority.data = sock->host.data;
            break;
        }
    }

    rctx = call->rctx;
    fake_r = &call->fake_r;
    r = rctx->r;

    fake_r->signature = NGX_WASM_MODULE;
    fake_r->connection = rctx->connection;
    fake_r->pool = r->pool;
    fake_r->ctx = r->ctx;
    fake_r->loc_conf = r->loc_conf;

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

#if 0
        dd("%.*s: %.*s",
           (int) elt->key.len, elt->key.data,
           (int) elt->value.len, elt->value.data);
#endif

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

    fake_r->headers_in.content_length_n = call->req_body_len;

    /**
     * GET /dispatched HTTP/1.1
     * Host:
     * Connection:
     * Content-Length:
     */
    len += call->method.len + 1 + call->path.len + 1
           + sizeof(ngx_http_header_version11) - 1;

    len += sizeof(ngx_http_host_header) - 1 + sizeof(": ") - 1
           + call->authority.len + sizeof(CRLF) - 1;

    len += sizeof(ngx_http_connection_header) - 1 + sizeof(": close") - 1
           + sizeof(CRLF) - 1;

    if (fake_r->headers_in.content_length == NULL
        && fake_r->headers_in.content_length_n >= 0)
    {
        len += sizeof(ngx_http_cl_header) - 1 + sizeof(": ") - 1
               + NGX_OFF_T_LEN + sizeof(CRLF) - 1;
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

    len += sizeof(CRLF) - 1;

    /* headers buffer write */

    nl = ngx_wasm_chain_get_free_buf(r->connection->pool,
                                     &rctx->free_bufs,
                                     len, buf_tag,
                                     rctx->sock_buffer_reuse);
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

    b->last = ngx_cpymem(b->last, call->path.data, call->path.len);
    *b->last++ = ' ';

    b->last = ngx_cpymem(b->last, ngx_http_header_version11,
                         sizeof(ngx_http_header_version11) - 1);

    b->last = ngx_cpymem(b->last, ngx_http_host_header,
                         sizeof(ngx_http_host_header) - 1);
    b->last = ngx_cpymem(b->last, ": ", sizeof(": ") - 1);
    b->last = ngx_cpymem(b->last, call->authority.data, call->authority.len);
    *b->last++ = CR;
    *b->last++ = LF;

    b->last = ngx_cpymem(b->last, ngx_http_connection_header,
                         sizeof(ngx_http_connection_header) - 1);
    b->last = ngx_cpymem(b->last, ": close" CRLF, sizeof(": close" CRLF) - 1);

    if (fake_r->headers_in.content_length == NULL
        && fake_r->headers_in.content_length_n >= 0)
    {
        b->last = ngx_cpymem(b->last, ngx_http_cl_header,
                             sizeof(ngx_http_cl_header) - 1);
        b->last = ngx_sprintf(b->last, ": %O" CRLF,
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
    ngx_int_t                        rc = NGX_ERROR;
    ngx_chain_t                     *nl;
    ngx_wavm_instance_t             *instance;
    ngx_proxy_wasm_dispatch_op_t    *dop;
    ngx_http_proxy_wasm_dispatch_t  *call;
    ngx_http_wasm_req_ctx_t         *rctx;
    ngx_http_request_t              *r;
    ngx_proxy_wasm_exec_t           *pwexec;
    ngx_proxy_wasm_filter_t         *filter;

    dd("enter");

    dop = sock->data;

    ngx_wa_assert(dop->type == NGX_PROXY_WASM_DISPATCH_HTTP_CALL);

    call = dop->call.http;

    rctx = call->rctx;
    r = rctx->r;
    pwexec = call->pwexec;
    filter = pwexec->filter;

    ngx_wa_assert(&call->sock == sock);

    if (sock->err) {
        goto error;
    }

    switch (call->state) {

    case NGX_HTTP_PROXY_WASM_DISPATCH_START:

        ngx_log_debug0(NGX_LOG_DEBUG_WASM, sock->log, 0,
                       "proxy_wasm http dispatch connecting...");

        call->state = NGX_HTTP_PROXY_WASM_DISPATCH_CONNECTING;

        /* fallthrough */

    case NGX_HTTP_PROXY_WASM_DISPATCH_CONNECTING:

        rc = ngx_wasm_socket_tcp_connect(sock);

        dd("connect rc: %ld", rc);

        if (rc == NGX_ERROR) {
            goto error;
        }

        if (rc == NGX_AGAIN) {
            break;
        }

        ngx_wa_assert(rc == NGX_OK);

        call->state = NGX_HTTP_PROXY_WASM_DISPATCH_SENDING;

        /* fallthrough */

    case NGX_HTTP_PROXY_WASM_DISPATCH_SENDING:

        nl = ngx_http_proxy_wasm_dispatch_request(call);
        if (nl == NULL) {
            goto error;
        }

        ngx_log_debug0(NGX_LOG_DEBUG_WASM, sock->log, 0,
                       "proxy_wasm http dispatch sending request...");

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

        ngx_wa_assert(rc == NGX_OK);

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

        ngx_wa_assert(rc == NGX_OK);

        call->state = NGX_HTTP_PROXY_WASM_DISPATCH_RECEIVED;

        /* fallthrough */

    case NGX_HTTP_PROXY_WASM_DISPATCH_RECEIVED:

        ngx_wasm_socket_tcp_close(sock);

        instance = ngx_proxy_wasm_pwexec2instance(pwexec);

        if (instance->trapped) {
            pwexec->ecode = NGX_PROXY_WASM_ERR_INSTANCE_TRAPPED;

            ngx_proxy_wasm_log_error(NGX_LOG_ERR, pwexec->log, pwexec->ecode,
                                     "proxy_wasm \"%V\" filter (%l/%l) "
                                     "failed resuming after dispatch",
                                     filter->name, pwexec->index + 1,
                                     pwexec->parent->nfilters);

            rc = NGX_ABORT;
            goto error;
        }

        /* call has finished */
        ngx_queue_remove(&dop->q);

        if (invoke_on_http_dispatch_response(pwexec, call) != NGX_OK) {
            rc = NGX_ERROR;
            ngx_http_proxy_wasm_dispatch_err(call, 0);
            goto done;
        }

        ngx_http_proxy_wasm_dispatch_destroy(call);
        break;

    default:
        ngx_wasm_log_error(NGX_LOG_WASM_NYI, pwexec->log, 0,
                           "NYI - dispatch state: %d", call->state);
        rc = NGX_ERROR;
        goto error2;

    }  /* switch(call->state) */

    ngx_wa_assert(rc == NGX_AGAIN || rc == NGX_OK);
    goto done;

error:

    /* call has errored */
    ngx_queue_remove(&dop->q);

error2:

    if (rc == NGX_ABORT) {
        /* catch trap for tcp socket resume retval or an instance
         * that trapped before the response was received */
        rc = NGX_ERROR;
    }

    ngx_http_proxy_wasm_dispatch_err(call, 1);

done:

    if (ngx_proxy_wasm_dispatch_ops_total(pwexec)) {
        ngx_log_debug0(NGX_LOG_DEBUG_WASM, pwexec->log, 0,
                       "proxy_wasm more dispatch operations pending...");

        rc = NGX_AGAIN;
        ngx_wasm_yield(&rctx->env);
        ngx_proxy_wasm_ctx_set_next_action(pwexec->parent,
                                           NGX_PROXY_WASM_ACTION_PAUSE);

    } else {
        ngx_log_debug0(NGX_LOG_DEBUG_WASM, pwexec->log, 0,
                       "proxy_wasm last dispatch operation handled");

        ngx_wasm_continue(&rctx->env);
        ngx_proxy_wasm_ctx_set_next_action(pwexec->parent,
                                           NGX_PROXY_WASM_ACTION_CONTINUE);

        /* resume current step if unfinished */
        if (rc != NGX_ERROR) {
            rc = ngx_proxy_wasm_resume(pwexec->parent,
                                       pwexec->parent->phase,
                                       pwexec->parent->step);
        }
    }

    dd("exit rc: %ld", rc);

    return rc;
}
