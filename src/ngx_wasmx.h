#ifndef _NGX_WA_H_INCLUDED_
#define _NGX_WA_H_INCLUDED_


#include <ngx_core.h>


#if (NGX_DEBUG)
#include <assert.h>
#   define ngx_wa_assert(a)        assert(a)
#else
#   define ngx_wa_assert(a)
#endif

#define NGX_WA_BAD_FD              (ngx_socket_t) -1


#endif /* _NGX_WA_H_INCLUDED_ */
