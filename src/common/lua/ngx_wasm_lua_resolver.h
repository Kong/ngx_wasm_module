#ifndef _NGX_WASM_LUA_RESOLVER_H_INCLUDED_
#define _NGX_WASM_LUA_RESOLVER_H_INCLUDED_


#include <ngx_wasm_lua.h>


#define NGX_WASM_LUA_RESOLVE_ERR  100


ngx_int_t ngx_wasm_lua_resolver_resolve(ngx_resolver_ctx_t *rslv_ctx);


#endif /* _NGX_WASM_LUA_RESOLVER_H_INCLUDED_ */
