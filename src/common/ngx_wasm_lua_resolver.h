#ifndef _NGX_WASM_LUA_RESOLVER_H_INCLUDED_
#define _NGX_WASM_LUA_RESOLVER_H_INCLUDED_


#include <ngx_wasm_socket_tcp.h>


void      ngx_wasm_lua_resolver_init(ngx_conf_t *cf);
ngx_int_t ngx_wasm_lua_resolver_resolve(ngx_resolver_ctx_t *ctx);


#endif /* _NGX_WASM_LUA_RESOLVER_H_INCLUDED_ */
