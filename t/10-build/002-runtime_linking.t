# vim:set ft= ts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestBuild;

our $buildroot = $t::TestBuild::buildroot;

my $nyi = $ENV{NGX_WASM_RUNTIME} eq 'v8' ? 2 : 0;

plan tests => 4 * blocks() - $nyi;

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



=== TEST 3: build with dynamically linked libwee8 (TODO: NYI: V8's build system builds libwee8 statically)
--- SKIP
--- skip_eval: 4: $ENV{NGX_WASM_RUNTIME} ne 'v8'
--- build: make
--- grep_nginxV
ngx_wasm_module [dev debug v8]
built by
--- grep_libs
libwee8



=== TEST 4: build with statically linked runtime - wasmtime, wasmer
--- skip_eval: 4: !( $ENV{NGX_WASM_RUNTIME} eq 'wasmtime' || $ENV{NGX_WASM_RUNTIME} eq 'wasmer' ) || $ENV{NGX_WASM_RUNTIME_DIR} eq '' || $ENV{NGX_WASM_RUNTIME} eq ''
--- build eval: qq{NGX_WASM_RUNTIME_INC="$ENV{NGX_WASM_RUNTIME_DIR}/include" NGX_WASM_RUNTIME_LD_OPT="$ENV{NGX_WASM_RUNTIME_DIR}/lib/lib$ENV{NGX_WASM_RUNTIME}.a -lm -ldl -lpthread" make}
--- no_grep_libs eval
[
    qr/libwasmtime/,
    qr/libwasmer/,
    qr/libwasm/,
]



=== TEST 5: build with statically linked runtime - v8
--- skip_eval: 4: $ENV{NGX_WASM_RUNTIME} ne 'v8' || $ENV{NGX_WASM_RUNTIME_LIB} eq '' || $ENV{NGX_WASM_RUNTIME_INC} eq '' || $ENV{NGX_WASM_RUNTIME} eq ''
--- build eval: qq{NGX_WASM_RUNTIME_LD_OPT="$ENV{NGX_WASM_RUNTIME_LIB}/libwee8.a -lm -ldl -lpthread -lstdc++ -L$ENV{NGX_WASM_RUNTIME_LIB} -L$ENV{NGX_WASM_CWABT_LIB} -lcwabt" make}
--- no_grep_libs eval
[
    qr/libwee8/,
]
