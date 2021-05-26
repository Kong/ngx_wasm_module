# vim:set ft= ts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestBuild;

our $buildroot = $t::TestBuild::buildroot;

plan tests => 4 * blocks();

run_tests();

__DATA__

=== TEST 1: build as a dynamic module, with dynamically linked runtime
--- build: NGX_BUILD_DYNAMIC_MODULE=1 make
--- run_cmd eval: qq{find $::buildroot -name 'ngx_wasm_module.*' | xargs -I{} nm -g {}}
--- grep_cmd eval
[
    qr/T _?ngx_wavm/,
    qr/T _?ngx_http_wasm/,
    qr/U _?wasm_instance_new/,
]



=== TEST 2: build as a dynamic module, with statically linked runtime
--- skip_eval: 4: $ENV{NGX_WASM_RUNTIME_DIR} eq '' || $ENV{NGX_WASM_RUNTIME} eq ''
--- build eval: qq{NGX_WASM_RUNTIME_INC="$ENV{NGX_WASM_RUNTIME_DIR}/include" NGX_WASM_RUNTIME_LD_OPT="$ENV{NGX_WASM_RUNTIME_DIR}/lib/lib$ENV{NGX_WASM_RUNTIME}.a -lm" NGX_BUILD_DYNAMIC_MODULE=1 make}
--- run_cmd eval: qq{find $::buildroot -name 'ngx_wasm_module.*' | xargs -I{} nm -g {}}
--- grep_cmd eval
[
    qr/T _?ngx_wavm/,
    qr/T _?ngx_http_wasm/,
    qr/T _?wasm_instance_new/,
]
