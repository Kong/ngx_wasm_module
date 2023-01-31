#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_http_wasm_util.h>
#include <ngx_http_proxy_wasm.h>
#include <ngx_http_proxy_wasm_dispatch.h>


static void ngx_http_wasm_close_fake_connection(ngx_connection_t *c);
static void ngx_http_wasm_close_fake_request(ngx_http_request_t *r);
static void ngx_http_wasm_free_fake_request(ngx_http_request_t *r);


ngx_str_t *
ngx_http_copy_escaped(ngx_str_t *dst, ngx_pool_t *pool,
    ngx_http_wasm_escape_kind kind)
{
    size_t   escape, len;
    u_char  *data;

    data = dst->data;
    len = dst->len;

    escape = ngx_http_wasm_escape(NULL, data, len, kind);
    if (escape > 0) {
        /* null-terminated for ngx_http_header_filter */
        dst->len = len + 2 * escape;
        dst->data = ngx_pnalloc(pool, dst->len + 1);
        if (dst->data == NULL) {
            return NULL;
        }

        (void) ngx_http_wasm_escape(dst->data, data, len, kind);

        dst->data[dst->len] = '\0';
    }

    return dst;
}


ngx_int_t
ngx_http_wasm_read_client_request_body(ngx_http_request_t *r,
    ngx_http_client_body_handler_pt post_handler)
{
    ngx_int_t   rc;

    r->request_body_in_single_buf = 1;

    rc = ngx_http_read_client_request_body(r, post_handler);

    ngx_wasm_assert(rc != NGX_AGAIN);

    if (rc < NGX_HTTP_SPECIAL_RESPONSE
        && rc != NGX_ERROR)
    {
        r->main->count--;
    }

    return rc;
}


ngx_int_t
ngx_http_wasm_send_chain_link(ngx_http_request_t *r, ngx_chain_t *in)
{
    ngx_int_t                 rc;
    ngx_http_wasm_req_ctx_t  *rctx;

    if (ngx_http_wasm_rctx(r, &rctx) != NGX_OK) {
        return NGX_ERROR;
    }

    if (!r->headers_out.status) {
        r->headers_out.status = NGX_HTTP_OK;
    }

    rc = ngx_http_send_header(r);
    if (rc == NGX_ERROR) {
        return NGX_ERROR;
    }

    if (rc > NGX_OK || r->header_only) {
        goto done;
    }

    ngx_wasm_assert(r->header_sent);
    ngx_wasm_assert(rc == NGX_OK
                    || rc == NGX_AGAIN
                    || rc == NGX_DONE
                    || rc == NGX_DECLINED);

    if (in == NULL) {
        rc = ngx_http_send_special(r, NGX_HTTP_LAST);
        if (rc == NGX_ERROR) {
            return NGX_ERROR;

        } else if (rc < NGX_HTTP_SPECIAL_RESPONSE) {
            /* special response >= 300 */
            rc = NGX_OK;
        }

        goto done;
    }

    rc = ngx_http_output_filter(r, in);
    if (rc == NGX_ERROR) {
        return NGX_ERROR;
    }

    if (!rctx->resp_content_sent) {
        r->main->count++;
    }

    rctx->resp_content_sent = 1;

done:

    dd("rc: %ld", rc);

    ngx_wasm_assert(rc == NGX_OK || rc == NGX_DONE);

    return rc;
}


