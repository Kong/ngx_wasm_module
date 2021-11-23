#ifndef DDEBUG
#define DDEBUG 1
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
static char  ngx_http_proxy_wasm_host[] = "Host: ";
static char  ngx_http_proxy_wasm_connection[] = "Connection: close";
static char  ngx_http_proxy_wasm_content_length[] = "Content-Length: 0";


ngx_int_t
ngx_http_proxy_wasm_dispatch(ngx_http_proxy_wasm_dispatch_t *call)
{
    ngx_event_t            *ev;
    ngx_pool_cleanup_t     *cln;
    ngx_wasm_socket_tcp_t  *sock = &call->sock;
    ngx_http_request_t     *r = call->r;

    /* init */

    sock->pool = r->pool;
    sock->log = r->connection->log;
    sock->read_timeout = call->timeout;
    sock->send_timeout = call->timeout;
    sock->connect_timeout = call->timeout;

    if (ngx_wasm_socket_tcp_init(sock, &call->host) != NGX_OK) {
        return NGX_ERROR;
    }

    call->http_reader.pool = r->pool;
    call->http_reader.log = r->connection->log;
    call->http_reader.r = r;
    call->http_reader.rctx = call->rctx;

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


static ngx_chain_t *
ngx_http_proxy_wasm_dispatch_request(ngx_http_proxy_wasm_dispatch_t *call)
{
    size_t                    i, len;
    ngx_chain_t              *nl;
    ngx_buf_t                *b;
    ngx_table_elt_t          *elt, *elts;
    ngx_http_request_t       *r = call->r;
    ngx_http_wasm_req_ctx_t  *rctx = call->rctx;

    /* GET /dispatched HTTP/1.1 */

    len = call->method.len + 1 + call->uri.len + 1
          + sizeof(ngx_http_proxy_wasm_dispatch_version_11) - 1;

    /* Host: ... */

    len += sizeof(ngx_http_proxy_wasm_host) - 1 + call->authority.len
           + sizeof(CRLF) - 1;

    /* Connection */

    len += sizeof(ngx_http_proxy_wasm_connection) - 1 + sizeof(CRLF) - 1;

    /* Content-Length */

    len += sizeof(ngx_http_proxy_wasm_content_length) - 1 + sizeof(CRLF) - 1;

    /* headers */

    elts = (ngx_table_elt_t *) call->headers.elts;

    for (i = 0; i < call->headers.nelts; i++) {
        elt = &elts[i];

        if (elt->hash == 0) {
            continue;
        }

        len += elt->key.len + sizeof(": ") - 1
               + elt->value.len + sizeof(CRLF) - 1;
    }

    /* body */

    len += sizeof(CRLF) - 1 + sizeof(CRLF) - 1;

    /* get buffer */

    nl = ngx_wasm_chain_get_free_buf(r->connection->log, r->pool,
                                     &rctx->free_bufs, len, buf_tag);
    if (nl == NULL) {
        return NULL;
    }

    b = nl->buf;

    /* GET /dispatched HTTP/1.1 */

    b->last = ngx_copy(b->last, call->method.data, call->method.len);
    *b->last++ = ' ';

    b->last = ngx_copy(b->last, call->uri.data, call->uri.len);
    *b->last++ = ' ';

    b->last = ngx_cpymem(b->last, ngx_http_proxy_wasm_dispatch_version_11,
                         sizeof(ngx_http_proxy_wasm_dispatch_version_11) - 1);

    /* Host: ... */

    b->last = ngx_cpymem(b->last, ngx_http_proxy_wasm_host,
                         sizeof(ngx_http_proxy_wasm_host) - 1);

    b->last = ngx_cpymem(b->last, call->authority.data, call->authority.len);
    *b->last++ = CR;
    *b->last++ = LF;

    /* Connection */

    b->last = ngx_cpymem(b->last, ngx_http_proxy_wasm_connection,

                         sizeof(ngx_http_proxy_wasm_connection) - 1);
    *b->last++ = CR;
    *b->last++ = LF;

    /* Content-Length */

    b->last = ngx_cpymem(b->last, ngx_http_proxy_wasm_content_length,
                         sizeof(ngx_http_proxy_wasm_content_length) - 1);
    *b->last++ = CR;
    *b->last++ = LF;

    /* headers */

    elts = (ngx_table_elt_t *) call->headers.elts;

    for (i = 0; i < call->headers.nelts; i++) {
        elt = &elts[i];

        if (elt->hash == 0) {
            continue;
        }

        b->last = ngx_cpymem(b->last, elt->key.data, elt->key.len);
        *b->last++ = ':';
        *b->last++ = ' ';

        b->last = ngx_cpymem(b->last, elt->value.data, elt->value.len);
        *b->last++ = CR;
        *b->last++ = LF;
    }

    /* body */

    *b->last++ = CR;
    *b->last++ = LF;
    *b->last++ = CR;
    *b->last++ = LF;

    ngx_wasm_assert(len == (size_t) (b->last - b->start));

    return nl;
}


static void
ngx_http_proxy_wasm_dispatch_resume_handler(ngx_wasm_socket_tcp_t *sock)
{
    size_t                           i;
    ngx_int_t                        rc;
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

    if (sock->timedout) {
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

        call->state = NGX_HTTP_PROXY_WASM_DISPATCH_WRITE;

        /* fallthrough */

    case NGX_HTTP_PROXY_WASM_DISPATCH_WRITE:

        nl = ngx_http_proxy_wasm_dispatch_request(call);
        if (nl == NULL) {
            goto error;
        }

        ngx_log_debug0(NGX_LOG_DEBUG_ALL, sock->log, 0,
                       "proxy_wasm http dispatch sending request");

        /* fallthrough */

    case NGX_HTTP_PROXY_WASM_DISPATCH_WRITING:

#if (NGX_DEBUG)
        if (call->state != NGX_HTTP_PROXY_WASM_DISPATCH_WRITE) {
            ngx_log_debug0(NGX_LOG_DEBUG_ALL, sock->log, 0,
                           "proxy_wasm http dispatch resuming send");
        }
#endif

        call->state = NGX_HTTP_PROXY_WASM_DISPATCH_WRITING;

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

        call->state = NGX_HTTP_PROXY_WASM_DISPATCH_READ;

        /* fallthrough */

    case NGX_HTTP_PROXY_WASM_DISPATCH_READ:

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

        ngx_wasm_socket_tcp_close(sock);

        /* fallthrough */

    case NGX_HTTP_PROXY_WASM_DISPATCH_RECEIVED:

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
                       "(fctx->id: %d token_id: %d, n_headers: %d)",
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

    ngx_proxy_wasm_resume(fctx);
}
