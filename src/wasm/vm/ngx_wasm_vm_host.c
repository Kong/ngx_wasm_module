#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_wasm_vm_host.h>


void
ngx_wasm_hctx_trapmsg(ngx_wasm_hctx_t *hctx,
#if (NGX_HAVE_VARIADIC_MACROS)
    const char *fmt, ...)

#else
    const char *fmt, va_list args)
#endif
{
    u_char               *p;
    static const size_t   maxlen = NGX_WASM_MAX_HOST_TRAP_STR - 1;

#if (NGX_HAVE_VARIADIC_MACROS)
    va_list  args;

    va_start(args, fmt);
    p = ngx_vsnprintf(&hctx->trapmsg[0], maxlen, fmt, args);
    va_end(args);

#else
    p = ngx_vsnprintf(&hctx->trapmsg[0], maxlen, fmt, args);
#endif

    *p++ = '\0';

    hctx->trapmsglen = p - hctx->trapmsg;
}