ngx_int_t
ngx_http_wasm_produce_resp_headers(ngx_http_wasm_req_ctx_t *rctx)
{
    ngx_int_t                  rc;
    static ngx_str_t           date = ngx_string("Date");
    ngx_str_t                  date_val;
    static ngx_str_t           last_modified = ngx_string("Last-Modified");
    ngx_str_t                  last_mod_val;
    static ngx_str_t           server = ngx_string("Server");
    ngx_str_t                 *server_val = NULL;
    static ngx_str_t           server_full = ngx_string(NGINX_VER);
    static ngx_str_t           server_build = ngx_string(NGINX_VER_BUILD);
    static ngx_str_t           server_default = ngx_string("nginx");
    ngx_http_request_t        *r = rctx->r;
    ngx_http_core_loc_conf_t  *clcf;

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "wasm producing default response headers");

    if (r->headers_out.server == NULL) {
        /* Server */

        clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);

        if (clcf->server_tokens == NGX_HTTP_SERVER_TOKENS_ON) {
            server_val = &server_full;

        } else if (clcf->server_tokens == NGX_HTTP_SERVER_TOKENS_BUILD) {
            server_val = &server_build;

        } else {
            server_val = &server_default;
        }

        if (server_val) {
            if (ngx_http_wasm_set_resp_header(r, &server, server_val,
                                              NGX_HTTP_WASM_HEADERS_SET)
                != NGX_OK)
            {
                return NGX_ERROR;
            }
        }
    }

    if (r->headers_out.date == NULL) {
        /* Date */

        date_val.len = ngx_cached_http_time.len;
        date_val.data = ngx_cached_http_time.data;

        rc = ngx_http_wasm_set_resp_header(r, &date, &date_val, 0);
        if (rc != NGX_OK) {
            return NGX_ERROR;
        }
    }

    if (r->headers_out.last_modified == NULL
        && r->headers_out.last_modified_time != -1)
    {
        /* Last-Modified */

        last_mod_val.len = sizeof("Mon, 28 Sep 1970 06:00:00 GMT") - 1;
        last_mod_val.data = ngx_pnalloc(r->pool, last_mod_val.len);
        if (last_mod_val.data == NULL) {
            return NGX_ERROR;
        }

        ngx_http_time(last_mod_val.data, r->headers_out.last_modified_time);

        rc = ngx_http_wasm_set_resp_header(r, &last_modified, &last_mod_val, 0);
        if (rc != NGX_OK) {
            return NGX_ERROR;
        }

        /* skip in ngx_http_header_filter, may affect stock logging */
        r->headers_out.last_modified_time = -1;
    }

    /* TODO: Location */

    return NGX_OK;
}


static ngx_inline ngx_http_request_body_t *
get_request_body(ngx_http_request_t *r)
{
    ngx_http_request_body_t  *rb;

    rb = r->request_body;

    if (rb == NULL) {
        rb = ngx_pcalloc(r->pool, sizeof(ngx_http_request_body_t));
        if (rb == NULL) {
            return NULL;
        }

        /*
         * set by ngx_pcalloc():
         *
         *     rb->bufs = NULL;
         *     rb->buf = NULL;
         *     rb->free = NULL;
         *     rb->busy = NULL;
         *     rb->chunked = NULL;
         *     rb->post_handler = NULL;
         */

        rb->rest = -1;

        r->request_body = rb;
    }

    return rb;
}


ngx_int_t
ngx_http_wasm_set_req_body(ngx_http_wasm_req_ctx_t *rctx, ngx_str_t *body,
    size_t at, size_t max)
{
    ngx_http_request_t       *r = rctx->r;
    ngx_http_request_body_t  *rb;

    if (rctx->entered_header_filter) {
        return NGX_ABORT;
    }

    rb = get_request_body(r);
    if (rb == NULL) {
        return NGX_ERROR;
    }

    body->len = ngx_min(body->len, max);

    if (ngx_wasm_chain_append(r->connection->pool, &rb->bufs, at, body,
                              &rctx->free_bufs, buf_tag, 0)
        != NGX_OK)
    {
        return NGX_ERROR;
    }

    r->headers_in.content_length_n = ngx_wasm_chain_len(rb->bufs, NULL);

    return NGX_OK;
}


ngx_int_t
ngx_http_wasm_prepend_req_body(ngx_http_wasm_req_ctx_t *rctx, ngx_str_t *body)
{
    ngx_http_request_t       *r = rctx->r;
    ngx_http_request_body_t  *rb;

    if (rctx->entered_header_filter) {
        return NGX_ABORT;
    }

    rb = get_request_body(r);
    if (rb == NULL) {
        return NGX_ERROR;
    }

    if (ngx_wasm_chain_prepend(r->connection->pool, &rb->bufs, body,
                               &rctx->free_bufs, buf_tag)
        != NGX_OK)
    {
        return NGX_ERROR;
    }

    r->headers_in.content_length_n = ngx_wasm_chain_len(rb->bufs, NULL);

    return NGX_OK;
}


