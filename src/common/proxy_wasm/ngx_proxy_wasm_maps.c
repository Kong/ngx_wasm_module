#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_proxy_wasm_maps.h>
#include <ngx_proxy_wasm_properties.h>
#ifdef NGX_WASM_HTTP
#include <ngx_http_proxy_wasm.h>
#endif


static ngx_list_t *ngx_proxy_wasm_maps_get_map(ngx_wavm_instance_t *instance,
    ngx_proxy_wasm_map_type_e map_type);
static ngx_str_t *ngx_proxy_wasm_maps_get_special_key(
    ngx_wavm_instance_t *instance, ngx_uint_t map_type, ngx_str_t *key);
static ngx_int_t ngx_proxy_wasm_maps_set_special_key(
    ngx_wavm_instance_t *instance, ngx_uint_t map_type,
    ngx_str_t *key, ngx_str_t *value);
#ifdef NGX_WASM_HTTP
static ngx_str_t *ngx_proxy_wasm_maps_get_path(ngx_wavm_instance_t *instance,
    ngx_proxy_wasm_map_type_e map_type);
static ngx_int_t ngx_proxy_wasm_maps_set_path(ngx_wavm_instance_t *instance,
    ngx_str_t *value, ngx_proxy_wasm_map_type_e map_type);
static ngx_str_t *ngx_proxy_wasm_maps_get_method(ngx_wavm_instance_t *instance,
    ngx_proxy_wasm_map_type_e map_type);
static ngx_int_t ngx_proxy_wasm_maps_set_method(ngx_wavm_instance_t *instance,
    ngx_str_t *value, ngx_proxy_wasm_map_type_e map_type);
static ngx_str_t *ngx_proxy_wasm_maps_get_scheme(ngx_wavm_instance_t *instance,
    ngx_proxy_wasm_map_type_e map_type);
static ngx_str_t *ngx_proxy_wasm_maps_get_authority(
    ngx_wavm_instance_t *instance, ngx_proxy_wasm_map_type_e map_type);
static ngx_str_t *ngx_proxy_wasm_maps_get_response_status(
    ngx_wavm_instance_t *instance, ngx_proxy_wasm_map_type_e map_type);
static ngx_str_t *ngx_proxy_wasm_maps_get_dispatch_status(
    ngx_wavm_instance_t *instance, ngx_proxy_wasm_map_type_e map_type);
#endif


static ngx_proxy_wasm_maps_key_t  ngx_proxy_wasm_maps_special_keys[] = {

#ifdef NGX_WASM_HTTP
    /* request */

    { ngx_string(":path"),
      NGX_PROXY_WASM_MAP_HTTP_REQUEST_HEADERS,
      ngx_proxy_wasm_maps_get_path,
      ngx_proxy_wasm_maps_set_path },

    { ngx_string(":method"),
      NGX_PROXY_WASM_MAP_HTTP_REQUEST_HEADERS,
      ngx_proxy_wasm_maps_get_method,
      ngx_proxy_wasm_maps_set_method },

    { ngx_string(":scheme"),
      NGX_PROXY_WASM_MAP_HTTP_REQUEST_HEADERS,
      ngx_proxy_wasm_maps_get_scheme,
      NULL },

    { ngx_string(":authority"),
      NGX_PROXY_WASM_MAP_HTTP_REQUEST_HEADERS,
      ngx_proxy_wasm_maps_get_authority,
      NULL },

    /* response */

    { ngx_string(":status"),
      NGX_PROXY_WASM_MAP_HTTP_RESPONSE_HEADERS,
      ngx_proxy_wasm_maps_get_response_status,
      NULL },

    /* dispatch response */

    { ngx_string(":status"),
      NGX_PROXY_WASM_MAP_HTTP_CALL_RESPONSE_HEADERS,
      ngx_proxy_wasm_maps_get_dispatch_status,
      NULL },
#endif

    { ngx_null_string, 0, NULL, NULL }

};


