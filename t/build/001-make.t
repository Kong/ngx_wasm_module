# vim:set ft= ts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestBuild;

plan tests => 4 * blocks();

run_tests();

__DATA__

=== TEST 1: make (default)
--- build: make
--- grep_nginxV
ngx_wasm_module [dev debug]
--with-debug
-Og -ggdb3 -gdwarf



=== TEST 2: make no debug
--- build: make NGX_BUILD_DEBUG=0
--- grep_nginxV
ngx_wasm_module [dev]
-Og -ggdb3 -gdwarf
--- no_grep_nginxV
--with-debug



=== TEST 3: make optimized
--- build: make NGX_BUILD_CC_OPT=-O2 NGX_BUILD_DEBUG=0
--- grep_nginxV
-O2
--- no_grep_nginxV eval
[
    qr/-O[01g]/,
    qr/-g/
]
