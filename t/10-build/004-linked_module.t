# vim:set ft= ts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestBuild;

skip_on_static_runtime();

our $buildroot = $t::TestBuild::buildroot;

plan tests => 4 * blocks();

run_tests();

__DATA__

=== TEST 1: build with dynamic runtime
--- build: make
--- grep_nginxV
ngx_wasm_module [dev debug wasmtime]
built by
--- grep_libs
libwasmtime
