#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_proxy_wasm_properties.h>
#include <ngx_proxy_wasm_maps.h>
#ifdef NGX_WASM_HTTP
#include <ngx_http_proxy_wasm.h>
#endif

#ifndef NGX_WASM_HOST_PROPERTY_NAMESPACE
#define NGX_WASM_HOST_PROPERTY_NAMESPACE  wasmx
#endif

/* see https://gcc.gnu.org/onlinedocs/gcc-13.1.0/cpp/Stringizing.html */
#define NGX_WASM_XSTR(s)  NGX_WASM_STR(s)
#define NGX_WASM_STR(s)   #s
#define NGX_WASM_HOST_PROPERTY_NAMESPACE_STR \
          NGX_WASM_XSTR(NGX_WASM_HOST_PROPERTY_NAMESPACE)


static ngx_int_t ngx_proxy_wasm_properties_get_ngx(
    ngx_proxy_wasm_ctx_t *pwctx, ngx_str_t *path, ngx_str_t *value);
static ngx_int_t ngx_proxy_wasm_properties_set_ngx(
    ngx_proxy_wasm_ctx_t *pwctx, ngx_str_t *path, ngx_str_t *value);


typedef ngx_int_t (*ngx_proxy_wasm_properties_getter)(
    ngx_proxy_wasm_ctx_t *pwctx, ngx_str_t *path, ngx_str_t *value);

typedef struct {
    ngx_str_t                          pwm_key;
    ngx_str_t                          ngx_key;
    ngx_proxy_wasm_properties_getter   getter;
    u_char                            *pwm_key_writable;
} pwm2ngx_mapping_t;


static const char             *ngx_prefix = "ngx.";
static const size_t            ngx_prefix_len = 4;


static ngx_hash_init_t         pwm2ngx_init;
static ngx_hash_combined_t     pwm2ngx_hash;
static ngx_hash_keys_arrays_t  pwm2ngx_keys;


static const char  *host_prefix = NGX_WASM_HOST_PROPERTY_NAMESPACE_STR ".";
static size_t       host_prefix_len;


typedef struct {
    ngx_str_node_t  sn;
    ngx_str_t       value;
    unsigned        is_const:1;
    unsigned        negative_cache:1;
} host_props_node_t;


static ngx_int_t
nyi(ngx_proxy_wasm_ctx_t *pwctx, ngx_str_t *path, ngx_str_t *value)
{
    ngx_wavm_log_error(NGX_LOG_WASM_NYI, pwctx->log, NULL,
                       "NYI - \"%V\" property", path);

    return NGX_DECLINED;
}


static ngx_int_t
not_supported(ngx_proxy_wasm_ctx_t *pwctx, ngx_str_t *path, ngx_str_t *value)
{
    ngx_wavm_log_error(NGX_LOG_ERR, pwctx->log, NULL,
                       "\"%V\" property not supported", path);

    return NGX_DECLINED;
}


static ngx_uint_t
hash_str(u_char *src, size_t n)
{
    ngx_uint_t  key;

    key = 0;

    while (n--) {
        key = ngx_hash(key, *src);
        src++;
    }

    return key;
}


#ifdef NGX_WASM_HTTP
static const size_t  request_headers_prefix_len = 16;   /* request.headers. */
static const size_t  response_headers_prefix_len = 17;  /* response.headers. */


static ngx_int_t
get_map_value(ngx_proxy_wasm_ctx_t *pwctx, ngx_str_t *name, ngx_str_t *value,
    ngx_proxy_wasm_map_type_e map_type)
{
    ngx_proxy_wasm_exec_t  *pwexec, *pwexecs;
    ngx_wavm_instance_t    *instance;
    ngx_str_t              *r;

    pwexecs = (ngx_proxy_wasm_exec_t *) pwctx->pwexecs.elts;
    pwexec = &pwexecs[pwctx->exec_index];

    instance = ngx_proxy_wasm_pwexec2instance(pwexec);

    r = ngx_proxy_wasm_maps_get(instance, map_type, name);
    if (r == NULL) {
        return NGX_DECLINED;
    }

    value->data = r->data;
    value->len = r->len;

    return NGX_OK;
}


static ngx_int_t
get_referer(ngx_proxy_wasm_ctx_t *pwctx, ngx_str_t *path, ngx_str_t *value)
{
    static ngx_str_t   header = ngx_string("referer");

    return get_map_value(pwctx, &header, value,
                         NGX_PROXY_WASM_MAP_HTTP_REQUEST_HEADERS);
}


static ngx_int_t
get_request_id(ngx_proxy_wasm_ctx_t *pwctx, ngx_str_t *path,
    ngx_str_t *value)
{
    static ngx_str_t   header = ngx_string("x-request-id");

    return get_map_value(pwctx, &header, value,
                         NGX_PROXY_WASM_MAP_HTTP_REQUEST_HEADERS);
}


