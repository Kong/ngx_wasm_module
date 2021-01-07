#ifndef _NGX_WASM_VM_HOST_H_INCLUDED_
#define _NGX_WASM_VM_HOST_H_INCLUDED_


#include <ngx_wasm.h>


void ngx_wasm_hctx_trapmsg(ngx_wasm_hctx_t *hctx,
#if (NGX_HAVE_VARIADIC_MACROS)
    const char *fmt, ...);

#else
    const char *fmt, va_list args);
#endif


#endif /* _NGX_WASM_VM_HOST_H_INCLUDED_ */
