#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_http_wasm_upstream.h>

#include "ngx_wasm_acl.h"

static ngx_str_t   https = ngx_string("https://");
static ngx_str_t   http = ngx_string("http://");


char *
ngx_http_wasm_upstream_select_directive(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_str_t                     *values, *value;
    ngx_int_t                      n;
    ngx_wavm_t                    *vm;
    ngx_http_wasm_srv_conf_t      *wscf = conf;
    ngx_http_upstream_srv_conf_t  *uscf;

    if (cf->args->nelts > 1) {
        values = cf->args->elts;
        value = &values[1];
        if (ngx_strncmp("max_tries=", value->data, 10) != 0) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "invalid parameter \"%V\"", value);
            return NGX_CONF_ERROR;
        }

        n = ngx_atoi(value->data + 10, value->len - 10);
        if (n == NGX_ERROR) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "invalid max_tries value in \"%V\"", value);
            return NGX_CONF_ERROR;
        }

        wscf->upstream_max_tries = n;
    }

    uscf = ngx_http_conf_get_module_srv_conf(cf, ngx_http_upstream_module);

    vm = ngx_wasm_main_vm(cf->cycle);
    if (vm == NULL) {
        return NGX_WASM_CONF_ERR_NO_WASM;
    }

    if (wscf->original_init_upstream) {
        return "is duplicate";
    }

    if (uscf->peer.init_upstream) {
        ngx_conf_log_error(NGX_LOG_WARN, cf, 0,
                           "load balancing method redefined");
    }

    wscf->original_init_upstream = uscf->peer.init_upstream
                                   ? uscf->peer.init_upstream
                                   : ngx_http_upstream_init_round_robin;

    uscf->peer.init_upstream = ngx_http_wasm_upstream_init;

    return NGX_CONF_OK;
}


ngx_int_t
ngx_http_wasm_upstream_init(ngx_conf_t *cf, ngx_http_upstream_srv_conf_t *us)
{
    ngx_http_wasm_srv_conf_t  *wscf;

    wscf = ngx_http_conf_upstream_srv_conf(us, ngx_http_wasm_module);

    if (wscf->original_init_upstream(cf, us) != NGX_OK) {
        return NGX_ERROR;
    }

    wscf->original_init_peer = us->peer.init;
    us->peer.init = ngx_http_wasm_upstream_init_peer;

    return NGX_OK;
}


ngx_int_t
ngx_http_wasm_upstream_init_peer(ngx_http_request_t *r,
    ngx_http_upstream_srv_conf_t *us)
{
    ngx_int_t                            rc;
    ngx_http_upstream_t                 *u;
    ngx_http_wasm_req_ctx_t             *rctx;
    ngx_http_wasm_srv_conf_t            *wscf;
    ngx_http_wasm_upstream_peer_data_t  *up;

    wscf = ngx_http_conf_upstream_srv_conf(us, ngx_http_wasm_module);

    up = ngx_pcalloc(r->pool, sizeof(ngx_http_wasm_upstream_peer_data_t));
    if (up == NULL) {
        return NGX_ERROR;
    }

    if (wscf->original_init_peer(r, us) != NGX_OK) {
        return NGX_ERROR;
    }

    rc = ngx_http_wasm_rctx(r, &rctx);
    if (rc == NGX_DECLINED) {
        return NGX_OK;
    } else if (rc != NGX_OK) {
        return NGX_ERROR;
    }

    rctx->upstream_peer = up;
    u = r->upstream;
    u->peer.tries = u->conf->next_upstream_tries
                    ? ngx_min(u->conf->next_upstream_tries,
                              wscf->upstream_max_tries)
                    : wscf->upstream_max_tries;

    up->data = u->peer.data;
    up->original_get_peer = u->peer.get;
    up->original_free_peer = u->peer.free;
#if (NGX_HTTP_SSL)
    up->original_set_session = u->peer.set_session;
    up->original_save_session = u->peer.save_session;
#endif

    up->request = r;

    u->peer.data = up;
    u->peer.get = ngx_http_wasm_upstream_get_peer;
    u->peer.free = ngx_http_wasm_upstream_free_peer;
    u->peer.notify = ngx_http_wasm_upstream_notify_peer;
    u->peer.test_next = ngx_http_wasm_upstream_test_next;
#if (NGX_HTTP_SSL)
    u->peer.set_session = ngx_http_wasm_upstream_set_session;
    u->peer.save_session = ngx_http_wasm_upstream_save_session;
#endif

    return NGX_OK;
}


