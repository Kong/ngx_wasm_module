#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_wasm_lua.h>
#include <ngx_wasm_lua_ffi.h>


ngx_wavm_t *
ngx_wasm_ffi_main_vm()
{
    ngx_wavm_t  *vm;

    vm = ngx_wasm_main_vm((ngx_cycle_t *) ngx_cycle);

    dd("vm: %p", vm);

    return vm;
}


#ifdef NGX_WASM_HTTP
ngx_int_t
ngx_http_wasm_ffi_plan_new(ngx_wavm_t *vm,
    ngx_wasm_ffi_filter_t *filters, size_t n_filters,
    ngx_wasm_ops_plan_t **out, u_char *err, size_t *errlen)
{
    size_t                      i;
    ngx_int_t                   rc;
    ngx_wasm_ops_plan_t        *plan;
    ngx_wasm_ffi_filter_t      *ffi_filter;
    ngx_http_wasm_main_conf_t  *mcf;

    mcf = ngx_http_cycle_get_module_main_conf(ngx_cycle, ngx_http_wasm_module);
    if (mcf == NULL) {
        /* no http{} block */
        *errlen = ngx_snprintf(err, *errlen, "%V", &NGX_WASM_STR_NO_HTTP)
                  - err;
        return NGX_ERROR;
    }

    plan = ngx_wasm_ops_plan_new(vm->pool, &ngx_http_wasm_subsystem);
    if (plan == NULL) {
        return NGX_ERROR;
    }

    for (i = 0; i < n_filters; i++) {
        ffi_filter = filters + i;

        dd("filter[%ld] name: \"%.*s\" (config: \"%.*s\")", i,
           (int) ffi_filter->name->len, ffi_filter->name->data,
           (int) ffi_filter->config->len, ffi_filter->config->data);

        rc = ngx_http_wasm_ops_add_filter(plan,
                                          ffi_filter->name,
                                          ffi_filter->config,
                                          &mcf->store, vm);
        if (rc != NGX_OK) {
            if (rc == NGX_ABORT) {
                *errlen = ngx_snprintf(err, *errlen,
                                       "no \"%V\" module defined",
                                       ffi_filter->name) - err;
            }

            return NGX_ERROR;
        }
    }

    *out = plan;

    return NGX_OK;
}


void
ngx_http_wasm_ffi_plan_free(ngx_wasm_ops_plan_t *plan)
{
    ngx_wasm_ops_plan_destroy(plan);
}


ngx_int_t
ngx_http_wasm_ffi_plan_load(ngx_wasm_ops_plan_t *plan)
{
    if (ngx_wasm_ops_plan_load(plan,
                               (ngx_log_t *) &ngx_cycle->new_log)
        != NGX_OK)
    {
        return NGX_ERROR;
    }

    return ngx_proxy_wasm_start((ngx_cycle_t *) ngx_cycle);
}


ngx_int_t
ngx_http_wasm_ffi_plan_attach(ngx_http_request_t *r, ngx_wasm_ops_plan_t *plan,
    ngx_uint_t isolation)
{
    ngx_int_t                  rc;
    ngx_http_wasm_req_ctx_t   *rctx;
    ngx_http_wasm_loc_conf_t  *loc;
    ngx_wasm_ops_plan_t       *old_plan;

    if (!plan->loaded) {
        return NGX_DECLINED;
    }

    loc = ngx_http_get_module_loc_conf(r, ngx_http_wasm_module);

    old_plan = loc->plan;
    loc->plan = plan;

    rc = ngx_http_wasm_rctx(r, &rctx);
    ngx_wasm_assert(rc != NGX_DECLINED);
    if (rc != NGX_OK) {
        return NGX_ERROR;
    }

    loc->plan = old_plan;

    if (rctx->ffi_attached) {
        return NGX_ABORT;
    }

    rctx->ffi_attached = 1;
    rctx->opctx.ctx.proxy_wasm.isolation = isolation;

    return NGX_OK;
}