static ngx_list_t *
ngx_proxy_wasm_maps_get_map(ngx_wavm_instance_t *instance,
    ngx_proxy_wasm_map_type_e map_type)
{
#ifdef NGX_WASM_HTTP
    ngx_http_wasm_req_ctx_t         *rctx;
    ngx_http_request_t              *r;
    ngx_proxy_wasm_exec_t           *pwexec;
    ngx_http_proxy_wasm_dispatch_t  *call;
    ngx_wasm_http_reader_ctx_t      *reader;
#endif

    switch (map_type) {

#ifdef NGX_WASM_HTTP
    case NGX_PROXY_WASM_MAP_HTTP_REQUEST_HEADERS:
        rctx = ngx_http_proxy_wasm_get_rctx(instance);
        r = rctx->r;
        return &r->headers_in.headers;

    case NGX_PROXY_WASM_MAP_HTTP_RESPONSE_HEADERS:
        rctx = ngx_http_proxy_wasm_get_rctx(instance);
        r = rctx->r;
        return &r->headers_out.headers;

    case NGX_PROXY_WASM_MAP_HTTP_CALL_RESPONSE_HEADERS:
        pwexec = ngx_proxy_wasm_instance2pwexec(instance);
        call = pwexec->call;
        if (call == NULL) {
            return NULL;
        }

        reader = &call->http_reader;

        return &reader->fake_r.upstream->headers_in.headers;
#endif

    default:
        ngx_wavm_log_error(NGX_LOG_WASM_NYI, instance->log, NULL,
                           "NYI - get_map bad map_type: %d", map_type);
        return NULL;

    }
}


ngx_list_t *
ngx_proxy_wasm_maps_get_all(ngx_wavm_instance_t *instance,
    ngx_proxy_wasm_map_type_e map_type, ngx_array_t *extras)
{
    size_t                      i;
    ngx_list_t                 *list;
    ngx_str_t                  *value;
    ngx_table_elt_t            *elt;
    ngx_proxy_wasm_maps_key_t  *mkey;
#ifdef NGX_WASM_HTTP
    ngx_table_elt_t            *shim;
    ngx_array_t                *shims;
#endif

    list = ngx_proxy_wasm_maps_get_map(instance, map_type);
    if (list == NULL) {
        return NULL;
    }

    if (extras) {
        for (i = 0; ngx_proxy_wasm_maps_special_keys[i].key.len; i++) {
            mkey = &ngx_proxy_wasm_maps_special_keys[i];

            ngx_wasm_assert(mkey->get_);

            if (map_type != mkey->map_type) {
                continue;
            }

            value = mkey->get_(instance, map_type);

            if (value == NULL || !value->len) {
                continue;
            }

            elt = ngx_array_push(extras);
            if (elt == NULL) {
                return NULL;
            }

            elt->hash = 0;
            elt->key = mkey->key;
            elt->value = *value;
            elt->lowcase_key = NULL;
        }

#ifdef NGX_WASM_HTTP
        if (map_type == NGX_PROXY_WASM_MAP_HTTP_RESPONSE_HEADERS) {
            /* inject shim response headers
             * (produced by ngx_http_header_filter)
             */
            shims = ngx_http_wasm_get_shim_headers(
                        ngx_http_proxy_wasm_get_rctx(instance));

            shim = shims->elts;

            for (i = 0; i < shims->nelts; i++) {
                elt = ngx_array_push(extras);
                if (elt == NULL) {
                    return NULL;
                }

                elt->hash = 0;
                elt->key = shim[i].key;
                elt->value = shim[i].value;
                elt->lowcase_key = NULL;
            }
        }
#endif
    }

    return list;
}


ngx_int_t
ngx_proxy_wasm_maps_set_all(ngx_wavm_instance_t *instance,
    ngx_proxy_wasm_map_type_e map_type, ngx_array_t *pairs)
{
    size_t            i;
    ngx_int_t         rc;
    ngx_table_elt_t  *elt;

    for (i = 0; i < pairs->nelts; i++) {
        elt = &((ngx_table_elt_t *) pairs->elts)[i];

        rc = ngx_proxy_wasm_maps_set(instance, map_type,
                                     &elt->key, &elt->value,
                                     NGX_PROXY_WASM_MAP_SET);
        if (rc != NGX_OK) {
            return rc;
        }
    }

    return NGX_OK;
}


