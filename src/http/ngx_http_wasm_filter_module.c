#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_http_wasm.h>


static ngx_int_t ngx_http_wasm_filter_init(ngx_conf_t *cf);
static ngx_int_t ngx_http_wasm_header_filter_handler(ngx_http_request_t *r);
static ngx_int_t ngx_http_wasm_body_filter_handler(ngx_http_request_t *r,
    ngx_chain_t *in);


static ngx_http_module_t  ngx_http_wasm_module_ctx = {
    NULL,                                /* preconfiguration */
    ngx_http_wasm_filter_init,           /* postconfiguration */
    NULL,                                /* create main configuration */
    NULL,                                /* init main configuration */
    NULL,                                /* create server configuration */
    NULL,                                /* merge server configuration */
    NULL,                                /* create location configuration */
    NULL                                 /* merge location configuration */
};


ngx_module_t  ngx_http_wasm_filter_module = {
    NGX_MODULE_V1,
    &ngx_http_wasm_module_ctx,           /* module context */
    NULL,                                /* module directives */
    NGX_HTTP_MODULE,                     /* module type */
    NULL,                                /* init master */
    NULL,                                /* init module */
    NULL,                                /* init process */
    NULL,                                /* init thread */
    NULL,                                /* exit thread */
    NULL,                                /* exit process */
    NULL,                                /* exit master */
    NGX_MODULE_V1_PADDING
};


static ngx_http_output_header_filter_pt  ngx_http_next_header_filter;
static ngx_http_output_body_filter_pt  ngx_http_next_body_filter;


static void ngx_http_wasm_body_filter_resume(ngx_http_wasm_req_ctx_t *rctx,
    ngx_chain_t *in);
static ngx_int_t ngx_http_wasm_body_filter_buffer(
    ngx_http_wasm_req_ctx_t *rctx, ngx_chain_t *in);


static ngx_int_t
ngx_http_wasm_filter_init(ngx_conf_t *cf)
{
    ngx_http_next_header_filter = ngx_http_top_header_filter;
    ngx_http_top_header_filter = ngx_http_wasm_header_filter_handler;

    ngx_http_next_body_filter = ngx_http_top_body_filter;
    ngx_http_top_body_filter = ngx_http_wasm_body_filter_handler;

    return NGX_OK;
}


static ngx_int_t
ngx_http_wasm_header_filter_handler(ngx_http_request_t *r)
{
    ngx_int_t                  rc = NGX_ERROR;
    ngx_http_wasm_req_ctx_t   *rctx = NULL;

    dd("enter");

    rc = ngx_http_wasm_rctx(r, &rctx);
    if (rc == NGX_ERROR) {
        goto done;
    }

    if (rc == NGX_DECLINED
        || rctx->entered_header_filter)
    {
        rc = ngx_http_next_header_filter(r);
        goto done;
    }

    ngx_wasm_assert(rc == NGX_OK);

    rctx->entered_header_filter = 1;

    if (ngx_http_wasm_produce_resp_headers(rctx) != NGX_OK) {
        goto done;
    }

    rc = ngx_wasm_ops_resume(&rctx->opctx,
                             NGX_HTTP_WASM_HEADER_FILTER_PHASE);

    dd("ops resume rc: %ld", rc);

    if (r->err_status) {
        /* previous unhandled error before resuming header_filter */
        rc = ngx_http_next_header_filter(r);
        goto done;

    } else if (rc == NGX_ERROR || rc >= NGX_HTTP_SPECIAL_RESPONSE) {
        if (rc == NGX_ERROR) {
            rc = NGX_HTTP_INTERNAL_SERVER_ERROR;
            goto done;
        }

        if (rctx->resp_content_chosen) {
            goto done;
        }
    }
#if (NGX_DEBUG)
    else if (rc == NGX_AGAIN) {
        /**
         * Proxy-Wasm idiom for "local response in response headers",
         * log potential usage and ignore rc.
         */
        ngx_log_debug2(NGX_LOG_DEBUG_WASM, r->connection->log, 0,
                       "wasm \"header_filter\" ops resume rc: %d "
                       "(resp_chunk_override: %d)", rc,
                       rctx->resp_chunk_override);
    }
#endif

    rc = ngx_http_next_header_filter(r);

    rctx->resp_content_chosen = 1;  /* if not already set */

done:

    ngx_log_debug1(NGX_LOG_DEBUG_WASM, r->connection->log, 0,
                   "wasm \"header_filter\" phase rc: %d", rc);

    return rc;
}