static ngx_int_t
get_request_time(ngx_proxy_wasm_ctx_t *pwctx, ngx_str_t *path,
    ngx_str_t *value)
{
    ngx_http_wasm_req_ctx_t  *rctx = (ngx_http_wasm_req_ctx_t *) pwctx->data;
    ngx_http_request_t       *r = rctx->r;

    if (!pwctx->start_time.len) {
        pwctx->start_time.data = ngx_pnalloc(pwctx->pool, NGX_TIME_T_LEN + 4);
        if (pwctx->start_time.data == NULL) {
            return NGX_ERROR;
        }

        pwctx->start_time.len = ngx_sprintf(pwctx->start_time.data, "%T.%03M",
                                            r->start_sec, r->start_msec)
                                - pwctx->start_time.data;
    }

    value->len = pwctx->start_time.len;
    value->data = pwctx->start_time.data;

    return NGX_OK;
}


static ngx_int_t
get_upstream_address(ngx_proxy_wasm_ctx_t *pwctx, ngx_str_t *path,
    ngx_str_t *value)
{
    size_t                    len;
    ngx_http_wasm_req_ctx_t  *rctx = (ngx_http_wasm_req_ctx_t *) pwctx->data;
    ngx_http_request_t       *r = rctx->r;
    ngx_http_upstream_t      *u = r->upstream;
    u_char                   *p;

    if (u == NULL) {
        return NGX_DECLINED;
    }

    p = u->peer.name->data;

    if (!pwctx->upstream_address.len) {
        len = ((u_char *) strrchr((char *) p, ':')) - p;

        pwctx->upstream_address.len = len;
        pwctx->upstream_address.data = ngx_pnalloc(pwctx->pool, len);
        if (pwctx->upstream_address.data == NULL) {
            return NGX_ERROR;
        }

        ngx_memcpy(pwctx->upstream_address.data, u->peer.name->data, len);
    }

    value->data = pwctx->upstream_address.data;
    value->len = pwctx->upstream_address.len;

    return NGX_OK;
}


static ngx_int_t
get_upstream_port(ngx_proxy_wasm_ctx_t *pwctx, ngx_str_t *path,
    ngx_str_t *value)
{
    size_t                    a_len, p_len;
    ngx_http_wasm_req_ctx_t  *rctx = (ngx_http_wasm_req_ctx_t *) pwctx->data;
    ngx_http_request_t       *r = rctx->r;
    ngx_http_upstream_t      *u = r->upstream;
    u_char                   *p;

    if (u == NULL) {
        return NGX_DECLINED;
    }

    p = u->peer.name->data;

    if (!pwctx->upstream_port.len) {
        a_len = (((u_char *) strrchr((char *) p, ':')) - p) + 1;
        p_len = u->peer.name->len - a_len;

        pwctx->upstream_port.len = p_len;
        pwctx->upstream_port.data = ngx_pnalloc(pwctx->pool, p_len);
        if (pwctx->upstream_port.data == NULL) {
            return NGX_ERROR;
        }

        ngx_memcpy(pwctx->upstream_port.data, u->peer.name->data + a_len,
                   p_len);
    }

    value->data = pwctx->upstream_port.data;
    value->len = pwctx->upstream_port.len;

    return NGX_OK;
}


static ngx_int_t
get_request_header(ngx_proxy_wasm_ctx_t *pwctx, ngx_str_t *path,
    ngx_str_t *value)
{
    ngx_str_t   name;

    name.data = (u_char *) (path->data + request_headers_prefix_len);
    name.len = path->len - request_headers_prefix_len;

    return get_map_value(pwctx, &name, value,
                         NGX_PROXY_WASM_MAP_HTTP_REQUEST_HEADERS);
}


static ngx_int_t
get_response_header(ngx_proxy_wasm_ctx_t *pwctx, ngx_str_t *path,
    ngx_str_t *value)
{
    ngx_str_t   name;

    name.data = (u_char *) (path->data + response_headers_prefix_len);
    name.len = path->len - response_headers_prefix_len;

    return get_map_value(pwctx, &name, value,
                         NGX_PROXY_WASM_MAP_HTTP_RESPONSE_HEADERS);
}


