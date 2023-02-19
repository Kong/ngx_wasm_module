#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_wavm.h>
#ifdef NGX_WASM_LUA_BRIDGE_TESTS
#include <ngx_wasm_lua.h>
#endif


ngx_int_t
ngx_wasm_hfuncs_log(ngx_wavm_instance_t *instance,
    wasm_val_t args[], wasm_val_t rets[])
{
    uint32_t         level, len;
    ngx_wavm_ptr_t  *msg;

    level = args[0].of.i32;
    len = args[2].of.i32;
    msg = NGX_WAVM_HOST_LIFT_SLICE(instance, args[1].of.i32, len);

    ngx_log_error((ngx_uint_t) level, instance->log, 0, "%*s", len, msg);

    return NGX_WAVM_OK;
}


#ifdef NGX_WASM_LUA_BRIDGE_TESTS
ngx_int_t
ngx_wasm_hfuncs_test_lua_argsrets(ngx_wavm_instance_t *instance,
    wasm_val_t args[], wasm_val_t rets[])
{
    ngx_wasm_subsys_env_t     env;
    ngx_wasm_lua_ctx_t       *lctx;
#if (NGX_WASM_HTTP)
    ngx_http_wasm_req_ctx_t  *rctx = instance->data;
#endif
    static const char        *SCRIPT_NAME = "argsrets_lua_chunk";
    static const char        *SCRIPT = "local arg = ...\n"
                                       "ngx.log(ngx.INFO, 'arg: ', arg)\n";
                                       "return 123, 456";

    ngx_memzero(&env, sizeof(ngx_wasm_subsys_env_t));

#if (NGX_WASM_HTTP)
    env.connection = rctx->r->connection;
    env.buf_tag = buf_tag;
    env.subsys = &ngx_http_wasm_subsystem;
    env.ctx.rctx = rctx;
#endif

    lctx = ngx_wasm_lua_thread_new(SCRIPT_NAME,
                                   SCRIPT,
                                   &env,
                                   instance->log,
                                   NULL);
    if (lctx == NULL) {
        return NGX_WAVM_ERROR;
    }

    lctx->nargs = 1;
    lua_pushstring(lctx->co, "argument");

    (void) ngx_wasm_lua_thread_run(lctx);

    ngx_wasm_lua_thread_destroy(lctx);

    return NGX_WAVM_OK;
}


ngx_int_t
ngx_wasm_hfuncs_test_lua_bad_chunk(ngx_wavm_instance_t *instance,
    wasm_val_t args[], wasm_val_t rets[])
{
    ngx_int_t                 rc;
    ngx_wasm_subsys_env_t     env;
    ngx_wasm_lua_ctx_t       *lctx;
#if (NGX_WASM_HTTP)
    ngx_http_wasm_req_ctx_t  *rctx = instance->data;
#endif
    static const char        *SCRIPT_NAME = "bad_lua_chunk";
    static const char        *SCRIPT = "local x = {";

    ngx_memzero(&env, sizeof(ngx_wasm_subsys_env_t));

#if (NGX_WASM_HTTP)
    env.connection = rctx->r->connection;
    env.buf_tag = buf_tag;
    env.subsys = &ngx_http_wasm_subsystem;
    env.ctx.rctx = rctx;
#endif

    lctx = ngx_wasm_lua_thread_new(SCRIPT_NAME,
                                   SCRIPT,
                                   &env,
                                   instance->log,
                                   NULL);
    if (lctx == NULL) {
        return NGX_WAVM_ERROR;
    }

    rc = ngx_wasm_lua_thread_run(lctx);

    ngx_log_debug1(NGX_LOG_DEBUG_WASM, instance->log, 0,
                   "lua thread rc: %d", rc);

    ngx_wasm_lua_thread_destroy(lctx);

    return NGX_WAVM_OK;
}
#endif


static ngx_wavm_host_func_def_t  ngx_wasm_core_hfuncs[] = {

    { ngx_string("ngx_log"),
      &ngx_wasm_hfuncs_log,
      ngx_wavm_arity_i32x3,
      NULL },

#ifdef NGX_WASM_LUA_BRIDGE_TESTS
    { ngx_string("ngx_wasm_lua_test_argsrets"),
      &ngx_wasm_hfuncs_test_lua_argsrets,
      NULL,
      NULL },

    { ngx_string("ngx_wasm_lua_test_bad_chunk"),
      &ngx_wasm_hfuncs_test_lua_bad_chunk,
      NULL,
      NULL },
#endif

    ngx_wavm_hfunc_null
};


ngx_wavm_host_def_t  ngx_wasm_core_interface = {
    ngx_string("ngx_wasm_core"),
    ngx_wasm_core_hfuncs,
};
