#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_wavm.h>


ngx_int_t
ngx_wasm_hfuncs_log(ngx_wavm_instance_t *instance,
    wasm_val_t args[], wasm_val_t rets[])
{
    uint32_t         level, len;
    ngx_wavm_ptr_t  *msg;

    level = args[0].of.i32;
    msg = ngx_wavm_memory_lift(instance->memory, args[1].of.i32);
    len = args[2].of.i32;

    ngx_log_error((ngx_uint_t) level, instance->log, 0, "%*s", len, msg);

    return NGX_WAVM_OK;
}


static ngx_wavm_host_func_def_t  ngx_wasm_core_hfuncs[] = {

    { ngx_string("ngx_log"),
      &ngx_wasm_hfuncs_log,
      ngx_wavm_arity_i32x3,
      NULL },

    ngx_wavm_hfunc_null
};


ngx_wavm_host_def_t  ngx_wasm_core_interface = {
    ngx_string("ngx_wasm_core"),
    ngx_wasm_core_hfuncs,
};