static ngx_int_t
get_connection_id(ngx_proxy_wasm_ctx_t *pwctx, ngx_str_t *path,
    ngx_str_t *value)
{
    size_t                    len;
    u_char                    buf[NGX_OFF_T_LEN];
    ngx_http_wasm_req_ctx_t  *rctx = (ngx_http_wasm_req_ctx_t *) pwctx->data;
    ngx_http_request_t       *r = rctx->r;

    if (!pwctx->connection_id.len) {
        len = ngx_sprintf(buf, "%i", r->connection->number) - buf;

        pwctx->connection_id.data = ngx_pnalloc(pwctx->pool, len);
        if (pwctx->connection_id.data == NULL) {
            return NGX_ERROR;
        }

        ngx_memcpy(pwctx->connection_id.data, buf, len);
        pwctx->connection_id.len = len;
    }

    value->len = pwctx->connection_id.len;
    value->data = pwctx->connection_id.data;

    return NGX_OK;
}


static ngx_int_t
get_connection_mtls(ngx_proxy_wasm_ctx_t *pwctx, ngx_str_t *path,
    ngx_str_t *value)
{
    unsigned          on;
    ngx_int_t         rc;
    ngx_str_t         https_v, verify_v, *result;

    static ngx_str_t  https_p = ngx_string("ngx.https");
    static ngx_str_t  https_e = ngx_string("on");

    static ngx_str_t  verify_p = ngx_string("ngx.ssl_client_verify");
    static ngx_str_t  verify_e = ngx_string("SUCCESS");

    static ngx_str_t  mtls_on  = ngx_string("true");
    static ngx_str_t  mtls_off = ngx_string("false");

    if (!pwctx->mtls.len) {
        rc = ngx_proxy_wasm_properties_get_ngx(pwctx, &https_p, &https_v);
        if (rc != NGX_OK) {
            return rc;
        }

        rc = ngx_proxy_wasm_properties_get_ngx(pwctx, &verify_p, &verify_v);
        if (rc != NGX_OK) {
            return rc;
        }

        on = ngx_str_eq(https_v.data, https_v.len, https_e.data, https_e.len)
             && ngx_str_eq(verify_v.data, verify_v.len,
                           verify_e.data, verify_e.len);

        result = on ? &mtls_on : &mtls_off;

        pwctx->mtls.data = ngx_pnalloc(pwctx->pool, result->len);
        if (pwctx->mtls.data == NULL) {
            return NGX_ERROR;
        }

        ngx_memcpy(pwctx->mtls.data, result->data, result->len);
        pwctx->mtls.len = result->len;
    }

    value->data = pwctx->mtls.data;
    value->len = pwctx->mtls.len;

    return NGX_OK;
}
#endif


static ngx_int_t
get_filter_name(ngx_proxy_wasm_ctx_t *pwctx, ngx_str_t *path,
    ngx_str_t *value)
{
    ngx_proxy_wasm_exec_t    *pwexec, *pwexecs;
    ngx_proxy_wasm_filter_t  *filter;

    pwexecs = (ngx_proxy_wasm_exec_t *) pwctx->pwexecs.elts;
    pwexec = &pwexecs[pwctx->exec_index];
    filter = pwexec->filter;

    value->len = filter->name->len;
    value->data = filter->name->data;

    return NGX_OK;
}


static ngx_int_t
get_filter_root_id(ngx_proxy_wasm_ctx_t *pwctx, ngx_str_t *path,
    ngx_str_t *value)
{
    size_t  len;
    u_char  buf[NGX_OFF_T_LEN];

    if (!pwctx->root_id.len) {
        len = ngx_sprintf(buf, "%i", pwctx->root_id) - buf;

        pwctx->root_id.data = ngx_pnalloc(pwctx->pool, len);
        if (pwctx->root_id.data == NULL) {
            return NGX_ERROR;
        }

        ngx_memcpy(pwctx->root_id.data, buf, len);
        pwctx->root_id.len = len;
    }

    value->len = pwctx->root_id.len;
    value->data = pwctx->root_id.data;

    return NGX_OK;
}