static ngx_int_t
ngx_http_wasm_body_filter_handler(ngx_http_request_t *r, ngx_chain_t *in)
{
    ngx_int_t                 rc;
    ngx_http_wasm_req_ctx_t  *rctx = NULL, *mrctx = NULL;

    dd("enter");

    rc = ngx_http_wasm_rctx(r, &rctx);
    if (rc == NGX_ERROR) {
        goto done;
    }

    if (rc == NGX_DECLINED || !rctx->entered_header_filter) {
        if (r != r->main) {
            /* get main rctx */
            rc = ngx_http_wasm_rctx(r->main, &mrctx);
            if (rc == NGX_ERROR) {
                goto done;
            }
        }

        if (rc == NGX_DECLINED && rctx == NULL) {
            /* r == r->main or r->main has no rctx */
            rc = ngx_http_next_body_filter(r, in);
            goto done;
        }

        if (r != r->main && rctx == NULL) {
            /* subrequest with no rctx; merge main rctx for buffering */
            ngx_wasm_assert(mrctx);
            rctx = mrctx;
        }
    }

    if (rctx->resp_chunk_override) {
        rctx->resp_chunk_override = 0;

        in = rctx->resp_chunk;
    }

    rctx->entered_body_filter = 1;

    ngx_http_wasm_body_filter_resume(rctx, in);

    if (rctx->resp_buffering) {
        rc = ngx_http_wasm_body_filter_buffer(rctx, in);
        dd("ngx_http_wasm_body_filter_buffer rc: %ld", rc);
        switch (rc) {
        case NGX_ERROR:
            goto done;
        case NGX_OK:
            /* chunk in buffers */
            ngx_wasm_assert(rctx->resp_bufs);

            if (!rctx->resp_chunk_eof) {
                /* more to come; go again */
                rc = NGX_AGAIN;
                goto done;
            }

            dd("eof, resume");
            rctx->resp_buffering = 0;
            ngx_http_wasm_body_filter_resume(rctx, rctx->resp_bufs);
            break;
        case NGX_DONE:
            dd("buffers full, resume");
            rctx->resp_buffering = 0;
            ngx_http_wasm_body_filter_resume(rctx, rctx->resp_bufs);
            break;
        default:
            ngx_wasm_assert(0);
            break;
        }
    }

    ngx_wasm_chain_log_debug(r->connection->log, rctx->resp_chunk,
                             "rctx->resp_chunk");

    rc = ngx_http_next_body_filter(r, rctx->resp_chunk);
    dd("ngx_http_next_body_filter rc: %ld", rc);
    if (rc == NGX_ERROR) {
        goto done;
    }

    rctx->resp_chunk = NULL;

#ifdef NGX_WASM_RESPONSE_TRAILERS
    if (rctx->resp_chunk_eof && r->parent == NULL) {
        (void) ngx_wasm_ops_resume(&rctx->opctx,
                                   NGX_HTTP_WASM_TRAILER_FILTER_PHASE);
    }
#endif

done:

    ngx_wasm_assert(rc == NGX_OK || rc == NGX_AGAIN || rc == NGX_ERROR);

    ngx_log_debug1(NGX_LOG_DEBUG_WASM, r->connection->log, 0,
                   "wasm \"body_filter\" phase rc: %d", rc);

    if (rc == NGX_OK && rctx && rctx->resp_chunk_eof) {
        ngx_chain_update_chains(rctx->pool,
                                &rctx->free_bufs, &rctx->busy_bufs,
                                &rctx->resp_bufs, rctx->env.buf_tag);
    }

    return rc;
}