ngx_str_t *
ngx_proxy_wasm_maps_get(ngx_wavm_instance_t *instance,
    ngx_proxy_wasm_map_type_e map_type, ngx_str_t *key)
{
    ngx_str_t                *value;
    ngx_list_t               *list;
#ifdef NGX_WASM_HTTP
    ngx_http_wasm_req_ctx_t  *rctx;

    rctx = ngx_http_proxy_wasm_get_rctx(instance);
#endif

    /* special keys lookup */

    value = ngx_proxy_wasm_maps_get_special_key(instance, map_type, key);
    if (value) {
        goto found;
    }

    list = ngx_proxy_wasm_maps_get_map(instance, map_type);
    if (list == NULL) {
        return NULL;
    }

    /* key lookup */

    value = ngx_wasm_get_list_elem(list, key->data, key->len);
    if (value) {
        goto found;
    }

#ifdef NGX_WASM_HTTP
    if (map_type == NGX_PROXY_WASM_MAP_HTTP_RESPONSE_HEADERS) {
        /* shim header lookup */

        value = ngx_http_wasm_get_shim_header(rctx, key->data, key->len);
        if (value) {
            goto found;
        }
    }
#endif

    return NULL;

found:

    ngx_wasm_assert(value);

    return value;
}


ngx_int_t
ngx_proxy_wasm_maps_set(ngx_wavm_instance_t *instance,
    ngx_proxy_wasm_map_type_e map_type, ngx_str_t *key, ngx_str_t *value,
    ngx_uint_t map_op)
{
    ngx_int_t                 rc = NGX_ERROR;
#ifdef NGX_WASM_HTTP
    ngx_proxy_wasm_exec_t    *pwexec = ngx_proxy_wasm_instance2pwexec(instance);
    ngx_proxy_wasm_ctx_t     *pwctx = pwexec->parent;
    ngx_str_t                 skey, svalue;
    ngx_uint_t                mode = NGX_HTTP_WASM_HEADERS_SET;
    ngx_http_wasm_req_ctx_t  *rctx;
    ngx_http_request_t       *r;

    rctx = ngx_http_proxy_wasm_get_rctx(instance);
    r = rctx->r;

    switch (map_op) {

    case NGX_PROXY_WASM_MAP_SET:
        mode = NGX_HTTP_WASM_HEADERS_SET;
        break;

    case NGX_PROXY_WASM_MAP_ADD:
        mode = NGX_HTTP_WASM_HEADERS_APPEND;
        break;

    case NGX_PROXY_WASM_MAP_REMOVE:
        mode = NGX_HTTP_WASM_HEADERS_REMOVE;
        break;

    }

    if (map_op == NGX_PROXY_WASM_MAP_SET
        || map_op == NGX_PROXY_WASM_MAP_ADD)
    {
        skey.len = key->len;
        skey.data = ngx_pstrdup(pwctx->pool, key);
        if (skey.data == NULL) {
            return NGX_ERROR;
        }

        svalue.len = value->len;
        svalue.data = ngx_pnalloc(pwctx->pool, value->len + 1);
        if (svalue.data == NULL) {
            return NGX_ERROR;
        }

        ngx_memcpy(svalue.data, value->data, value->len);
        svalue.data[value->len] = '\0';

        key = &skey;
        value = &svalue;
    }
#endif

    rc = ngx_proxy_wasm_maps_set_special_key(instance, map_type, key, value);

    switch (rc) {
    case NGX_OK:
    case NGX_ERROR:
        goto done;

    case NGX_ABORT:
        /* read-only */
        ngx_wavm_log_error(NGX_LOG_ERR, instance->log, NULL,
                           "cannot set read-only \"%V\" header", key);
        goto done;

    case NGX_DECLINED:
        /* not found */
        break;

    default:
        ngx_wasm_assert(0);
        return NGX_ERROR;
    }

    ngx_wasm_assert(rc == NGX_DECLINED);

    switch (map_type) {

#ifdef NGX_WASM_HTTP
    case NGX_PROXY_WASM_MAP_HTTP_REQUEST_HEADERS:
        rc = ngx_http_wasm_set_req_header(r, key, value, mode);
        if (rc == NGX_DECLINED) {
            /* bad header value, error logged */
            rc = NGX_OK;
        }

        break;

    case NGX_PROXY_WASM_MAP_HTTP_RESPONSE_HEADERS:
        rc = ngx_http_wasm_set_resp_header(r, key, value, mode);
        if (rc == NGX_DECLINED) {
            /* bad header value, error logged */
            rc = NGX_OK;
        }

        break;
#endif

    default:
        ngx_wasm_assert(0);
        return NGX_ERROR;

    }

done:

    return rc;
}