static pwm2ngx_mapping_t  pw2ngx[] = {
#ifdef NGX_WASM_HTTP

    /* Request properties */

    { ngx_string("request.path"),
      ngx_string("ngx.request_uri"),
      NULL, NULL },
    { ngx_string("request.url_path"),
      ngx_string("ngx.uri"),
      NULL, NULL },
    { ngx_string("request.host"),
      ngx_string("ngx.hostname"),
      NULL, NULL },
    { ngx_string("request.scheme"),
      ngx_string("ngx.scheme"),
      NULL, NULL },
    { ngx_string("request.method"),
      ngx_string("ngx.request_method"),
      NULL, NULL },
    { ngx_string("request.useragent"),
      ngx_string("ngx.http_user_agent"),
      NULL, NULL },
    { ngx_string("request.protocol"),
      ngx_string("ngx.server_protocol"),
      NULL, NULL },
    { ngx_string("request.query"),
      ngx_string("ngx.args"),
      NULL, NULL },
    { ngx_string("request.id"),
      ngx_null_string,
      &get_request_id, NULL },
    { ngx_string("request.referer"),
      ngx_null_string,
      &get_referer, NULL },
    { ngx_string("request.time"),
      ngx_null_string,
      &get_request_time, NULL },
    { ngx_string("request.duration"),
      ngx_string("ngx.request_time"),
      NULL, NULL },
    { ngx_string("request.size"),
      ngx_string("ngx.content_length"),
      NULL, NULL },
    { ngx_string("request.total_size"),
      ngx_string("ngx.request_length"),
      NULL, NULL },
    { ngx_string("request.headers.*"),
      ngx_null_string,
      &get_request_header, NULL },

    /* Response properties */

    { ngx_string("response.code"),
      ngx_string("ngx.status"),
      NULL, NULL },
    { ngx_string("response.size"),
      ngx_string("ngx.body_bytes_sent"),
      NULL, NULL },
    { ngx_string("response.total_size"),
      ngx_string("ngx.bytes_sent"),
      NULL, NULL },
    { ngx_string("response.grpc_status"),
      ngx_null_string,
      &nyi, NULL },
    { ngx_string("response.trailers"),
      ngx_null_string,
      &nyi, NULL },
    { ngx_string("response.code_details"),
      ngx_null_string,
      &not_supported, NULL },
    { ngx_string("response.flags"),
      ngx_null_string,
      &not_supported, NULL },
    { ngx_string("response.headers.*"),
      ngx_null_string,
      &get_response_header, NULL },

    /* Upstream properties */

    { ngx_string("upstream.address"),
      ngx_null_string,
      &get_upstream_address, NULL },
    { ngx_string("upstream.port"),
      ngx_null_string,
      &get_upstream_port, NULL },
    { ngx_string("upstream.tls_version"),
      ngx_null_string,
      &nyi, NULL },
    { ngx_string("upstream.subject_local_certificate"),
      ngx_null_string,
      &nyi, NULL },
    { ngx_string("upstream.subject_peer_certificate"),
      ngx_null_string,
      &nyi, NULL },
    { ngx_string("upstream.dns_san_local_certificate"),
      ngx_null_string,
      &nyi, NULL },
    { ngx_string("upstream.dns_san_peer_certificate"),
      ngx_null_string,
      &nyi, NULL },
    { ngx_string("upstream.uri_san_local_certificate"),
      ngx_null_string,
      &nyi, NULL },
    { ngx_string("upstream.uri_san_peer_certificate"),
      ngx_null_string,
      &nyi, NULL },
    { ngx_string("upstream.sha256_peer_certificate_digest"),
      ngx_null_string,
      &nyi, NULL },
    { ngx_string("upstream.local_address"),
      ngx_null_string,
      &nyi, NULL },
    { ngx_string("upstream.transport_failure_reason"),
      ngx_null_string,
      &nyi, NULL },

    /* Connection properties */

    { ngx_string("destination.address"),
      ngx_string("ngx.proxy_protocol_addr"),
      NULL, NULL },
    { ngx_string("destination.port"),
      ngx_string("ngx.proxy_protocol_port"),
      NULL, NULL },
    { ngx_string("connection.requested_server_name"),
      ngx_string("ngx.ssl_server_name"),
      NULL, NULL },
    { ngx_string("connection.tls_version"),
      ngx_string("ngx.ssl_protocol"),
      NULL, NULL },
    { ngx_string("connection.subject_local_certificate"),
      ngx_string("ngx.ssl_client_s_dn"),
      NULL, NULL },
    { ngx_string("connection.id"),
      ngx_null_string,
      &get_connection_id, NULL },
    { ngx_string("connection.mtls"),
      ngx_null_string,
      &get_connection_mtls, NULL },
    { ngx_string("connection.subject_peer_certificate"),
      ngx_null_string,
      &nyi, NULL },
    { ngx_string("connection.dns_san_local_certificate"),
      ngx_null_string,
      &nyi, NULL },
    { ngx_string("connection.dns_san_peer_certificate"),
      ngx_null_string,
      &nyi, NULL },
    { ngx_string("connection.uri_san_local_certificate"),
      ngx_null_string,
      &nyi, NULL },
    { ngx_string("connection.uri_san_peer_certificate"),
      ngx_null_string,
      &nyi, NULL },
    { ngx_string("connection.sha256_peer_certificate_digest"),
      ngx_null_string,
      &nyi, NULL },
    { ngx_string("connection.termination_details"),
      ngx_null_string,
      &not_supported, NULL },
#endif
    { ngx_string("source.address"),
      ngx_string("ngx.remote_addr"),
      NULL, NULL },
    { ngx_string("source.port"),
      ngx_string("ngx.remote_port"),
      NULL, NULL },

    /* proxy-wasm properties */

    { ngx_string("plugin_name"),
      ngx_null_string,
      &get_filter_name, NULL },
    { ngx_string("plugin_root_id"),
      ngx_null_string,
      &get_filter_root_id, NULL },
    { ngx_string("plugin_vm_id"),
      ngx_null_string,
      &nyi, NULL },
    { ngx_string("node"),
      ngx_null_string,
      &not_supported, NULL },
    { ngx_string("cluster_name"),
      ngx_null_string,
      &not_supported, NULL },
    { ngx_string("cluster_metadata"),
      ngx_null_string,
      &not_supported, NULL },
    { ngx_string("listener_direction"),
      ngx_null_string,
      &not_supported, NULL },
    { ngx_string("listener_metadata"),
      ngx_null_string,
      &not_supported, NULL },
    { ngx_string("route_name"),
      ngx_null_string,
      &not_supported, NULL },
    { ngx_string("route_metadata"),
      ngx_null_string,
      &not_supported, NULL },
    { ngx_string("upstream_host_metadata"),
      ngx_null_string,
      &not_supported, NULL },

    { ngx_null_string, ngx_null_string, NULL, NULL }
};


