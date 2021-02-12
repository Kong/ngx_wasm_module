#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_proxy_wasm.h>
#include <ngx_wavm.h>




ngx_int_t
ngx_proxy_wasm_hfuncs_proxy_abi_version(ngx_wavm_instance_t *instance,
   const wasm_val_t args[], wasm_val_t rets[])
{
   return NGX_WAVM_OK;
}


ngx_int_t
ngx_proxy_wasm_hfuncs_proxy_log(ngx_wavm_instance_t *instance,
    const wasm_val_t args[], wasm_val_t rets[])
{
    ngx_uint_t   level;
    int32_t      log_level, msg_size, msg_data;

    log_level = args[0].of.i32;
    msg_data = args[1].of.i32;
    msg_size = args[2].of.i32;

    switch (log_level) {

    case NGX_PROXY_WASM_LOG_TRACE:
    case NGX_PROXY_WASM_LOG_DEBUG:
        level = NGX_LOG_DEBUG;
        break;

    case NGX_PROXY_WASM_LOG_INFO:
        level = NGX_LOG_INFO;
        break;

    case NGX_PROXY_WASM_LOG_WARNING:
        level = NGX_LOG_WARN;
        break;

    case NGX_PROXY_WASM_LOG_ERROR:
        level = NGX_LOG_ERR;
        break;

    default:
        rets[0] = ngx_proxy_wasm_result_bad_arg();
        return NGX_WAVM_OK;

    }

    ngx_log_error(level, instance->log, 0, "%*s",
                  msg_size, instance->mem_offset + msg_data);

    rets[0] = ngx_proxy_wasm_result_ok();
    return NGX_WAVM_OK;
}


static ngx_wavm_host_func_def_t  ngx_proxy_wasm_hfuncs[] = {

   { ngx_string("proxy_abi_version_0_2_1"),
     &ngx_proxy_wasm_hfuncs_proxy_abi_version,
     NULL,
     NULL },

   { ngx_string("proxy_log"),
     &ngx_proxy_wasm_hfuncs_proxy_log,
     ngx_wavm_arity_i32_i32_i32,
     ngx_wavm_arity_i32 },

   ngx_wavm_hfunc_null
};


ngx_wavm_host_def_t  ngx_proxy_wasm_host = {
    ngx_string("ngx_proxy_wasm"),
    ngx_proxy_wasm_hfuncs,
};
