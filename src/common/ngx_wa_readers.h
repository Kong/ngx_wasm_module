#ifndef _NGX_WA_READERS_H_INCLUDED_
#define _NGX_WA_READERS_H_INCLUDED_


#include <ngx_core.h>


#if 0
ngx_int_t ngx_wa_read_all(ngx_buf_t *src, ngx_chain_t *buf_in, ssize_t bytes);
ngx_int_t ngx_wa_read_line(ngx_buf_t *src, ngx_chain_t *buf_in, ssize_t bytes);
#endif
ngx_int_t ngx_wa_read_bytes(ngx_buf_t *src, ngx_chain_t *buf_in,
    ssize_t bytes, size_t *rest);


#endif /* _NGX_WA_READERS_H_INCLUDED_ */