static int ngx_libc_cdecl
cmp_properties_wildcards(const void *one, const void *two)
{
    ngx_hash_key_t  *first, *second;

    first = (ngx_hash_key_t *) one;
    second = (ngx_hash_key_t *) two;

    return ngx_dns_strcmp(first->key.data, second->key.data);
}


static ngx_int_t
add_mapping_to_hash(ngx_conf_t *cf, ngx_hash_keys_arrays_t *keys, ngx_str_t *k,
    pwm2ngx_mapping_t *m)
{
    ngx_uint_t  flag;
    ngx_str_t   aux = { k->len, NULL };

    flag = k->data[k->len - 1] == '*' ? NGX_HASH_WILDCARD_KEY
                                      : NGX_HASH_READONLY_KEY;

    if (flag & NGX_HASH_WILDCARD_KEY) {
        m->pwm_key_writable = ngx_pnalloc(cf->pool, k->len);
        if (m->pwm_key_writable == NULL) {
            ngx_wavm_log_error(NGX_LOG_ERR, cf->log, NULL,
                               "failed allocating memory for \"%V\" \
                               wildcard hash key", k);
            return NGX_ERROR;

        }

        ngx_memcpy(m->pwm_key_writable, k->data, k->len);
        aux.data = m->pwm_key_writable;

        return ngx_hash_add_key(keys, &aux, m, flag);
    }

    return ngx_hash_add_key(keys, k, m, flag);
}


void
ngx_proxy_wasm_properties_init(ngx_conf_t *cf)
{
    size_t              i;
    ngx_int_t           rc = NGX_ERROR;
    pwm2ngx_mapping_t  *m;

    pwm2ngx_init.hash = &pwm2ngx_hash.hash;
    pwm2ngx_init.key = ngx_hash_key;
    pwm2ngx_init.max_size = 256;
    pwm2ngx_init.bucket_size = ngx_align(64, ngx_cacheline_size);
    pwm2ngx_init.name = "pwm2ngx_properties";
    pwm2ngx_init.pool = cf->pool;
    pwm2ngx_init.temp_pool = cf->temp_pool;

    pwm2ngx_keys.pool = cf->pool;
    pwm2ngx_keys.temp_pool = cf->temp_pool;

    rc = ngx_hash_keys_array_init(&pwm2ngx_keys, NGX_HASH_SMALL);
    if (rc != NGX_OK) {
        ngx_wavm_log_error(NGX_LOG_ERR, cf->log, NULL,
                           "failed initializing \"%V\" hash keys",
                           pwm2ngx_init.name);
        return;
    }

    for (i = 0; pw2ngx[i].pwm_key.len; i++) {
        m = &pw2ngx[i];
        rc = add_mapping_to_hash(cf, &pwm2ngx_keys, &m->pwm_key, m);
        if (rc != NGX_OK) {
            ngx_wavm_log_error(NGX_LOG_WARN, cf->log, NULL,
                               "failed adding \"%V\" key to \"%V\" hash",
                               m->pwm_key, pwm2ngx_init.name);
        }
    }

    ngx_hash_init(&pwm2ngx_init, pwm2ngx_keys.keys.elts,
                  pwm2ngx_keys.keys.nelts);

    if (pwm2ngx_keys.dns_wc_tail.nelts) {
        /**
         * leveraging ngx hash wildcard (usually intended for DNS matching)
         * to match request.headers.foo as request.headers.*
         */
        ngx_qsort(pwm2ngx_keys.dns_wc_tail.elts,
                  (size_t) pwm2ngx_keys.dns_wc_tail.nelts,
                  sizeof(ngx_hash_key_t),
                  cmp_properties_wildcards);

        pwm2ngx_init.hash = NULL;
        pwm2ngx_init.temp_pool = cf->temp_pool;

        ngx_hash_wildcard_init(&pwm2ngx_init,
                               pwm2ngx_keys.dns_wc_tail.elts,
                               pwm2ngx_keys.dns_wc_tail.nelts);

        pwm2ngx_hash.wc_tail = (ngx_hash_wildcard_t *) pwm2ngx_init.hash;
    }

    host_prefix_len = ngx_strlen(host_prefix);
}


