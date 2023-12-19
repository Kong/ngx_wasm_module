#ifndef _NGX_WA_H_INCLUDED_
#define _NGX_WA_H_INCLUDED_


#include <ngx_core.h>


#if (NGX_DEBUG)
#include <assert.h>
#   define ngx_wasm_assert(a)        assert(a)
#else
#   define ngx_wasm_assert(a)
#endif


#endif /* _NGX_WA_H_INCLUDED_ */
