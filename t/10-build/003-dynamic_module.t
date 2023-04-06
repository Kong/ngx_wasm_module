# vim:set ft= ts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestBuild;

our $buildroot = $t::TestBuild::buildroot;

plan tests => 4 * blocks();

run_tests();

__DATA__

=== TEST 1: build as a dynamic module, with dynamically linked runtime
--- skip_eval: 4: $ENV{NGX_WASM_RUNTIME} eq 'v8'
--- build: NGX_BUILD_DYNAMIC_MODULE=1 make
--- run_cmd eval: qq{find $::buildroot -name 'ngx_wasm_module.*' | xargs -I{} nm -g {}}
--- grep_cmd eval
[
    qr/T _?ngx_wavm/,
    qr/T _?ngx_http_wasm/,
    qr/U _?wasm(time)?_store_new/,
]



=== TEST 2: build as a dynamic module, with statically linked runtime - wasmtime, wasmer
--- skip_eval: 4: !( $ENV{NGX_WASM_RUNTIME} eq 'wasmtime' || $ENV{NGX_WASM_RUNTIME} eq 'wasmer' ) || $ENV{NGX_WASM_RUNTIME_DIR} eq '' || $ENV{NGX_WASM_RUNTIME} eq ''
--- build eval: qq{NGX_WASM_RUNTIME_INC="$ENV{NGX_WASM_RUNTIME_DIR}/include" NGX_WASM_RUNTIME_LD_OPT="$ENV{NGX_WASM_RUNTIME_DIR}/lib/lib$ENV{NGX_WASM_RUNTIME}.a -lm -ldl -lpthread" NGX_BUILD_DYNAMIC_MODULE=1 make }
--- run_cmd eval: qq{find $::buildroot -name 'ngx_wasm_module.*' | xargs -I{} nm -g {}}
--- grep_cmd eval
[
    qr/T _?ngx_wavm/,
    qr/T _?ngx_http_wasm/,
    qr/T _?wasm(time)?_store_new/,
]



=== TEST 3: build as a dynamic module, with statically linked runtime - v8
--- skip_eval: 4: $ENV{NGX_WASM_RUNTIME} ne 'v8' || $ENV{NGX_WASM_RUNTIME_DIR} eq '' || $ENV{NGX_WASM_RUNTIME} eq ''
--- build eval: qq{NGX_WASM_RUNTIME_INC="$ENV{NGX_WASM_RUNTIME_DIR}/include" NGX_WASM_RUNTIME_LD_OPT="$ENV{NGX_WASM_RUNTIME_DIR}/lib/libwee8.a -L$ENV{NGX_WASM_RUNTIME_DIR}/lib -lv8bridge -lstdc++ -lm -ldl -lpthread" NGX_BUILD_DYNAMIC_MODULE=1 make}
--- run_cmd eval: qq{find $::buildroot -name 'ngx_wasm_module.*' | xargs -I{} nm -g {}}
--- grep_cmd eval
[
    qr/T _?ngx_wavm/,
    qr/T _?ngx_http_wasm/,
    qr/T _?wasm(time)?_store_new/,
]