static ngx_int_t
ngx_proxy_wasm_properties_get_ngx(ngx_proxy_wasm_ctx_t *pwctx,
    ngx_str_t *path, ngx_str_t *value)
{
#ifdef NGX_WASM_HTTP
    ngx_uint_t                  hash;
    ngx_str_t                   name;
    ngx_http_variable_value_t  *vv;
    ngx_http_wasm_req_ctx_t    *rctx;

    name.data = (u_char *) (path->data + ngx_prefix_len);
    name.len = path->len - ngx_prefix_len;

    hash = hash_str(name.data, name.len);

    rctx = (ngx_http_wasm_req_ctx_t *) pwctx->data;

    if (rctx == NULL || rctx->fake_request) {
        ngx_wavm_log_error(NGX_LOG_ERR, pwctx->log, NULL,
                           "cannot get ngx properties outside of a request");
        return NGX_ERROR;
    }

    vv = ngx_http_get_variable(rctx->r, &name, hash);
    if (vv && !vv->not_found) {
        value->data = vv->data;
        value->len = vv->len;

        return NGX_OK;
    }
#endif

    return NGX_DECLINED;
}


static ngx_int_t
ngx_proxy_wasm_properties_set_ngx(ngx_proxy_wasm_ctx_t *pwctx,
    ngx_str_t *path, ngx_str_t *value)
{
#ifdef NGX_WASM_HTTP
    ngx_uint_t                  hash;
    ngx_str_t                   name;
    u_char                     *p;
    ngx_http_variable_t        *v;
    ngx_http_variable_value_t  *vv;
    ngx_http_request_t         *r;
    ngx_http_wasm_req_ctx_t    *rctx;
    ngx_http_core_main_conf_t  *cmcf;

    rctx = (ngx_http_wasm_req_ctx_t *) pwctx->data;
    if (rctx == NULL || rctx->fake_request) {
        ngx_wavm_log_error(NGX_LOG_ERR, pwctx->log, NULL,
                           "cannot set ngx properties outside of a request");
        return NGX_ERROR;
    }

    r = rctx->r;

    cmcf = ngx_http_get_module_main_conf(r, ngx_http_core_module);
    if (cmcf == NULL) {
        /* possible path on fake request, no http{} block */
        ngx_wavm_log_error(NGX_LOG_ERR, pwctx->log, NULL,
                           "%V", &NGX_WASM_STR_NO_HTTP);
        return NGX_ERROR;
    }

    name.data = (u_char *) (path->data + ngx_prefix_len);
    name.len = path->len - ngx_prefix_len;

    hash = hash_str(name.data, name.len);

    v = ngx_hash_find(&cmcf->variables_hash, hash, name.data, name.len);
    if (v == NULL) {
        return NGX_DECLINED;
    }

    if (!(v->flags & NGX_HTTP_VAR_CHANGEABLE)) {
        ngx_wavm_log_error(NGX_LOG_ERR, pwctx->log, NULL,
                           "variable \"%V\" is not changeable", &name);

        return NGX_ERROR;
    }

    if (v->set_handler) {
        vv = ngx_pcalloc(r->pool, sizeof(ngx_http_variable_value_t)
                         + value->len);
        if (vv == NULL) {
            return NGX_ERROR;
        }

        if (value->data) {
            vv->valid = 1;
            vv->len = value->len;

            if (vv->len) {
                vv->data = (u_char *) vv + sizeof(ngx_http_variable_value_t);
                ngx_memcpy(vv->data, value->data, vv->len);
            }

        } else {
            vv->not_found = 1;
        }

        v->set_handler(r, vv, v->data);

        return NGX_OK;
    }

    if (v->flags & NGX_HTTP_VAR_INDEXED) {

#if 0
        /* fake requests are presently caught above */
        if (r->variables == NULL) {
            ngx_wasm_assert(r->connection->fd == NGX_WASM_BAD_FD);
            return NGX_ERROR;
        }
#endif

        vv = &r->variables[v->index];

        if (value->data == NULL) {
            vv->valid = 0;
            vv->not_found = 1;
            vv->no_cacheable = 0;
            vv->data = NULL;
            vv->len = 0;

            return NGX_OK;
        }

        p = ngx_pnalloc(r->pool, value->len);
        if (p == NULL) {
            return NGX_ERROR;
        }

        ngx_memcpy(p, value->data, value->len);

        vv->valid = 1;
        vv->not_found = 0;
        vv->no_cacheable = 0;
        vv->data = p;
        vv->len = value->len;

        return NGX_OK;
    }
#endif

    return NGX_DECLINED;
}