static ngx_str_t *
ngx_proxy_wasm_maps_get_special_key(ngx_wavm_instance_t *instance,
    ngx_uint_t map_type, ngx_str_t *key)
{
    size_t                      i;
    ngx_proxy_wasm_maps_key_t  *mkey;

    for (i = 0; ngx_proxy_wasm_maps_special_keys[i].key.len; i++) {
        mkey = &ngx_proxy_wasm_maps_special_keys[i];

        dd("key: %.*s", (int) mkey->key.len, mkey->key.data);

        ngx_wasm_assert(mkey->get_);

        if (map_type != mkey->map_type
            || !ngx_str_eq(mkey->key.data, mkey->key.len, key->data, key->len))
        {
            continue;
        }

        return mkey->get_(instance, map_type);
    }

    return NULL;
}


ngx_int_t
ngx_proxy_wasm_maps_set_special_key(ngx_wavm_instance_t *instance,
    ngx_uint_t map_type, ngx_str_t *key, ngx_str_t *value)
{
    size_t                      i;
    ngx_proxy_wasm_maps_key_t  *mkey;

    for (i = 0; ngx_proxy_wasm_maps_special_keys[i].key.len; i++) {
        mkey = &ngx_proxy_wasm_maps_special_keys[i];

        if (map_type != mkey->map_type
            || !ngx_str_eq(mkey->key.data, mkey->key.len, key->data, key->len))
        {
            continue;
        }

        if (mkey->set_ == NULL) {
            return NGX_ABORT;
        }

        return mkey->set_(instance, value, map_type);
    }

    return NGX_DECLINED;
}


#ifdef NGX_WASM_HTTP
static ngx_str_t *
ngx_proxy_wasm_maps_get_path(ngx_wavm_instance_t *instance,
    ngx_proxy_wasm_map_type_e map_type)
{
    size_t                    len;
    u_char                   *p;
    ngx_proxy_wasm_exec_t    *pwexec = ngx_proxy_wasm_instance2pwexec(instance);
    ngx_proxy_wasm_ctx_t     *pwctx = pwexec->parent;
    ngx_http_wasm_req_ctx_t  *rctx = ngx_http_proxy_wasm_get_rctx(instance);
    ngx_http_request_t       *r = rctx->r;

    if (pwctx->path.len) {
        return &pwctx->path;
    }

    len = r->uri.len;

    if (r->args.len) {
        len += r->args.len + 1;
    }

    pwctx->path.data = ngx_pnalloc(pwctx->pool, len);
    if (pwctx->path.data == NULL) {
        return NULL;
    }

    p = ngx_cpymem(pwctx->path.data, r->uri.data, r->uri.len);

    if (r->args.len) {
        *p++ = '?';
        ngx_memcpy(p, r->args.data, r->args.len);
    }

    pwctx->path.len = len;

    return &pwctx->path;
}


static ngx_int_t
ngx_proxy_wasm_maps_set_path(ngx_wavm_instance_t *instance, ngx_str_t *value,
    ngx_proxy_wasm_map_type_e map_type)
{
    ngx_proxy_wasm_exec_t    *pwexec = ngx_proxy_wasm_instance2pwexec(instance);
    ngx_proxy_wasm_ctx_t     *pwctx = pwexec->parent;
    ngx_http_wasm_req_ctx_t  *rctx = ngx_http_proxy_wasm_get_rctx(instance);
    ngx_http_request_t       *r = rctx->r;

    if (ngx_strchr(value->data, '?')) {
        ngx_wavm_instance_trap_printf(instance,
                                      "NYI - cannot set request path "
                                      "with querystring");
        return NGX_ERROR;
    }

    r->uri.len = value->len;
    r->uri.data = value->data;

    if (pwctx->path.len) {
        ngx_pfree(pwctx->pool, pwctx->path.data);
        pwctx->path.len = 0;
    }

    return NGX_OK;
}


