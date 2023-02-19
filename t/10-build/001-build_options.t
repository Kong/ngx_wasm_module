# vim:set ft= ts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestBuild;

our $buildroot = $t::TestBuild::buildroot;

plan tests => 4 * blocks();

run_tests();

__DATA__

=== TEST 1: build default options
--- build: make
--- grep_nginxV
ngx_wasm_module [dev debug
--with-debug
-O0 -ggdb3 -gdwarf



=== TEST 2: build without debug
--- build: make NGX_BUILD_DEBUG=0
--- grep_nginxV
ngx_wasm_module [dev
-O0 -ggdb3 -gdwarf
--- no_grep_nginxV
--with-debug



=== TEST 3: build optimized, without debug
--- build: make NGX_BUILD_CC_OPT=-O2 NGX_BUILD_DEBUG=0
--- grep_nginxV
-O2
--- no_grep_nginxV eval
[
    qr/-O[01g]/,
    qr/-g/
]



=== TEST 4: build with minimal libraries
--- build: NGX_BUILD_CONFIGURE_OPT='--without-pcre --without-http_rewrite_module --without-http_gzip_module --without-http_auth_basic_module' NGX_BUILD_SSL=0 make
--- no_grep_libs eval
[
    qr/libz/,
    qr/libcrypt/,
    qr/libpcre/
]



=== TEST 5: build with ngx_stream_core_module (--with-stream)
--- build: NGX_BUILD_CONFIGURE_OPT='--with-stream' make
--- grep_nginxV
--with-stream
--- run_cmd eval: qq{nm -g $::buildroot/nginx}
--- grep_cmd
ngx_http_wasm
ngx_stream_wasm_module



=== TEST 6: build without ngx_http_core_module nor ngx_stream_core_module (--without-http)
--- build: NGX_BUILD_CONFIGURE_OPT='--without-http' make
--- grep_nginxV
--without-http
--- run_cmd eval: qq{nm -g $::buildroot/nginx}
--- no_grep_cmd
ngx_http_wasm
--- grep_cmd
ngx_wavm



=== TEST 7: build without ngx_http_core_module (--without-http --with-stream)
--- build: NGX_BUILD_CONFIGURE_OPT='--without-http --with-stream' make
--- grep_nginxV
--with-stream
--- run_cmd eval: qq{nm -g $::buildroot/nginx}
--- no_grep_cmd
ngx_http_wasm
--- grep_cmd
ngx_stream_wasm_module



=== TEST 8: build with OpenResty
--- build: NGX_WASM_RUNTIME=wasmtime NGX_BUILD_OPENRESTY=1.21.4.1 make
--- grep_nginxV
openresty/1.21.4.1 (ngx_wasm_module [dev debug
--with-debug
-O0 -ggdb3 -gdwarf
