# vim:set ft= ts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestBuild;

our $buildroot = $t::TestBuild::buildroot;

plan tests => 4 * blocks();

run_tests();

__DATA__

=== TEST 1: build (default)
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
--- build: NGX_BUILD_CONFIGURE='--without-pcre --without-http_rewrite_module --without-http_gzip_module --without-http_auth_basic_module' make
--- run_cmd eval: qq{ldd $::buildroot/nginx}
--- no_grep_cmd eval
[
    qr/libz/,
    qr/libcrypt/,
    qr/libpcre/
]



=== TEST 5: build with wasmtime
--- build: NGX_WASM_RUNTIME=wasmtime make
--- run_cmd eval: qq{ldd $::buildroot/nginx}
--- grep_nginxV
ngx_wasm_module [dev debug wasmtime]
built by
--- grep_cmd
libwasmtime