static ngx_int_t
ngx_proxy_wasm_properties_get_host(ngx_proxy_wasm_ctx_t *pwctx,
    ngx_str_t *path, ngx_str_t *value)
{
    host_props_node_t        *hpn;
    uint32_t                  hash;
#ifdef NGX_WASM_HTTP
    ngx_http_wasm_req_ctx_t  *rctx = pwctx->data;

    if (rctx == NULL || rctx->fake_request) {
        ngx_wavm_log_error(NGX_LOG_ERR, pwctx->log, NULL,
                           "cannot get host properties outside of a request");
        return NGX_ERROR;
    }
#endif

    hash = hash_str(path->data, path->len);

    hpn = (host_props_node_t *)
              ngx_str_rbtree_lookup(&pwctx->host_props_tree, path, hash);
    if (!hpn) {
        return NGX_DECLINED;
    }

    /* stored value is not const: need to call getter again */
    if (!hpn->is_const && pwctx->host_prop_getter) {
        return NGX_DECLINED;
    }

    value->data = hpn->value.data;
    value->len = hpn->value.len;

    if (hpn->negative_cache) {
        return NGX_BUSY;
    }

    return NGX_OK;
}


static ngx_inline ngx_int_t
set_hpn_value(host_props_node_t *hpn, ngx_str_t *value, unsigned is_const,
    ngx_pool_t *pool)
{
    unsigned char  *new_data;

    if (value->data == NULL) {
        new_data = NULL;
        hpn->negative_cache = 1;

    } else {
        new_data = ngx_pstrdup(pool, value);
        if (new_data == NULL) {
            return NGX_ERROR;
        }
    }

    hpn->is_const = is_const;
    hpn->value.len = value->len;
    hpn->value.data = new_data;

    return NGX_OK;
}


ngx_int_t
ngx_proxy_wasm_properties_set_host(ngx_proxy_wasm_ctx_t *pwctx,
    ngx_str_t *path, ngx_str_t *value, unsigned is_const, unsigned retrieve)
{
    host_props_node_t        *hpn;
    uint32_t                  hash;
    ngx_int_t                 rc;
#ifdef NGX_WASM_HTTP
    ngx_http_wasm_req_ctx_t  *rctx = pwctx->data;

    if (rctx == NULL || rctx->fake_request) {
        ngx_wavm_log_error(NGX_LOG_ERR, pwctx->log, NULL,
                           "cannot set host properties outside of a request");
        return NGX_ERROR;
    }
#endif

    hash = hash_str(path->data, path->len);

    hpn = (host_props_node_t *)
              ngx_str_rbtree_lookup(&pwctx->host_props_tree, path, hash);

    if (hpn) {
        if (hpn->is_const) {
            return NGX_DECLINED;
        }

        ngx_pfree(pwctx->pool, hpn->value.data);

        if (value->data == NULL && !is_const) {
            ngx_rbtree_delete(&pwctx->host_props_tree, &hpn->sn.node);

            return NGX_OK;
        }

        rc = set_hpn_value(hpn, value, is_const, pwctx->pool);
        if (rc != NGX_OK) {
            return rc;
        }

    } else if (value->data == NULL && !is_const) {
        /* no need to store a non-const NULL */
        return NGX_OK;

    } else {
        hpn = ngx_pcalloc(pwctx->pool, sizeof(host_props_node_t) + path->len);
        if (hpn == NULL) {
            return NGX_ERROR;
        }

        hpn->sn.node.key = hash;
        hpn->sn.str.len = path->len;
        hpn->sn.str.data = (u_char *) hpn + sizeof(host_props_node_t);
        ngx_memcpy(hpn->sn.str.data, path->data, path->len);

        rc = set_hpn_value(hpn, value, is_const, pwctx->pool);
        if (rc != NGX_OK) {
            return rc;
        }

        ngx_rbtree_insert(&pwctx->host_props_tree, &hpn->sn.node);
    }

    if (retrieve) {
        value->data = hpn->value.data;
    }

    return NGX_OK;
}


static u_char *
replace_nulls_by_dots(ngx_str_t *from, u_char *to)
{
    size_t  i;

    for (i = 0; i < from->len; i++) {
        to[i] = from->data[i] == '\0' ? '.' : from->data[i];
    }

    return to;
}


