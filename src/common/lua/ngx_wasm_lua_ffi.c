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


void
ngx_http_wasm_ffi_set_property(ngx_http_request_t *r,
    ngx_str_t *key, ngx_str_t *value, ngx_int_t *rc)
{
    ngx_http_wasm_req_ctx_t  *rctx;
    ngx_proxy_wasm_ctx_t     *pwctx;

    if (ngx_http_wasm_rctx(r, &rctx) != NGX_OK) {
        *rc = NGX_ERROR;
        return;
    }

    pwctx = ngx_proxy_wasm_ctx(NULL, 0,
                               NGX_PROXY_WASM_ISOLATION_STREAM,
                               &ngx_http_proxy_wasm, rctx);
    if (pwctx == NULL) {
        *rc = NGX_ERROR;
        return;
    }

    *rc = ngx_proxy_wasm_properties_set(pwctx, key, value);
    return;
}


void
ngx_http_wasm_ffi_get_property(ngx_http_request_t *r,
    ngx_str_t *key, ngx_str_t *value, ngx_int_t *rc)
{
    ngx_http_wasm_req_ctx_t  *rctx;
    ngx_proxy_wasm_ctx_t     *pwctx;

    if (ngx_http_wasm_rctx(r, &rctx) != NGX_OK) {
        /**
         * TODO: return code signifying "no plan attached to request" and co.
         * return associated constant as err/code from Lua lib
         */
        *rc = NGX_ERROR;
        return;
    }

    pwctx = ngx_proxy_wasm_ctx(NULL, 0,
                               NGX_PROXY_WASM_ISOLATION_STREAM,
                               &ngx_http_proxy_wasm, rctx);
    if (pwctx == NULL) {
        *rc = NGX_ERROR;
        return;
    }

    *rc = ngx_proxy_wasm_properties_get(pwctx, key, value);
    return;
}


void
ngx_wasm_lua_ffi_host_prop_handler(
    ngx_wasm_ffi_host_property_req_ctx_t *hprctx,
    unsigned char ok, const char* new_value, size_t len,
    unsigned char is_const)
{
    ngx_proxy_wasm_ctx_t  *pwctx = get_pwctx(hprctx->r);
    ngx_str_t             *key = hprctx->key;
    ngx_str_t             *value = hprctx->value;

    if (!ok) {
        if (new_value) {
            ngx_wavm_log_error(NGX_LOG_ERR, pwctx->log, NULL,
                               "error in property handler: %s",
                               new_value);
            hprctx->rc = NGX_ERROR;
            return;

        } else if (!hprctx->is_getter) {
            ngx_wavm_log_error(NGX_LOG_ERR, pwctx->log, NULL,
                               "error in property handler: unknown error");
            hprctx->rc = NGX_ERROR;
            return;
        }

        hprctx->rc = NGX_DECLINED;
        return;
    }

    if (new_value == NULL) {
        value->data = NULL;
        value->len = 0;

    } else {
        value->data = (u_char *) new_value;
        value->len = len;
    }

    hprctx->rc = ngx_proxy_wasm_properties_set_host(pwctx, key, value, is_const,
                                                    hprctx->is_getter);

    if (hprctx->is_getter && hprctx->rc == NGX_OK && value->data == NULL) {
        hprctx->rc = NGX_DECLINED;
    }

    hprctx->rc = NGX_OK;
}

static const char  *HOST_PROPERTY_SCRIPT_NAME = "wasm_lua_host_property_chunk";
static const char  *HOST_PROPERTY_SCRIPT = ""
    "local hprctx, is_getter, key, value = ...                            \n"
    "local ffi = require('ffi')                                           \n"
    "local fmt = string.format                                            \n"
    "                                                                     \n"
    "ffi.cdef[[                                                           \n"
    "    typedef unsigned char  u_char;                                   \n"
    "    typedef void           ngx_wasm_ffi_host_property_req_ctx_t;     \n"
    "                                                                     \n"
    "    void ngx_wasm_lua_ffi_host_prop_handler(                         \n"
    "        ngx_wasm_ffi_host_property_req_ctx_t *hprctx,                \n"
    "        u_char ok, const char* value, size_t len,                    \n"
    "        u_char is_const);                                            \n"
    "]]                                                                   \n"
    "                                                                     \n"
    "local proxy_wasm = require('resty.wasmx.proxy_wasm')                 \n"
    "                                                                     \n"
    "local fn_name = is_getter and 'property_getter' or 'property_setter' \n"
    "local fn = proxy_wasm[fn_name]                                       \n"
    "if not fn then                                                       \n"
    "    error('property handler function not found')                     \n"
    "end                                                                  \n"
    "                                                                     \n"
    "local pok, ok, new_value, is_const = pcall(fn, key, value)           \n"
    "if not pok then                                                      \n"
    "    ngx.log(ngx.ERR, 'Lua error in property handler: ' .. ok)        \n"
    "    error('fail')                                                    \n"
    "end                                                                  \n"
    "                                                                     \n"
    "local new_value_len = new_value and #new_value or 0                  \n"
    "                                                                     \n"
    "                                                                     \n"
    "ffi.C.ngx_wasm_lua_ffi_host_prop_handler(hprctx, ok and 1 or 0,      \n"
    "                                         new_value, new_value_len,   \n"
    "                                         is_const and 1 or 0)        \n";

