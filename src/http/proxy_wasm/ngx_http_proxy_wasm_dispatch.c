#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_http_proxy_wasm_dispatch.h>


static void ngx_http_proxy_wasm_dispatch_cleanup(void *data);
static void ngx_http_proxy_wasm_dispatch_handler(ngx_event_t *ev);
static ngx_chain_t *ngx_http_proxy_wasm_dispatch_request(
    ngx_http_proxy_wasm_dispatch_t *call);
static void ngx_http_proxy_wasm_dispatch_resume_handler(
    ngx_wasm_socket_tcp_t *sock);


static char  ngx_http_proxy_wasm_dispatch_version_11[] = "HTTP/1.1" CRLF;
static char  ngx_http_proxy_wasm_connection[] = "Connection: close" CRLF;
static char  ngx_http_proxy_wasm_host[] = "Host: ";


ngx_int_t
ngx_http_proxy_wasm_dispatch(ngx_http_proxy_wasm_dispatch_t *call)
{
    ngx_event_t            *ev;
    ngx_pool_cleanup_t     *cln;
    ngx_wasm_socket_tcp_t  *sock = &call->sock;
    ngx_http_request_t     *r = call->r;

    /* init */

    if (ngx_wasm_socket_tcp_init(sock, &call->host, r->pool, r->connection->log)
        != NGX_OK)
    {
        return NGX_ERROR;
    }

    sock->read_timeout = call->timeout;
    sock->send_timeout = call->timeout;
    sock->connect_timeout = call->timeout;

    call->http_reader.pool = r->pool;
    call->http_reader.log = r->connection->log;
    call->http_reader.r = r;
    call->http_reader.rctx = call->rctx;
    call->http_reader.sock = sock;

    /* cleanup */

    cln = ngx_pool_cleanup_add(r->pool, 0);
    if (cln == NULL) {
        return NGX_ERROR;
    }

    cln->handler = ngx_http_proxy_wasm_dispatch_cleanup;
    cln->data = call;

    /* dispatch */

    ev = ngx_calloc(sizeof(ngx_event_t), r->connection->log);
    if (ev == NULL) {
        return NGX_ERROR;
    }

    ev->handler = ngx_http_proxy_wasm_dispatch_handler;
    ev->data = call;
    ev->log = r->connection->log;

    ngx_post_event(ev, &ngx_posted_events);

    return NGX_OK;
}


static void
ngx_http_proxy_wasm_dispatch_cleanup(void *data)
{
    ngx_http_proxy_wasm_dispatch_t  *call = data;
    ngx_wasm_socket_tcp_t           *sock = &call->sock;

    ngx_wasm_socket_tcp_destroy(sock);

    ngx_free(call);
}


static void
ngx_http_proxy_wasm_dispatch_handler(ngx_event_t *ev)
{
    ngx_http_proxy_wasm_dispatch_t  *call = ev->data;
    ngx_wasm_socket_tcp_t           *sock = &call->sock;

    ngx_free(ev);

    sock->resume = ngx_http_proxy_wasm_dispatch_resume_handler;
    sock->data = call;

    sock->resume(sock);
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
    ngx_http_request_t       *r = call->r;
    ngx_http_request_t       *fake_r = &call->fake_r;
    ngx_http_wasm_req_ctx_t  *rctx = call->rctx;

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

    fake_r->headers_in.content_length_n = call->body_len;

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

    nl = ngx_wasm_chain_get_free_buf(r->pool, &rctx->free_bufs, len,
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

    if (call->body_len) {
        nl->next = call->body;
    }

    return nl;
}


static void
ngx_http_proxy_wasm_dispatch_resume_handler(ngx_wasm_socket_tcp_t *sock)
{
    size_t                           i;
    ngx_int_t                        rc = NGX_ERROR;
    ngx_uint_t                       n_headers;
    ngx_chain_t                     *nl;
    ngx_list_part_t                 *part;
    ngx_table_elt_t                 *header;
    ngx_wavm_instance_t             *instance;
    ngx_http_proxy_wasm_dispatch_t  *call = sock->data;
    ngx_http_wasm_req_ctx_t         *rctx = call->rctx;
    ngx_proxy_wasm_filter_ctx_t     *fctx = call->fctx;
    ngx_http_request_t              *r = call->r;
    ngx_proxy_wasm_filter_t         *filter = fctx->filter;

    ngx_wasm_assert(&call->sock == sock);

    if (sock->err) {
        goto error;
    }

    switch (call->state) {

    case NGX_HTTP_PROXY_WASM_DISPATCH_START:

        ngx_log_debug0(NGX_LOG_DEBUG_ALL, sock->log, 0,
                       "proxy_wasm http dispatch connecting");

        rc = ngx_wasm_socket_tcp_connect(sock, rctx);
        if (rc == NGX_ERROR) {
            goto error;
        }

        if (rc == NGX_AGAIN) {
            break;
        }

        ngx_wasm_assert(rc == NGX_OK);

        call->state = NGX_HTTP_PROXY_WASM_DISPATCH_CONNECTED;

        /* fallthrough */

    case NGX_HTTP_PROXY_WASM_DISPATCH_CONNECTED:

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

        ngx_wasm_assert(rc == NGX_OK);

        ngx_chain_update_chains(r->pool, &rctx->free_bufs, &rctx->busy_bufs,
                                &nl, buf_tag);

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
        header = part->elts;

        for (i = 0, n_headers = 0; /* void */; i++, n_headers++) {
            if (i >= part->nelts) {
                if (part->next == NULL) {
                    break;
                }

                part = part->next;
                header = part->elts;
                i = 0;
            }

            if (header[i].hash == 0) {
                continue;
            }
        }

        ngx_log_debug3(NGX_LOG_DEBUG_ALL, sock->log, 0,
                       "proxy_wasm http dispatch response received "
                       "(fctx->id: %d, token_id: %d, n_headers: %d)",
                       fctx->id, call->callout_id, n_headers);

        instance = ngx_proxy_wasm_fctx2instance(fctx);

        fctx->data = call;

        rc = ngx_wavm_instance_call_funcref(instance,
                                            filter->proxy_on_http_call_response,
                                            NULL, fctx->id, call->callout_id,
                                            n_headers, 0, 0);

        fctx->data = NULL;

        if (rc == NGX_ABORT) {
            fctx->ecode = NGX_PROXY_WASM_ERR_INSTANCE_TRAPPED;
        }

        break;

    default:
        ngx_wasm_assert(0);
        break;

    }

    return;

error:

    if (sock->errlen) {
        ngx_wasm_log_error(NGX_LOG_ERR, r->connection->log, 0,
                           "dispatch failed (%*s)",
                           (int) sock->errlen, sock->err);
    }

    ngx_wasm_socket_tcp_close(sock);

    if (rc != NGX_ABORT) {
        /* background error */
        fctx->ecode = NGX_PROXY_WASM_ERR_DISPATCH_FAILED;
    }

    ngx_proxy_wasm_resume_main(fctx);
}
