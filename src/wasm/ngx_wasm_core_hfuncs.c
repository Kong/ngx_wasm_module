#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_core.h>
#include <ngx_wasm.h>


ngx_int_t
ngx_wasm_hfuncs_log(ngx_wasm_hctx_t *hctx, const ngx_wasm_val_t args[],
    ngx_wasm_val_t rets[])
{
    int32_t  level, len, msg_offset;

    level = args[0].value.I32;
    msg_offset = args[1].value.I32;
    len = args[2].value.I32;

    ngx_log_error((ngx_uint_t) level, hctx->log, 0, "%*s",
                  len, hctx->mem_off + msg_offset);

    return NGX_WASM_OK;
}


ngx_wasm_hdef_func_t  ngx_wasm_core_hfuncs[] = {

    { ngx_string("ngx_log"),
      &ngx_wasm_hfuncs_log,
      ngx_wasm_args_i32_i32_I32,
      ngx_wasm_rets_none },

    ngx_wasm_hfunc_null
};