static ngx_int_t
property_handler(void *data, ngx_str_t *key, ngx_str_t *value)
{
    ngx_wasm_ffi_host_property_ctx_t      *hpctx = data;
    ngx_wasm_ffi_host_property_req_ctx_t   hprctx;
    ngx_proxy_wasm_ctx_t                  *pwctx = get_pwctx(hpctx->r);
    lua_State                             *co;
    ngx_int_t                              rc = NGX_OK;
    ngx_wasm_lua_ctx_t                    *lctx;

    // FIXME if this function is changed to return NGX_AGAIN,
    // hprctx needs to be dynamically allocated:

    hprctx.r = hpctx->r;
    hprctx.is_getter = hpctx->is_getter;
    hprctx.key = key;
    hprctx.value = value;

    /* create coroutine */

    lctx = ngx_wasm_lua_thread_new(HOST_PROPERTY_SCRIPT_NAME,
                                   HOST_PROPERTY_SCRIPT,
                                   hpctx->env, pwctx->log, &hprctx,
                                   NULL, NULL);
    if (lctx == NULL) {
        return NGX_ERROR;
    }

    /* push key and value arguments */

    co = lctx->co;

    ngx_wasm_assert(key && key->data);
    ngx_wasm_assert(value);

    lctx->nargs = 4;

    lua_pushlightuserdata(co, &hprctx);
    lua_pushboolean(co, hpctx->is_getter);
    lua_pushlstring(co, (char *) key->data, key->len);

    if (hpctx->is_getter || value->data == NULL) {
        lua_pushnil(co);

    } else {
        if ((int) value->len == 0) {
            lua_pushliteral(co, "");

        } else {
            lua_pushlstring(co, (char *) value->data, value->len);
        }
    }

    /* run handler */

    rc = ngx_wasm_lua_thread_run(lctx);
    if (rc == NGX_AGAIN) {
        ngx_wavm_log_error(NGX_LOG_ERR, pwctx->log, NULL,
        "cannot yield from host property handler");

        return NGX_ERROR;
    }

    // ngx_wasm_assert(rc == NGX_DONE);

    return hprctx.rc;
}


static ngx_wasm_ffi_host_property_ctx_t *
get_hpctx(ngx_http_request_t *r, unsigned is_getter)
{
    ngx_proxy_wasm_ctx_t              *pwctx = get_pwctx(r);
    ngx_wasm_ffi_host_property_ctx_t  *hpctx;
    ngx_http_wasm_req_ctx_t           *rctx;
    ngx_int_t                          rc;

    if (pwctx == NULL) {
        return NULL;
    }

    rc = ngx_http_wasm_rctx(r, &rctx);
    ngx_wasm_assert(rc != NGX_DECLINED);
    if (rc != NGX_OK) {
        return NULL;
    }

    hpctx = ngx_palloc(pwctx->pool, sizeof(ngx_wasm_ffi_host_property_ctx_t));
    if (hpctx == NULL) {
        return NULL;
    }

    hpctx->r = r;
    hpctx->env = &rctx->env;
    hpctx->is_getter = is_getter;

    return hpctx;
}


ngx_int_t
ngx_http_wasm_ffi_enable_property_setter(ngx_http_request_t *r)
{
    ngx_proxy_wasm_ctx_t              *pwctx = get_pwctx(r);
    ngx_wasm_ffi_host_property_ctx_t  *hpctx;

    if (pwctx == NULL) {
        return NGX_ERROR;
    }

    hpctx = get_hpctx(r, 0);
    if (hpctx == NULL) {
        return NGX_ERROR;
    }

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

    hpctx = get_hpctx(r, 1);
    if (hpctx == NULL) {
        return NGX_ERROR;
    }

    return ngx_proxy_wasm_properties_set_host_prop_getter(pwctx,
                                                          property_handler,
                                                          hpctx);
}
#endif