ngx_int_t
ngx_http_wasm_set_resp_body(ngx_http_wasm_req_ctx_t *rctx, ngx_str_t *body,
    size_t at, size_t max)
{
    ngx_http_request_t  *r = rctx->r;

    if (rctx->resp_chunk == NULL) {
        return NGX_ABORT;
    }

    if (r->header_sent && !r->chunked) {
        ngx_wasm_log_error(NGX_LOG_WARN, r->connection->log, 0,
                           "overriding response body chunk while "
                           "Content-Length header already sent");
    }

    body->len = ngx_min(body->len, max);

    if (ngx_wasm_chain_append(r->connection->pool, &rctx->resp_chunk, at, body,
                              &rctx->free_bufs, buf_tag, 0)
        != NGX_OK)
    {
        return NGX_ERROR;
    }

    rctx->resp_chunk_len = ngx_wasm_chain_len(rctx->resp_chunk,
                                              &rctx->resp_chunk_eof);

    if (!rctx->resp_chunk_len) {
        /* discard chunk */
        rctx->resp_chunk = NULL;
    }

    return NGX_OK;
}


ngx_int_t
ngx_http_wasm_prepend_resp_body(ngx_http_wasm_req_ctx_t *rctx, ngx_str_t *body)
{
    ngx_http_request_t  *r = rctx->r;

    if (rctx->resp_chunk == NULL) {
        return NGX_ABORT;
    }

    if (r->header_sent && !r->chunked) {
        ngx_wasm_log_error(NGX_LOG_WARN, r->connection->log, 0,
                           "overriding response body chunk while "
                           "Content-Length header already sent");
    }

    if (ngx_wasm_chain_prepend(r->connection->pool, &rctx->resp_chunk, body,
                               &rctx->free_bufs, buf_tag)
        != NGX_OK)
    {
        return NGX_ERROR;
    }

    rctx->resp_chunk_len = ngx_wasm_chain_len(rctx->resp_chunk,
                                              &rctx->resp_chunk_eof);

    if (!rctx->resp_chunk_len) {
        /* discard chunk */
        rctx->resp_chunk = NULL;
    }

    return NGX_OK;
}


ngx_int_t
ngx_http_wasm_ops_add_filter(ngx_wasm_ops_plan_t *plan,
    ngx_str_t *name, ngx_str_t *config, ngx_uint_t *isolation,
    ngx_proxy_wasm_store_t *store, ngx_wavm_t *vm)
{
    ngx_int_t                 rc = NGX_ERROR;
    ngx_wasm_op_t            *op;
    ngx_proxy_wasm_filter_t  *filter;

    filter = ngx_pcalloc(plan->pool, sizeof(ngx_proxy_wasm_filter_t));
    if (filter == NULL) {
        goto error;
    }

    filter->pool = plan->pool;
    filter->log = vm->log;
    filter->store = store;

    if (config) {
        filter->config.len = config->len;
        filter->config.data = ngx_pstrdup(filter->pool, config);
        if (filter->config.data == NULL) {
            goto error;
        }
    }

    /* filter init */

    filter->isolation = isolation;
    filter->max_pairs = NGX_HTTP_WASM_MAX_REQ_HEADERS;
    filter->subsystem = &ngx_http_proxy_wasm;

    filter->module = ngx_wavm_module_lookup(vm, name);
    if (filter->module == NULL) {
        rc = NGX_ABORT;
        goto error;
    }

    /* op */

    op = ngx_pcalloc(plan->pool, sizeof(ngx_wasm_op_t));
    if (op == NULL) {
        goto error;
    }

    op->code = NGX_WASM_OP_PROXY_WASM;
    op->module = filter->module;
    op->host = &ngx_proxy_wasm_host;
    op->on_phases = (1 << NGX_HTTP_REWRITE_PHASE)
                    | (1 << NGX_HTTP_ACCESS_PHASE)
                    | (1 << NGX_HTTP_CONTENT_PHASE)
                    | (1 << NGX_HTTP_WASM_HEADER_FILTER_PHASE)
                    | (1 << NGX_HTTP_WASM_BODY_FILTER_PHASE)
                    | (1 << NGX_HTTP_WASM_TRAILER_FILTER_PHASE)
                    | (1 << NGX_HTTP_LOG_PHASE)
                    | (1 << NGX_WASM_DONE_PHASE);

    op->conf.proxy_wasm.filter = filter;

    if (ngx_wasm_ops_plan_add(plan, &op, 1) != NGX_OK) {
        goto error;
    }

    rc = NGX_OK;

    goto done;

error:

    if (filter) {
        if (filter->config.data) {
            ngx_pfree(filter->pool, filter->config.data);
        }

        ngx_pfree(filter->pool, filter);
    }

done:

    return rc;
}