static void
ngx_http_wasm_body_filter_resume(ngx_http_wasm_req_ctx_t *rctx, ngx_chain_t *in)
{
    ngx_int_t     rc;
    ngx_chain_t  *cl;

    rctx->resp_chunk = in;
    rctx->resp_chunk_len = 0;

    for (cl = rctx->resp_chunk; cl; cl = cl->next) {
        rctx->resp_chunk_len += ngx_buf_size(cl->buf);

        if (cl->buf->last_buf) {
            rctx->resp_chunk_eof = 1;
            break;

        } else if (cl->buf->last_in_chain) {
            if (rctx->r != rctx->r->main) {
                rctx->resp_chunk_eof = 1;
            }

            break;
        }
    }

    if (!rctx->resp_buffering) {
        rc = ngx_wasm_ops_resume(&rctx->opctx,
                                 NGX_HTTP_WASM_BODY_FILTER_PHASE);
        if (rc == NGX_AGAIN) {
            ngx_wasm_assert(rctx->resp_bufs == NULL);
            rctx->resp_buffering = 1;
        }
    }
}


static ngx_int_t
ngx_http_wasm_body_filter_buffer(ngx_http_wasm_req_ctx_t *rctx, ngx_chain_t *in)
{
    off_t                      n, avail, copy;
    ngx_chain_t               *cl, *ll, *rl;
    ngx_http_request_t        *r = rctx->r;
    ngx_http_wasm_loc_conf_t  *loc;

    dd("enter");

    ngx_wasm_assert(rctx->resp_buffering);

    cl = rctx->resp_buf_last;
    loc = ngx_http_get_module_loc_conf(r, ngx_http_wasm_module);

    for (ll = in; ll; ll = ll->next) {

        n = ngx_buf_size(ll->buf);
        dd("n: %ld", n);

        if (n == 0) {
            if (ll->buf->last_in_chain || ll->buf->last_buf) {
                if (rctx->resp_bufs == NULL) {
                    rctx->resp_bufs = ll;

                } else {
                    rctx->resp_buf_last->next = ll;
                    rctx->resp_buf_last = ll;
                }
            }
        }

        while (n) {
            if (cl == NULL) {
                if (rctx->resp_bufs_count
                    >= (ngx_uint_t) loc->resp_body_buffers.num)
                {
                    if (rctx->resp_buf_last) {
                        for (rl = ll; rl; rl = rl->next) {
                            if (ngx_buf_size(rl->buf)) {
                                rctx->resp_buf_last->next = rl;
                                rctx->resp_buf_last = rl;
                            }
                        }
                    }

                    ngx_wasm_chain_log_debug(r->connection->log,
                                             rctx->resp_bufs,
                                             "response buffers: ");

                    return NGX_DONE;
                }

                cl = ngx_wasm_chain_get_free_buf(rctx->pool,
                                                 &rctx->free_bufs,
                                                 loc->resp_body_buffers.size,
                                                 rctx->env.buf_tag, 1);
                if (cl == NULL) {
                    return NGX_ERROR;
                }

                cl->buf->pos = cl->buf->start;
                cl->buf->last = cl->buf->pos;
                cl->buf->tag = rctx->env.buf_tag;
                cl->buf->memory = 1;
                cl->next = NULL;

                if (rctx->resp_bufs == NULL) {
                    rctx->resp_bufs = cl;

                } else if (rctx->resp_buf_last) {
                    rctx->resp_buf_last->next = cl;
                }

                rctx->resp_buf_last = cl;
                rctx->resp_bufs_count++;
            }

            avail = cl->buf->end - cl->buf->last;
            copy = ngx_min(n, avail);
            dd("avail: %ld, copy: %ld", avail, copy);
            if (copy == 0) {
                cl = NULL;
                continue;
            }

            ngx_memcpy(cl->buf->last, ll->buf->pos, copy);

            ll->buf->pos += copy;
            cl->buf->last += copy;

            cl->buf->flush = ll->buf->flush;
            cl->buf->last_buf = ll->buf->last_buf;
            cl->buf->last_in_chain = ll->buf->last_in_chain;

            dd("f: %d, l: %d, lic: %d", ll->buf->flush,
               ll->buf->last_buf, ll->buf->last_in_chain);

            if (copy < n) {
                /* more to copy, next buffer */
                ngx_wasm_assert(cl->buf->last == cl->buf->end);
                rctx->resp_buf_last = cl;
                cl = NULL;
            }

            n -= copy;
            dd("next n: %ld", n);
        }
    }

    ngx_wasm_chain_log_debug(r->connection->log, rctx->resp_bufs,
                             "response buffers: ");

    return NGX_OK;
}
