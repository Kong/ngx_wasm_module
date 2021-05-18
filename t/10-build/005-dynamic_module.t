# vim:set ft= ts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestBuild;

skip_on_dyn_runtime();

our $buildroot = $t::TestBuild::buildroot;

plan tests => 4 * blocks();

run_tests();

__DATA__

=== TEST 1: build as a dynamic module
--- build: NGX_BUILD_DYNAMIC_MODULE=1 make
--- run_cmd eval: qq{nm -gD $::buildroot/ngx_wasm_module.so}
--- grep_cmd eval
[
    qr/T ngx_wavm/,
    qr/T ngx_http_wasm/,
    qr/T wasm_module_new/,
]



=== TEST 2: cannot build as a dynamic module with dynamic runtime
--- build: NGX_WASM_RUNTIME_PATH= NGX_BUILD_DYNAMIC_MODULE=1 make
--- exit_code: 2
--- expect_stderr
--- grep_build eval
[
    qr/checking/,
    qr/error: dynamic ngx_wasm_module build, NGX_WASM_RUNTIME_PATH is required/,
    qr/Dynamic build must statically link to the \".*?\" runtime/
]