ngx_connection_t *
ngx_http_wasm_create_fake_connection(ngx_pool_t *pool)
{
#if 0
    ngx_log_t         *log;
#endif
    ngx_connection_t  *c = NULL;
    ngx_connection_t  *saved_c = NULL;

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, pool->log, 0,
                   "wasm creating fake connection...");

    /* temporarily use a valid fd (0) for ngx_get_connection */
    if (ngx_cycle->files) {
        saved_c = ngx_cycle->files[0];
    }

    c = ngx_get_connection(0, ngx_cycle->log);

    if (ngx_cycle->files) {
        ngx_cycle->files[0] = saved_c;
    }

    if (c == NULL) {
        goto failed;
    }

    c->fd = NGX_WASM_BAD_FD;
    c->number = ngx_atomic_fetch_add(ngx_connection_counter, 1);

#if 0
    if (pool) {
        c->pool = pool;

    } else {
#endif
        c->pool = ngx_create_pool(128, c->log);
        if (c->pool == NULL) {
            goto failed;
        }
#if 0
    }
#endif

#if 0
    log = ngx_pcalloc(c->pool, sizeof(ngx_log_t));
    if (log == NULL) {
        goto failed;
    }

    c->log = log;
    c->log->data = c;
    c->log->connection = c->number;
    c->log->action = NULL;
    c->log->data = NULL;
    c->log_error = NGX_ERROR_INFO;
#else
    c->log = ngx_cycle->log;
#endif

    c->error = 1;

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, c->log, 0,
                   "wasm new fake connection: %p", c);

    return c;

failed:

    ngx_wasm_log_error(NGX_LOG_ERR, pool->log, 0,
                       "failed creating fake connection");

    if (c) {
        ngx_http_wasm_close_fake_connection(c);
    }

    return NULL;
}


ngx_http_request_t *
ngx_http_wasm_create_fake_request(ngx_connection_t *c)
{
    ngx_http_request_t  *r;

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, c->log, 0,
                   "wasm creating fake request... (c: %p)", c);

    r = ngx_pcalloc(c->pool, sizeof(ngx_http_request_t));
    if (r == NULL) {
        return NULL;
    }

    c->requests++;

    r->pool = c->pool;

#if 0
    hc = ngx_pcalloc(c->pool, sizeof(ngx_http_connection_t));
    if (hc == NULL) {
        goto failed;
    }

    r->header_in = c->buffer;
    r->header_end = c->buffer->start;

    if (ngx_list_init(&r->headers_out.headers, r->pool, 0,
                      sizeof(ngx_table_elt_t))
        != NGX_OK)
    {
        goto failed;
    }

    if (ngx_list_init(&r->headers_in.headers, r->pool, 0,
                      sizeof(ngx_table_elt_t))
        != NGX_OK)
    {
        goto failed;
    }
#endif

    r->ctx = ngx_pcalloc(r->pool, sizeof(void *) * ngx_http_max_module);
    if (r->ctx == NULL) {
        return NULL;
    }

#if 0
    cmcf = ngx_http_get_module_main_conf(r, ngx_http_core_module);

    r->variables = ngx_pcalloc(r->pool, cmcf->variables.nelts
                               * sizeof(ngx_http_variable_value_t));
    if (r->variables == NULL) {
        goto failed;
    }
#endif

    r->connection = c;

    r->headers_in.content_length_n = 0;
    c->data = r;
#if 0
    hc->request = r;
    r->http_connection = hc;
#endif
    r->signature = NGX_HTTP_MODULE;
    r->main = r;
    r->count = 1;

    r->method = NGX_HTTP_UNKNOWN;

    r->headers_in.keep_alive_n = -1;
    r->uri_changes = NGX_HTTP_MAX_URI_CHANGES + 1;
    r->subrequests = NGX_HTTP_MAX_SUBREQUESTS + 1;

    r->http_state = NGX_HTTP_PROCESS_REQUEST_STATE;
    r->discard_body = 1;

    ngx_log_debug2(NGX_LOG_DEBUG_HTTP, c->log, 0,
                   "wasm created fake request (c: %p, r: %p)", c, r);

    return r;
}