ngx_int_t
ngx_http_wasm_ffi_start(ngx_http_request_t *r)
{
    ngx_int_t                  rc, phase;
    ngx_http_wasm_req_ctx_t   *rctx;
    ngx_http_wasm_loc_conf_t  *loc;

    rc = ngx_http_wasm_rctx(r, &rctx);
    if (rc != NGX_OK) {
        return rc;
    }

#if 1
    ngx_wasm_assert(rctx->ffi_attached);
#else
    /*
     * presently, the above rctx rc is already NGX_DECLINED
     * since loc->plan is empty
     */
    if (!rctx->ffi_attached) {
        ngx_wasm_assert(0);
        return NGX_DECLINED;
    }
#endif

    loc = ngx_http_get_module_loc_conf(r, ngx_http_wasm_module);

    phase = (loc->pwm_req_headers_in_access == 1)
            ? NGX_HTTP_ACCESS_PHASE
            : NGX_HTTP_REWRITE_PHASE;

    rc = ngx_wasm_ops_resume(&rctx->opctx, phase);
    if (rc == NGX_AGAIN) {
        ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                       "wasm \"ffi_start\" yield");

        ngx_wasm_yield(&rctx->env);
    }

    /* ignore errors: resume could trap, but FFI call succeeded */

    ngx_wasm_assert(rc == NGX_OK
                    || rc == NGX_DONE
                    || rc == NGX_AGAIN
                    || rc >= NGX_HTTP_SPECIAL_RESPONSE);

    return NGX_OK;
}


static ngx_inline ngx_proxy_wasm_ctx_t *
get_pwctx(ngx_http_request_t *r)
{
    ngx_http_wasm_req_ctx_t  *rctx;

    if (ngx_http_wasm_rctx(r, &rctx) != NGX_OK) {
        return NULL;
    }

    return ngx_proxy_wasm_ctx(NULL, 0,
                              NGX_PROXY_WASM_ISOLATION_STREAM,
                              &ngx_http_proxy_wasm, rctx);
}


ngx_int_t
ngx_http_wasm_ffi_set_property(ngx_http_request_t *r,
    ngx_str_t *key, ngx_str_t *value)
{
    ngx_http_wasm_req_ctx_t  *rctx;
    ngx_proxy_wasm_ctx_t     *pwctx;

    if (ngx_http_wasm_rctx(r, &rctx) != NGX_OK) {
        return NGX_ERROR;
    }

    pwctx = ngx_proxy_wasm_ctx(NULL, 0,
                               NGX_PROXY_WASM_ISOLATION_STREAM,
                               &ngx_http_proxy_wasm, rctx);
    if (pwctx == NULL) {
        return NGX_ERROR;
    }

    return ngx_proxy_wasm_properties_set(pwctx, key, value);
}


ngx_int_t
ngx_http_wasm_ffi_get_property(ngx_http_request_t *r,
    ngx_str_t *key, ngx_str_t *value)
{
    ngx_http_wasm_req_ctx_t  *rctx;
    ngx_proxy_wasm_ctx_t     *pwctx;

    if (ngx_http_wasm_rctx(r, &rctx) != NGX_OK) {
        /**
         * TODO: return code signifying "no plan attached to request" and co.
         * return associated constant as err/code from Lua lib
         */
        return NGX_ERROR;
    }

    pwctx = ngx_proxy_wasm_ctx(NULL, 0,
                               NGX_PROXY_WASM_ISOLATION_STREAM,
                               &ngx_http_proxy_wasm, rctx);
    if (pwctx == NULL) {
        return NGX_ERROR;
    }

    return ngx_proxy_wasm_properties_get(pwctx, key, value);
}