static ngx_str_t *
ngx_proxy_wasm_maps_get_method(ngx_wavm_instance_t *instance,
    ngx_proxy_wasm_map_type_e map_type)
{
    ngx_http_wasm_req_ctx_t  *rctx = ngx_http_proxy_wasm_get_rctx(instance);
    ngx_http_request_t       *r = rctx->r;

    return &r->method_name;
}


static ngx_int_t
ngx_proxy_wasm_maps_set_method(ngx_wavm_instance_t *instance, ngx_str_t *value,
    ngx_proxy_wasm_map_type_e map_type)
{
    ngx_http_wasm_req_ctx_t  *rctx = ngx_http_proxy_wasm_get_rctx(instance);
    ngx_http_request_t       *r = rctx->r;

    r->method_name.len = value->len;
    r->method_name.data = value->data;

    return NGX_OK;
}


static ngx_str_t *
ngx_proxy_wasm_maps_get_scheme(ngx_wavm_instance_t *instance,
    ngx_proxy_wasm_map_type_e map_type)
{
    u_char                     *p;
    ngx_uint_t                  hash;
    ngx_http_variable_value_t  *vv;
    ngx_proxy_wasm_exec_t      *pwexec;
    ngx_proxy_wasm_ctx_t       *pwctx;
    ngx_http_wasm_req_ctx_t    *rctx;
    ngx_http_request_t         *r;
    static ngx_str_t            name = ngx_string("scheme");

    pwexec = ngx_proxy_wasm_instance2pwexec(instance);
    rctx = ngx_http_proxy_wasm_get_rctx(instance);
    pwctx = pwexec->parent;
    r = rctx->r;

    if (pwctx->scheme.len) {
        return &pwctx->scheme;
    }

    p = ngx_palloc(pwctx->pool, name.len);
    if (p == NULL) {
        return NULL;
    }

    hash = ngx_hash_strlow(p, name.data, name.len);

    vv = ngx_http_get_variable(r, &name, hash);

    ngx_pfree(pwctx->pool, p);

    if (vv == NULL || vv->not_found) {
        return NULL;
    }

    pwctx->scheme.data = vv->data;
    pwctx->scheme.len = vv->len;

    return &pwctx->scheme;
}


static ngx_str_t *
ngx_proxy_wasm_maps_get_authority(ngx_wavm_instance_t *instance,
    ngx_proxy_wasm_map_type_e map_type)
{
    u_char                    *p;
    ngx_uint_t                 port;
    ngx_str_t                 *server_name;
    ngx_proxy_wasm_exec_t     *pwexec;
    ngx_proxy_wasm_ctx_t      *pwctx;
    ngx_http_core_srv_conf_t  *cscf;
    ngx_http_wasm_req_ctx_t   *rctx;
    ngx_http_request_t        *r;

    pwexec = ngx_proxy_wasm_instance2pwexec(instance);
    rctx = ngx_http_proxy_wasm_get_rctx(instance);
    pwctx = pwexec->parent;
    r = rctx->r;

    if (pwctx->authority.len) {
        return &pwctx->authority;
    }

    if (ngx_connection_local_sockaddr(r->connection, NULL, 0)
        != NGX_OK)
    {
        return NULL;
    }

    cscf = ngx_http_get_module_srv_conf(r, ngx_http_core_module);
    server_name = &cscf->server_name;
    if (!server_name->len) {
        server_name = (ngx_str_t *) &ngx_cycle->hostname;
    }

    pwctx->authority.len = server_name->len;

    port = ngx_inet_get_port(r->connection->local_sockaddr);
    if (port && port < 65536) {
        pwctx->authority.len += 1 + sizeof("65535") - 1; /* ':' */
    }

    pwctx->authority.data = ngx_pnalloc(pwctx->pool, pwctx->authority.len);
    if (pwctx->authority.data == NULL) {
        return NULL;
    }

    p = ngx_sprintf(pwctx->authority.data, "%V", server_name);

    if (port && port < 65536) {
        pwctx->authority.len = ngx_sprintf(p, ":%ui", port)
                               - pwctx->authority.data;
    }

    return &pwctx->authority;
}


