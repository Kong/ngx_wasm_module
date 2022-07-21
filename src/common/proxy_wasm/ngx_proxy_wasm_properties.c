#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_proxy_wasm_properties.h>
#ifdef NGX_WASM_HTTP
#include <ngx_http_proxy_wasm.h>
#endif


static const char* ngx_prefix = "ngx\0";
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
