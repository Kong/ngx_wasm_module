# vim:set ft= ts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestBuild;

our $buildroot = $t::TestBuild::buildroot;

plan tests => 4 * blocks();

run_tests();

__DATA__

=== TEST 1: build with dynamically linked libwasmtime
--- skip_eval: 4: $ENV{NGX_WASM_RUNTIME} ne 'wasmtime'
--- build: make
--- grep_nginxV
ngx_wasm_module [dev debug wasmtime]
built by
--- grep_libs
libwasmtime



=== TEST 2: build with dynamically linked libwasmer
--- skip_eval: 4: $ENV{NGX_WASM_RUNTIME} ne 'wasmer'
--- build: make
--- grep_nginxV
ngx_wasm_module [dev debug wasmer]
built by
--- grep_libs
libwasmer



=== TEST 3: build with statically linked runtime
--- skip_eval: 4: $ENV{NGX_WASM_RUNTIME_DIR} eq '' || $ENV{NGX_WASM_RUNTIME} eq ''
--- build eval: qq{NGX_WASM_RUNTIME_INC="$ENV{NGX_WASM_RUNTIME_DIR}/include" NGX_WASM_RUNTIME_LD_OPT="$ENV{NGX_WASM_RUNTIME_DIR}/lib/lib$ENV{NGX_WASM_RUNTIME}.a -lm" make}
--- no_grep_libs eval
[
    qr/libwasmtime/,
    qr/libwasmer/,
    qr/libwasm/,
]
