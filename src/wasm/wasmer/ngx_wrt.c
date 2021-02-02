#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_wasm.h>


void
ngx_wasmer_last_err(ngx_wrt_res_t **res)
{
    ngx_str_t   *werr = NULL;
    u_char      *err = (u_char *) "no memory";
    ngx_uint_t   errlen;
    ngx_int_t    rc;

    errlen = wasmer_last_error_length();
    if (errlen) {
        werr = ngx_alloc(sizeof(ngx_str_t), ngx_cycle->log);
        if (werr == NULL) {
            goto error;
        }

        werr->len = errlen;
        werr->data = ngx_alloc(errlen, ngx_cycle->log);
        if (werr->data == NULL) {
            goto error;
        }

        rc = wasmer_last_error_message((char *) werr->data, errlen);
        if (rc < 0) {
            err = (u_char *) "NYI: wasmer error";
            goto error;
        }

        *res = werr;
    }
#if (NGX_DEBUG)
    else {
        ngx_wasm_log_error(NGX_LOG_EMERG, ngx_cycle->log, 0,
                           "no wasmer error to retrieve");
    }
#endif

    return;

error:

    ngx_wasm_log_error(NGX_LOG_EMERG, ngx_cycle->log, 0, NULL,
                       "failed to retrieve last wasmer error: %s", err);

    if (werr) {
        if (werr->data) {
            ngx_free(werr->data);
        }

        ngx_free(werr);
    }
}


u_char *
ngx_wrt_error_log_handler(ngx_wrt_res_t *res, u_char *buf, size_t len)
{
    u_char  *p = buf;

    if (res && res->len) {
        p = ngx_snprintf(buf, ngx_min(len, res->len), "\n%V", res);

        ngx_free(res->data);
        ngx_free(res);
    }

    return p;
}
