#ifndef _DDEBUG_H_INCLUDED_
#define _DDEBUG_H_INCLUDED_


#include <ngx_config.h>
#include <nginx.h>
#include <ngx_core.h>


#if defined(DDEBUG) && (DDEBUG)
#   if (NGX_HAVE_VARIADIC_MACROS)
#       define dd(...) fprintf(stderr, "wasm %s: ", __func__);               \
            fprintf(stderr, __VA_ARGS__);                                    \
            fprintf(stderr, " at %s line %d.\n", __FILE__, __LINE__)
#   else
#include <stdarg.h>
#include <stdio.h>
#include <stdarg.h>

static ngx_inline void
dd(const char *fmt, ...) {
}
#   endif
#else
#   if (NGX_HAVE_VARIADIC_MACROS)
#       define dd(...)
#   else
#include <stdarg.h>

static ngx_inline void
dd(const char *fmt, ...) {
}
#   endif
#endif


#endif /* _DDEBUG_H_INCLUDED_ */