ngx_int_t
ngx_proxy_wasm_properties_get(ngx_proxy_wasm_ctx_t *pwctx,
    ngx_str_t *path, ngx_str_t *value)
{
    u_char              dotted_path_buf[path->len];
    ngx_str_t           p = { path->len, NULL };
    ngx_uint_t          key;
    pwm2ngx_mapping_t  *m;
    ngx_int_t           rc;

    p.data = replace_nulls_by_dots(path, dotted_path_buf);
    key = ngx_hash_key(p.data, p.len);

    m = ngx_hash_find_combined(&pwm2ngx_hash, key, p.data, p.len);
    if (m) {
        if (!m->getter) {
            /* attribute mapped to an nginx variable */
            return ngx_proxy_wasm_properties_get_ngx(pwctx, &m->ngx_key,
                                                     value);
        }

        /* attribute getter */
        return m->getter(pwctx, &p, value);
    }

    if (p.len > ngx_prefix_len
        && ngx_memcmp(p.data, ngx_prefix, ngx_prefix_len) == 0)
    {
        /* nginx variable (ngx.*) */
        return ngx_proxy_wasm_properties_get_ngx(pwctx, &p, value);
    }

    if (p.len > host_prefix_len
        && ngx_memcmp(p.data, host_prefix, host_prefix_len) == 0)
    {
        /* host variable */

        /* even if there is a getter, try reading const value first */
        rc = ngx_proxy_wasm_properties_get_host(pwctx, &p, value);
        if (rc == NGX_OK) {
            return NGX_OK;

        } else if (rc == NGX_BUSY) {
            return NGX_DECLINED;
        }

        if (pwctx->host_prop_getter) {
            return pwctx->host_prop_getter(pwctx->host_prop_getter_data,
                                           &p, value);
        }

        return rc;
    }

    return NGX_DECLINED;
}


ngx_int_t
ngx_proxy_wasm_properties_set(ngx_proxy_wasm_ctx_t *pwctx,
    ngx_str_t *path, ngx_str_t *value)
{
    u_char              dotted_path_buf[path->len];
    ngx_str_t           p = { path->len, NULL };
    ngx_uint_t          key;
    pwm2ngx_mapping_t  *m;

    p.data = replace_nulls_by_dots(path, dotted_path_buf);

    if (p.len > ngx_prefix_len
        && ngx_memcmp(p.data, ngx_prefix, ngx_prefix_len) == 0)
    {
        /* nginx variable (ngx.*) */
        return ngx_proxy_wasm_properties_set_ngx(pwctx, &p, value);
    }

    key = ngx_hash_key(p.data, p.len);
    m = ngx_hash_find_combined(&pwm2ngx_hash, key, p.data, p.len);
    if (m && m->ngx_key.len) {
        /* attribute mapped to nginx variable */
        return ngx_proxy_wasm_properties_set_ngx(pwctx, &m->ngx_key, value);
    }

    if (p.len > host_prefix_len
        && ngx_memcmp(p.data, host_prefix, host_prefix_len) == 0)
    {
        /* host variable */
        if (pwctx->host_prop_setter) {
            return pwctx->host_prop_setter(pwctx->host_prop_setter_data,
                                           &p, value);
        }

        return ngx_proxy_wasm_properties_set_host(pwctx, &p, value, 0, 0);
    }

    return NGX_DECLINED;
}


ngx_int_t
ngx_proxy_wasm_properties_set_host_prop_setter(ngx_proxy_wasm_ctx_t *pwctx,
    ngx_wasm_host_prop_fn_t fn, void *data)
{
#ifdef NGX_WASM_HTTP
    ngx_http_wasm_req_ctx_t  *rctx = pwctx->data;

    if (rctx == NULL || rctx->fake_request) {
        ngx_wavm_log_error(NGX_LOG_ERR, pwctx->log, NULL,
                           "cannot set host properties setter "
                           "outside of a request");
        return NGX_ERROR;
    }
#endif

    pwctx->host_prop_setter = fn;
    pwctx->host_prop_setter_data = data;

    return NGX_OK;
}


ngx_int_t
ngx_proxy_wasm_properties_set_host_prop_getter(ngx_proxy_wasm_ctx_t *pwctx,
    ngx_wasm_host_prop_fn_t fn, void *data)
{
#ifdef NGX_WASM_HTTP
    ngx_http_wasm_req_ctx_t  *rctx = pwctx->data;

    if (rctx == NULL || rctx->fake_request) {
        ngx_wavm_log_error(NGX_LOG_ERR, pwctx->log, NULL,
                           "cannot set host properties getter "
                           "outside of a request");
        return NGX_ERROR;
    }
#endif

    pwctx->host_prop_getter = fn;
    pwctx->host_prop_getter_data = data;

    return NGX_OK;
}