#if (NGX_HTTP_SSL)
ngx_int_t
ngx_http_wasm_upstream_set_session(ngx_peer_connection_t *pc, void *data)
{
    ngx_http_wasm_upstream_peer_data_t  *up = data;

    if (up->sockaddr && up->socklen) {
        return NGX_OK;
    }

    return up->original_set_session(pc, up->data);
}


void
ngx_http_wasm_upstream_save_session(ngx_peer_connection_t *pc, void *data)
{
    ngx_http_wasm_upstream_peer_data_t  *up = data;

    if (up->sockaddr && up->socklen) {
        return;
    }

    up->original_save_session(pc, up->data);
}
#endif


ngx_int_t
ngx_http_wasm_upstream_get_peer(ngx_peer_connection_t *pc, void *data)
{
    void                                *peer_data;
    ngx_int_t                            rc = NGX_ERROR;
    ngx_chain_t                         *cl, *next;
    ngx_http_request_t                  *r;
    ngx_http_upstream_t                 *u;
    ngx_http_wasm_req_ctx_t             *rctx;
    ngx_http_wasm_upstream_peer_data_t  *up = data;

    r = up->request;
    u = r->upstream;

    rc = ngx_http_wasm_rctx(r, &rctx);
    if (rc == NGX_DECLINED) {
        goto original;
    } else if (rc != NGX_OK) {
        return NGX_ERROR;
    }

    rctx->req_headers_modified = 0;
    peer_data = r->upstream->peer.data;
    r->upstream->peer.data = data;

    rc = ngx_proxy_wasm_upstream_resume(rctx, NGX_PROXY_WASM_STEP_UPSTREAM_SELECT);
    r->upstream->peer.data = peer_data;
    if (rc == NGX_ERROR || rc >= NGX_HTTP_SPECIAL_RESPONSE) {
        return NGX_ERROR;
    }

    if (rctx->local_resp_status) {
        ngx_http_wasm_flush_local_response(rctx);

        if (r->header_sent) {
            r->upstream->header_sent = 1;
        }

        return NGX_ERROR;
    }

    if (rctx->req_headers_modified) {
        cl = r->upstream->request_bufs;

        if (cl && (!r->request_body || cl != r->request_body->bufs)) {
            ngx_pfree(r->pool, cl->buf->start);
        }

        for (cl = r->upstream->request_bufs; cl; /* void */ ) {
            next = cl->next;
            ngx_free_chain(r->pool, cl);
            cl = next;
        }

        /* reset upstream buffers */
        r->upstream->request_bufs = r->request_body
                                    ? r->request_body->bufs : NULL;

        if (r->upstream->create_request(r) != NGX_OK) {
            return NGX_ERROR;
        }
    }

    if (up->sockaddr && up->socklen) {
        pc->sockaddr = up->sockaddr;
        pc->socklen = up->socklen;
        pc->name = up->name;
        pc->cached = 0;
        pc->connection = NULL;

        if (up->tls) {
            u->ssl = 1;
            u->schema = https;

            if (up->sni.len) {
                u->ssl_name = up->sni;
            }
        } else {
            u->ssl = 0;
            u->schema = http;
        }

        return NGX_OK;
    }

    ngx_wasm_log_error(NGX_LOG_INFO, r->connection->log, 0,
                       "no upstream selected in \"on_upstream_select\"");

original:

    ngx_wasm_log_error(NGX_LOG_INFO, r->connection->log, 0,
                   "calling original get_peer");
    return up->original_get_peer(pc, up->data);
}


