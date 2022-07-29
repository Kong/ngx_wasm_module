#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_proxy_wasm_properties.h>
#ifdef NGX_WASM_HTTP
#include <ngx_http_proxy_wasm.h>
#endif


static const char *ngx_prefix = "ngx\0";
static size_t ngx_prefix_len = 4;


#ifdef NGX_WASM_HTTP
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
#endif


static ngx_int_t
ngx_proxy_wasm_properties_get_ngx(ngx_wavm_instance_t *instance,
    ngx_str_t *path, ngx_str_t *value)
{
#ifdef NGX_WASM_HTTP
    ngx_uint_t                  hash;
    ngx_str_t                   name;
    ngx_http_variable_value_t  *vv;
    ngx_http_wasm_req_ctx_t    *rctx;

    name.data = (u_char *)(path->data + ngx_prefix_len);
    name.len = path->len - ngx_prefix_len;

    hash = hash_str(name.data, name.len);

    rctx = ngx_http_proxy_wasm_get_rctx(instance);
    if (!rctx) {
        return NGX_DECLINED;
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


ngx_int_t
ngx_proxy_wasm_properties_get(ngx_wavm_instance_t *instance,
    ngx_str_t *path, ngx_str_t *value)
{
    value->data = NULL;
    value->len = 0;

    if (path->len > ngx_prefix_len
        && ngx_memcmp(path->data, ngx_prefix, ngx_prefix_len) == 0)
    {
        return ngx_proxy_wasm_properties_get_ngx(instance, path, value);
    }

    return NGX_DECLINED;
}


static ngx_int_t
ngx_proxy_wasm_properties_set_ngx(ngx_wavm_instance_t *instance,
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

    rctx = ngx_http_proxy_wasm_get_rctx(instance);
    if (!rctx) {
        /* on_tick */

        ngx_wavm_log_error(NGX_LOG_ERR, instance->log, NULL,
                           "cannot get request context");

        return NGX_ERROR;
    }

    r = rctx->r;
    cmcf = ngx_http_get_module_main_conf(r, ngx_http_core_module);

    name.data = (u_char *)(path->data + ngx_prefix_len);
    name.len = path->len - ngx_prefix_len;

    hash = hash_str(name.data, name.len);

    v = ngx_hash_find(&cmcf->variables_hash, hash, name.data, name.len);
    if (!v) {
        ngx_wavm_log_error(NGX_LOG_ERR, instance->log, NULL,
                           "nginx variable '%*s' not found",
                           (int) name.len, name.data);

        return NGX_DECLINED;
    }

    if (!(v->flags & NGX_HTTP_VAR_CHANGEABLE)) {
        ngx_wavm_log_error(NGX_LOG_ERR, instance->log, NULL,
                           "variable '%*s' is not changeable",
                           (int) name.len, name.data);

        return NGX_ERROR;
    }

    if (v->set_handler) {
        vv = ngx_pcalloc(r->pool, sizeof(ngx_http_variable_value_t)
                         + value->len);
        if (!vv) {
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
        vv = &r->variables[v->index];

        if (value->data == NULL) {
            vv->valid = 0;
            vv->not_found = 1;
            vv->no_cacheable = 0;
            vv->data = NULL;
            vv->len = 0;

            return NGX_OK;
        }

        p = ngx_palloc(r->pool, value->len);
        if (!p) {
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


ngx_int_t
ngx_proxy_wasm_properties_set(ngx_wavm_instance_t *instance,
    ngx_str_t *path, ngx_str_t *value)
{
    ngx_int_t   rc;
    u_char     *p;
    size_t      i;

    rc = NGX_DECLINED;

    if (path->len > ngx_prefix_len
        && ngx_memcmp(path->data, ngx_prefix, ngx_prefix_len) == 0)
    {
        rc = ngx_proxy_wasm_properties_set_ngx(instance, path, value);
    }

    if (rc == NGX_DECLINED) {
        p = ngx_palloc(instance->pool, path->len);
        if (p == NULL) {
            return NGX_ERROR;
        }

        ngx_memcpy(p, path->data, path->len);

        for (i = 0; i < path->len; i++) {
            if (p[i] == '\0')
                p[i] = '.';
        }

        ngx_wavm_log_error(NGX_LOG_ERR, instance->log, NULL,
                           "property '%*s' not found",
                           (int) path->len, p);

        ngx_pfree(instance->pool, p);
    }

    return rc;
}
