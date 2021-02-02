#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_wrt.h>


u_char *
ngx_wrt_error_log_handler(ngx_wrt_res_t *res, u_char *buf, size_t len)
{
    u_char            *p = buf;
    wasmtime_error_t  *error = res;
    wasm_message_t     errmsg;

    if (error) {
        wasmtime_error_message(error, &errmsg);

        p = ngx_snprintf(buf, len, "\n%*s", errmsg.size, errmsg.data);

        wasm_byte_vec_delete(&errmsg);
        wasmtime_error_delete(error);
    }

    return p;
}