void
ngx_http_wasm_upstream_free_peer(ngx_peer_connection_t *pc, void *data,
    ngx_uint_t state)
{
    void                                *peer_data;
    ngx_int_t                            rc;
    ngx_uint_t                           status, mask;
    ngx_http_request_t                  *r;
    ngx_http_wasm_req_ctx_t             *rctx;
    ngx_http_upstream_next_t            *un;
    ngx_http_wasm_upstream_peer_data_t  *up = data;

    if (up->sockaddr && up->socklen) {
        r = up->request;
        rc = ngx_http_wasm_rctx(r, &rctx);
        if (rc != NGX_OK) {
            return;
        }

        up->last_peer_state = state;
        if (pc->tries) {
            pc->tries--;
        }

        if (pc->tries == 0 && state == 0 && r->upstream->state) {

            status = r->upstream->state->status;

            for (un = ngx_http_upstream_next_errors; un->status; un++) {
                if (status != un->status) {
                    continue;
                }

                mask = un->mask;

                if ((r->upstream->conf->next_upstream & mask) == mask) {
                    up->last_peer_state = NGX_PEER_FAILED;
                }

                break;
            }
        }

        up->sockaddr = NULL;
        up->socklen = 0;

        peer_data = r->upstream->peer.data;
        r->upstream->peer.data = data;

        ngx_proxy_wasm_upstream_resume(rctx, NGX_PROXY_WASM_STEP_UPSTREAM_INFO);

        r->upstream->peer.data = peer_data;

        return;
    }

    up->original_free_peer(pc, up->data, state);
}


void
ngx_http_wasm_upstream_notify_peer(ngx_peer_connection_t *pc, void *data,
    ngx_uint_t type)
{
    (void) pc;
    (void) data;
    (void) type;
}


ngx_int_t
ngx_http_wasm_upstream_test_next(ngx_peer_connection_t *pc, void *data,
    ngx_uint_t status)
{
    ngx_int_t                           rc;
    ngx_http_request_t                  *r;
    ngx_http_wasm_req_ctx_t             *rctx;
    ngx_http_wasm_upstream_peer_data_t  *up;

    r = pc->connection->data;
    rc = ngx_http_wasm_rctx(r, &rctx);
    if (rc != NGX_OK) {
        return NGX_OK;
    }

    up = rctx->upstream_peer;
    if (up == NULL) {
        return NGX_OK;
    }

    if (up->sockaddr && up->socklen) {
        up->accept_resp = 0;
        up->last_status = status;

        r->upstream->peer.data = up;

        ngx_proxy_wasm_upstream_resume(rctx, NGX_PROXY_WASM_STEP_NEXT_UPSTREAM);

        r->upstream->peer.data = data;

        if (up->accept_resp) {
            return NGX_DECLINED;
        }
    }

    return NGX_OK;
}


ngx_int_t
ngx_proxy_wasm_upstream_resume(ngx_http_wasm_req_ctx_t *rctx, ngx_proxy_wasm_step_e step)
{
    ngx_int_t              rc;
    ngx_proxy_wasm_ctx_t  *pwctx;
    ngx_proxy_wasm_step_e  prev_step, last_completed_step;

    pwctx = rctx->data;
    if (pwctx == NULL) {
        return NGX_OK;
    }

    prev_step = pwctx->step;
    last_completed_step = pwctx->last_completed_step;
    pwctx->phase = ngx_wasm_phase_lookup(&ngx_http_wasm_subsystem,
                                 NGX_WASM_BACKGROUND_PHASE);

    rc = ngx_proxy_wasm_resume(pwctx, pwctx->phase, step);

    pwctx->step = prev_step;
    pwctx->last_completed_step = last_completed_step;

    return rc;
}


ngx_int_t
ngx_http_wasm_upstream_host_variable(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data)
{
    ngx_http_wasm_req_ctx_t             *rctx;
    ngx_http_variable_value_t           *pv;
    ngx_http_wasm_upstream_peer_data_t  *up;
    ngx_int_t                            proxy_host_idx;

    if (ngx_http_wasm_rctx(r, &rctx) == NGX_OK
        && rctx->upstream_peer)
    {
        up = rctx->upstream_peer;
        if (up->host.len) {
            v->len = up->host.len;
            v->data = up->host.data;
            v->valid = 1;
            v->no_cacheable = 1;
            v->not_found = 0;
            return NGX_OK;
        }
    }

    proxy_host_idx = *(ngx_int_t *) data;
    if (proxy_host_idx != NGX_ERROR) {
        pv = ngx_http_get_indexed_variable(r, proxy_host_idx);
        if (pv && pv->valid && !pv->not_found) {
            *v = *pv;
            v->no_cacheable = 1;
            return NGX_OK;
        }
    }

    v->not_found = 1;
    return NGX_OK;
}