static ngx_str_t *
ngx_proxy_wasm_maps_get_response_status(ngx_wavm_instance_t *instance,
    ngx_proxy_wasm_map_type_e map_type)
{
    ngx_uint_t                status;
    ngx_proxy_wasm_ctx_t     *pwctx;
    ngx_proxy_wasm_exec_t    *pwexec;
    ngx_http_wasm_req_ctx_t  *rctx;
    ngx_http_request_t       *r;

    ngx_wasm_assert(map_type == NGX_PROXY_WASM_MAP_HTTP_RESPONSE_HEADERS);

    pwexec = ngx_proxy_wasm_instance2pwexec(instance);
    rctx = ngx_http_proxy_wasm_get_rctx(instance);
    pwctx = pwexec->parent;
    r = rctx->r;

    /* status */

    if (r->err_status) {
        status = r->err_status;

    } else if (r->headers_out.status) {
        status = r->headers_out.status;

    } else if (r->http_version == NGX_HTTP_VERSION_9) {
        status = 9;

    } else {
        return NULL;
    }

    /* update cached value */

    if (status != pwctx->response_code) {
        pwctx->response_code = status;

        if (pwctx->response_status.len) {
            ngx_pfree(pwctx->pool, pwctx->response_status.data);
            pwctx->response_status.len = 0;
        }
    }

    /* format */

    if (!pwctx->response_status.len) {
        pwctx->response_status.data = ngx_pnalloc(pwctx->pool, NGX_INT_T_LEN);
        if (pwctx->response_status.data == NULL) {
            return NULL;
        }

        pwctx->response_status.len = ngx_sprintf(pwctx->response_status.data,
                                                 "%03ui", pwctx->response_code)
                                     - pwctx->response_status.data;
    }

    ngx_wasm_assert(pwctx->response_status.len);

    return &pwctx->response_status;
}


static ngx_str_t *
ngx_proxy_wasm_maps_get_dispatch_status(ngx_wavm_instance_t *instance,
    ngx_proxy_wasm_map_type_e map_type)
{
    ngx_uint_t                       status;
    ngx_proxy_wasm_ctx_t            *pwctx;
    ngx_proxy_wasm_exec_t           *pwexec;
    ngx_http_proxy_wasm_dispatch_t  *call;
    ngx_wasm_http_reader_ctx_t      *reader;

    ngx_wasm_assert(map_type == NGX_PROXY_WASM_MAP_HTTP_CALL_RESPONSE_HEADERS);

    pwexec = ngx_proxy_wasm_instance2pwexec(instance);
    pwctx = pwexec->parent;
    call = pwexec->call;
    reader = &call->http_reader;

    /* status */

    ngx_wasm_assert(reader->fake_r.upstream);

    if (reader->fake_r.upstream == NULL) {
        return NULL;
    }

    status = reader->fake_r.upstream->headers_in.status_n;
    if (!status) {
        return NULL;
    }

    /* update cached value */

    if (status != pwctx->call_code) {
        pwctx->call_code = status;

        if (pwctx->call_status.len) {
            ngx_pfree(pwctx->pool, pwctx->call_status.data);
            pwctx->call_status.len = 0;
        }
    }

    /* format */

    if (!pwctx->call_status.len) {
        pwctx->call_status.data = ngx_pnalloc(pwctx->pool, NGX_INT_T_LEN);
        if (pwctx->call_status.data == NULL) {
            return NULL;
        }

        pwctx->call_status.len = ngx_sprintf(pwctx->call_status.data, "%03ui",
                                             pwctx->call_code)
                                 - pwctx->call_status.data;
    }

    ngx_wasm_assert(pwctx->call_status.len);

    return &pwctx->call_status;
}
#endif
