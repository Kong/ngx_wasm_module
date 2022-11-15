#ifndef _NGX_STREAM_WASM_H_INCLUDED_
#define _NGX_STREAM_WASM_H_INCLUDED_


#include <ngx_stream.h>
#include <ngx_wasm_ops.h>


struct ngx_stream_wasm_ctx_s {
    ngx_stream_session_t              *s;
    ngx_connection_t                  *connection;
    ngx_pool_t                        *pool;  /* r->pool */
    ngx_wasm_op_ctx_t                  opctx;
    void                              *data;  /* per-stream extra context */

    ngx_chain_t                       *free_bufs;
    ngx_chain_t                       *busy_bufs;
};


#endif /* _NGX_STREAM_WASM_H_INCLUDED_ */