static ngx_int_t
property_handler(void *data, ngx_str_t *key, ngx_str_t *value)
{
    ngx_wasm_ffi_host_property_ctx_t  *hpctx = data;
    ngx_proxy_wasm_ctx_t              *pwctx = get_pwctx(hpctx->r);
    lua_State                         *L = hpctx->L;
    ngx_int_t                          rc;
    unsigned                           is_const;
    int                                pok;
    int                                top;

    top = lua_gettop(L);

    lua_getglobal(L, "package");
    lua_getfield(L, -1, "loaded");
    lua_getfield(L, -1, "resty.wasmx.proxy_wasm");
    if (lua_type(L, -1) != LUA_TTABLE) {
        goto fail;
    }

    lua_getfield(L, -1,
                 hpctx->is_getter ? "property_getter" : "property_setter");
    if (lua_type(L, -1) != LUA_TFUNCTION) {
        goto fail;
    }

    ngx_wasm_assert(key && key->data);

    lua_pushlstring(L, (char *) key->data, key->len);

    ngx_wasm_assert(value);

    if (value->data == NULL) {
        lua_pushnil(L);

    } else {
        if ((int) value->len == 0) {
            lua_pushliteral(L, "");

        } else {
            lua_pushlstring(L, (char *) value->data, value->len);
        }
    }

    pok = lua_pcall(L, 2, 3, 0);
    if (pok != 0) {
        ngx_wavm_log_error(NGX_LOG_ERR, pwctx->log, NULL,
                           "Lua error in property handler: %s",
                           lua_tostring(L, -1));
        goto fail;
    }

    if (lua_toboolean(L, -3) == 0) {
        if (lua_isstring(L, -2)) {
            ngx_wavm_log_error(NGX_LOG_ERR, pwctx->log, NULL,
                               "error in property handler: %s",
                               lua_tostring(L, -2));
            goto fail;

        } else if (!hpctx->is_getter) {
            ngx_wavm_log_error(NGX_LOG_ERR, pwctx->log, NULL,
                               "error in property handler: unknown error");
            goto fail;

        }

        return NGX_DECLINED;
    }

    if (lua_isnil(L, -2)) {
        value->data = NULL;
        value->len = 0;

    } else {
        value->data = (u_char *) lua_tolstring(L, -2, &value->len);
    }

    is_const = lua_toboolean(L, -1);

    if (pwctx == NULL) {
        return NGX_ERROR;
    }

    rc = ngx_proxy_wasm_properties_set_host(pwctx, key, value, is_const,
                                            hpctx->is_getter);

    lua_settop(L, top);

    if (hpctx->is_getter && rc == NGX_OK && value->data == NULL) {
        return NGX_DECLINED;
    }

    return rc;

fail:

    lua_settop(L, top);
    return NGX_ERROR;
}


ngx_int_t
ngx_http_wasm_ffi_enable_property_setter(ngx_http_request_t *r)
{
    ngx_proxy_wasm_ctx_t              *pwctx = get_pwctx(r);
    ngx_wasm_ffi_host_property_ctx_t  *hpctx;

    if (pwctx == NULL) {
        return NGX_ERROR;
    }

    hpctx = ngx_palloc(pwctx->pool, sizeof(ngx_wasm_ffi_host_property_ctx_t));
    if (hpctx == NULL) {
        return NGX_ERROR;
    }

    hpctx->r = r;
    hpctx->L = ngx_http_lua_get_lua_vm(r, NULL);
    hpctx->is_getter = 0;

    return ngx_proxy_wasm_properties_set_host_prop_setter(pwctx,
                                                          property_handler,
                                                          hpctx);
}


ngx_int_t
ngx_http_wasm_ffi_enable_property_getter(ngx_http_request_t *r)
{
    ngx_proxy_wasm_ctx_t              *pwctx = get_pwctx(r);
    ngx_wasm_ffi_host_property_ctx_t  *hpctx;

    if (pwctx == NULL) {
        return NGX_ERROR;
    }

    hpctx = ngx_palloc(pwctx->pool, sizeof(ngx_wasm_ffi_host_property_ctx_t));
    if (hpctx == NULL) {
        return NGX_ERROR;
    }

    hpctx->r = r;
    hpctx->L = ngx_http_lua_get_lua_vm(r, NULL);
    hpctx->is_getter = 1;

    return ngx_proxy_wasm_properties_set_host_prop_getter(pwctx,
                                                          property_handler,
                                                          hpctx);
}
#endif