void
ngx_http_wasm_finalize_fake_request(ngx_http_request_t *r, ngx_int_t rc)
{
    ngx_connection_t        *c;
#if (0 && NGX_HTTP_SSL)
    ngx_ssl_conn_t          *ssl_conn;
    ngx_http_lua_ssl_ctx_t  *cctx;
#endif

    c = r->connection;

    ngx_log_debug3(NGX_LOG_DEBUG_HTTP, c->log, 0,
                   "http wasm finalize fake request: %d, a:%d, c:%d",
                   rc, r == c->data, r->main->count);

    if (rc == NGX_DONE) {
        ngx_http_wasm_close_fake_request(r);
        return;
    }

    if (rc == NGX_ERROR || rc >= NGX_HTTP_SPECIAL_RESPONSE) {
#if 0
#if (NGX_HTTP_SSL)
        if (r->connection->ssl) {
            ssl_conn = r->connection->ssl->connection;
            if (ssl_conn) {
                c = ngx_ssl_get_connection(ssl_conn);

                if (c && c->ssl) {
                    cctx = ngx_http_lua_ssl_get_ctx(c->ssl->connection);
                    if (cctx != NULL) {
                        cctx->exit_code = 0;
                    }
                }
            }
        }
#endif
#endif
        ngx_http_wasm_close_fake_request(r);
        return;
    }

    if (c->read->timer_set) {
        ngx_del_timer(c->read);
    }

    if (c->write->timer_set) {
        c->write->delayed = 0;
        ngx_del_timer(c->write);
    }

    ngx_http_wasm_close_fake_request(r);
}


static void
ngx_http_wasm_close_fake_connection(ngx_connection_t *c)
{
    ngx_pool_t        *pool;
    ngx_connection_t  *saved_c = NULL;

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, c->log, 0,
                   "wasm closing fake connection: %p", c);

    c->destroyed = 1;

    pool = c->pool;

    if (c->read->timer_set) {
        ngx_del_timer(c->read);
    }

    if (c->write->timer_set) {
        ngx_del_timer(c->write);
    }

    c->read->closed = 1;
    c->write->closed = 1;

    /* temporarily use a valid fd (0) for ngx_free_connection */

    c->fd = 0;

    if (ngx_cycle->files) {
        saved_c = ngx_cycle->files[0];
    }

    ngx_free_connection(c);

    c->fd = NGX_WASM_BAD_FD;

    if (ngx_cycle->files) {
        ngx_cycle->files[0] = saved_c;
    }

    if (pool) {
        ngx_destroy_pool(pool);
    }
}


static void
ngx_http_wasm_close_fake_request(ngx_http_request_t *r)
{
    ngx_connection_t  *c;

    r = r->main;
    c = r->connection;

    ngx_log_debug2(NGX_LOG_DEBUG_HTTP, c->log, 0,
                   "wasm closing fake request (r: %p, count: %d)",
                   r, r->count);

    if (r->count == 0) {
        ngx_log_error(NGX_LOG_ALERT, c->log, 0,
                      "wasm fake request count is zero");
    }

    r->count--;

    if (r->count) {
        return;
    }

    ngx_http_wasm_free_fake_request(r);
    ngx_http_wasm_close_fake_connection(c);
}


static void
ngx_http_wasm_free_fake_request(ngx_http_request_t *r)
{
    ngx_log_t           *log;
    ngx_http_cleanup_t  *cln;

    log = r->connection->log;

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, log, 0,
                   "wasm freeing fake request (r: %p)", r);

    if (r->pool == NULL) {
        ngx_log_error(NGX_LOG_ALERT, log, 0,
                      "wasm fake request already freed");
        return;
    }

    cln = r->cleanup;
    r->cleanup = NULL;

    while (cln) {
        if (cln->handler) {
            cln->handler(cln->data);
        }

        cln = cln->next;
    }

    r->request_line.len = 0;
    r->connection->destroyed = 1;
}
