# vim:set ft= ts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestBuild;

skip_valgrind();

our $buildroot = $t::TestBuild::buildroot;
our $openssl_ver = get_variable_from_makefile("OPENSSL");

plan tests => blocks() * 5;

run_tests();

__DATA__

=== TEST 1: build default options
--- build: make
--- grep_nginxV
ngx_wasm_module [dev debug
--with-debug
-O0 -ggdb3 -gdwarf
built with OpenSSL



=== TEST 2: build without debug
--- build: make NGX_BUILD_DEBUG=0
--- grep_nginxV
ngx_wasm_module [dev
-O0 -ggdb3 -gdwarf
built with OpenSSL
--- no_grep_nginxV
--with-debug



=== TEST 3: build optimized, without debug
--- build: make NGX_BUILD_CC_OPT=-O2 NGX_BUILD_DEBUG=0
--- grep_nginxV
-O2
built with OpenSSL
--- no_grep_nginxV eval
[
    qr/-O[01g]/,
    qr/-g/
]



=== TEST 4: linked against local OpenSSL
--- build: make NGX_BUILD_SSL=1 NGX_BUILD_SSL_STATIC=0
--- grep_nginxV eval
[
    qr/built with OpenSSL $::openssl_ver/
]
--- no_grep_nginxV
running with OpenSSL
--- run_cmd eval: qq{nm -g $::buildroot/nginx}
--- grep_libs eval
[
    qr/libssl/,
    qr/libcrypto/
]



=== TEST 5: static build against local OpenSSL
--- build: make NGX_BUILD_SSL_STATIC=1
--- grep_nginxV eval
[
    qr/ngx_wasm_module \[dev debug/,
    qr/built with OpenSSL $::openssl_ver/
]
--- run_cmd eval: qq{nm -g $::buildroot/nginx}
--- no_grep_libs eval
[
    qr/libssl/,
    qr/libcrypto/
]



=== TEST 6: build with minimal libraries
--- build: NGX_BUILD_CONFIGURE_OPT='--without-pcre --without-http_rewrite_module --without-http_gzip_module --without-http_auth_basic_module' NGX_BUILD_SSL=0 make
--- run_cmd eval: qq{nm -g $::buildroot/nginx}
--- no_grep_cmd
ssl_
--- no_grep_libs eval
[
    qr/libz/,
    qr/libcrypt/,
    qr/libpcre/
]



=== TEST 7: build with ngx_stream_core_module (--with-stream)
--- build: NGX_BUILD_CONFIGURE_OPT='--with-stream' make
--- grep_nginxV
--with-stream
built with OpenSSL
--- run_cmd eval: qq{nm -g $::buildroot/nginx}
--- grep_cmd
ngx_http_wasm
ngx_stream_wasm_module



=== TEST 8: build without ngx_http_core_module nor ngx_stream_core_module (--without-http)
--- build: NGX_BUILD_CONFIGURE_OPT='--without-http' make
--- grep_nginxV
--without-http
--- no_grep_nginxV
built with OpenSSL
--- run_cmd eval: qq{nm -g $::buildroot/nginx}
--- no_grep_cmd
ngx_http_wasm
--- grep_cmd
ngx_wavm



=== TEST 9: build without ngx_http_core_module (--without-http --with-stream)
--- build: NGX_BUILD_CONFIGURE_OPT='--without-http --with-stream' make
--- grep_nginxV
--with-stream
built with OpenSSL
--- run_cmd eval: qq{nm -g $::buildroot/nginx}
--- no_grep_cmd
ngx_http_wasm
--- grep_cmd
ngx_stream_wasm_module



=== TEST 10: build with OpenResty
--- build: NGX_BUILD_OPENRESTY=1.21.4.1 make
--- grep_nginxV
openresty/1.21.4.1 (ngx_wasm_module [dev debug
built with OpenSSL
--with-debug
-O0 -ggdb3 -gdwarf