ngx_int_t
ngx_http_wasm_set_upstream(ngx_http_wasm_upstream_peer_data_t  *up,
    ngx_str_t *addr, ngx_int_t port, ngx_uint_t tls, ngx_str_t *sni,
    ngx_pool_t *pool, ngx_proxy_wasm_exec_t *pwexec)
{
    size_t                  len;
    u_char                 *p;
    ngx_url_t               url;
    ngx_proxy_wasm_exec_t  *rexec;

    rexec = ngx_proxy_wasm_rexec(pwexec);

    if (up->sockaddr && up->socklen) {
        ngx_wasm_log_error(NGX_LOG_DEBUG, up->request->connection->log, 0,
                       "upstream \"%V\" already set, overwriting not allowed",
                           up->name);
        return NGX_DECLINED;
    }

    ngx_memzero(&url, sizeof(ngx_url_t));

    if (ngx_strlchr(addr->data, addr->data + addr->len, ':')
        && addr->data[0] != '[')
    {
        len = addr->len + 2;
        p = ngx_pnalloc(pool, len);
        if (p == NULL) {
            return NGX_ERROR;
        }

        p[0] = '[';
        ngx_memcpy(p + 1, addr->data, addr->len);
        p[addr->len + 1] = ']';

    } else {
        len = addr->len;
        p = ngx_pnalloc(pool, addr->len);
        if (p == NULL) {
            return NGX_ERROR;
        }

        ngx_memcpy(p, addr->data, addr->len);
    }

    url.url.data = p;
    url.url.len = len;
    url.default_port = port;
    url.no_resolve = 1;
    url.uri_part = 0;

    if (ngx_parse_url(pool, &url) != NGX_OK
        || url.addrs == NULL
        || url.addrs[0].sockaddr == NULL)
    {
        ngx_wasm_log_error(NGX_LOG_WARN, up->request->connection->log, 0,
                       "invalid upstream address \"%V\"", addr);
        return NGX_DECLINED;
    }

    if (rexec->acl != NULL
        && ngx_wasm_acl_check_addr(rexec->acl, url.addrs[0].sockaddr) != NGX_OK)
    {
        ngx_wasm_log_error(NGX_LOG_WARN, up->request->connection->log, 0,
                       "forbidden upstream address \"%V\"", addr);
        return NGX_DECLINED;
    }

    ngx_wasm_log_error(NGX_LOG_DEBUG, up->request->connection->log, 0,
                   "set upstream peer \"%V\"", &url.addrs[0].name);

    up->sockaddr = url.addrs[0].sockaddr;
    up->socklen = url.addrs[0].socklen;
    up->name = &url.addrs[0].name;
    up->tls = tls;

    if (sni && sni->len) {
        up->sni.data = ngx_palloc(pool, sni->len);
        if (up->sni.data == NULL) {
            return NGX_ERROR;
        }

        ngx_memcpy(up->sni.data, sni->data, sni->len);
        up->sni.len = sni->len;
    }

    return NGX_OK;
}


ngx_int_t
ngx_http_wasm_get_last_upstream_state(ngx_proxy_wasm_ctx_t *pwctx,
    ngx_http_upstream_state_t **state)
{
    ngx_http_request_t       *r;
    ngx_http_wasm_req_ctx_t  *rctx;

    if (pwctx->step != NGX_PROXY_WASM_STEP_UPSTREAM_INFO) {
        return NGX_DECLINED;
    }

    rctx = pwctx->data;
    r = rctx->r;

    if (r->upstream_states == NULL || r->upstream_states->nelts == 0) {
        return NGX_DECLINED;
    }

    *state = (ngx_http_upstream_state_t *) r->upstream_states->elts
             + (r->upstream_states->nelts - 1);
    return NGX_OK;
}


ngx_int_t
ngx_http_wasm_set_upstream_timeouts(ngx_http_request_t *r, ngx_msec_t connect,
    ngx_msec_t send, ngx_msec_t read)
{
    ngx_http_upstream_conf_t            *conf;
    ngx_http_wasm_upstream_peer_data_t  *up;

    up = r->upstream->peer.data;

    if (!up->original_conf) {
        conf = ngx_palloc(r->pool, sizeof(ngx_http_upstream_conf_t));
        if (conf == NULL) {
            return NGX_ERROR;
        }

        up->original_conf = r->upstream->conf;
        *conf = *r->upstream->conf;
        r->upstream->conf = conf;
    }

    conf = r->upstream->conf;

    if (connect) {
        conf->connect_timeout = ngx_min(connect, up->original_conf->connect_timeout);
    }
    if (send) {
        conf->send_timeout = ngx_min(send, up->original_conf->send_timeout);
    }
    if (read) {
        conf->read_timeout = ngx_min(read, up->original_conf->read_timeout);
    }

    return NGX_OK;
}


ngx_int_t
ngx_http_wasm_get_upstreams(ngx_proxy_wasm_exec_t *pwexec, u_char **ret_start, size_t *ret_len)
{
    size_t                          i, j, len, total_addrs, ip_len;
    u_char                         *p;
    ngx_str_t                      *name;
    ngx_http_upstream_server_t     *servers;
    ngx_http_upstream_srv_conf_t   *uscf, **uscfp;
    ngx_http_upstream_main_conf_t  *umcf;
    u_char                          ip_buf[NGX_SOCKADDR_STRLEN];

    umcf = ngx_http_cycle_get_module_main_conf(ngx_cycle, ngx_http_upstream_module);
    if (umcf == NULL) {
        return NGX_DECLINED;
    }

    name = &pwexec->filter->upstream;
    if (name->len == 0) {
        return NGX_DECLINED;
    }

    uscfp = umcf->upstreams.elts;
    uscf = NULL;
    total_addrs = 0;

    for (i = 0; i < umcf->upstreams.nelts; i++) {
        if (uscfp[i]->host.len == name->len
            && ngx_strncasecmp(uscfp[i]->host.data, name->data, name->len) == 0)
        {
            uscf = uscfp[i];
            break;
        }
    }

    if (uscf == NULL) {
        return NGX_DECLINED;
    }

    len = NGX_PROXY_WASM_PTR_SIZE;  /* servers count */
    servers = uscf->servers->elts;

    for (i = 0; i < uscf->servers->nelts; i++) {
        if (servers[i].down) {
            continue;
        }

        for (j = 0; j < servers[i].naddrs; j++) {
            total_addrs++;

            ip_len = ngx_sock_ntop(servers[i].addrs[j].sockaddr,
                                   servers[i].addrs[j].socklen,
                                   ip_buf, NGX_SOCKADDR_STRLEN, 0);

            len += NGX_PROXY_WASM_PTR_SIZE;       /* addr length */
            len += ip_len;                        /* addr */
            len += NGX_PROXY_WASM_PTR_SIZE;       /* port */
            len += 4 * NGX_PROXY_WASM_PTR_SIZE;   /* weight, max_fails, fail_timeout, backup */
        }
    }

    p = ngx_pcalloc(pwexec->pool, len);
    if (p == NULL) {
        return NGX_ERROR;
    }

    *ret_start = p;
    *ret_len = len;

    *(uint32_t *)p = total_addrs;
    p += NGX_PROXY_WASM_PTR_SIZE;

    for (i = 0; i < uscf->servers->nelts; i++) {
        if (servers[i].down) {
            continue;
        }

        for (j = 0; j < servers[i].naddrs; j++) {
            ip_len = ngx_sock_ntop(servers[i].addrs[j].sockaddr,
                                   servers[i].addrs[j].socklen,
                                   ip_buf, NGX_SOCKADDR_STRLEN, 0);

            *(uint32_t *)p = ip_len;
            p += NGX_PROXY_WASM_PTR_SIZE;

            ngx_memcpy(p, ip_buf, ip_len);
            p += ip_len;

            *(uint32_t *)p = ngx_inet_get_port(servers[i].addrs[j].sockaddr);
            p += NGX_PROXY_WASM_PTR_SIZE;

            *(uint32_t *)p = servers[i].weight;
            p += NGX_PROXY_WASM_PTR_SIZE;
            *(uint32_t *)p = servers[i].max_fails;
            p += NGX_PROXY_WASM_PTR_SIZE;
            *(uint32_t *)p = servers[i].fail_timeout;
            p += NGX_PROXY_WASM_PTR_SIZE;
            *(uint32_t *)p = servers[i].backup;
            p += NGX_PROXY_WASM_PTR_SIZE;
        }
    }

    return NGX_OK;
}
