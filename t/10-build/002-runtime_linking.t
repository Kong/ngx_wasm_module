# vim:set ft= ts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestBuild;

skip_valgrind();

our $buildroot = $t::TestBuild::buildroot;

plan tests => blocks() * 4;

run_tests();

__DATA__

=== TEST 1: dynamically linked libwasmtime (environment variables)
--- skip_eval: 4: $ENV{NGX_WASM_RUNTIME} ne 'wasmtime'
--- build: make
--- grep_nginxV
ngx_wasm_module [dev debug wasmtime]
built by
--- grep_libs
libwasmtime



=== TEST 2: dynamically linked libwasmtime (./configure arguments)
--- skip_eval: 4: $ENV{NGX_WASM_RUNTIME} ne 'wasmtime'
--- build eval: qq{NGX_WASM_RUNTIME_INC= NGX_WASM_RUNTIME_LIB= NGX_WASM_RUNTIME_LD_OPT= NGX_BUILD_CONFIGURE_OPT="--with-cc-opt='-I$ENV{NGX_WASM_RUNTIME_DIR}/include' --with-ld-opt='-L$ENV{NGX_WASM_RUNTIME_DIR}'" make}
--- grep_nginxV
ngx_wasm_module [dev debug wasmtime]
built by
--- grep_libs
libwasmtime



=== TEST 3: dynamically linked libwasmer (environment variables)
--- skip_eval: 4: $ENV{NGX_WASM_RUNTIME} ne 'wasmer'
--- build: make
--- grep_nginxV
ngx_wasm_module [dev debug wasmer]
built by
--- grep_libs
libwasmer



=== TEST 4: dynamically linked libwasmer (./configure arguments)
--- skip_eval: 4: $ENV{NGX_WASM_RUNTIME} ne 'wasmer'
--- build eval: qq{NGX_WASM_RUNTIME_INC= NGX_WASM_RUNTIME_LIB= NGX_WASM_RUNTIME_LD_OPT= NGX_BUILD_CONFIGURE_OPT="--with-cc-opt='-I$ENV{NGX_WASM_RUNTIME_DIR}/include' --with-ld-opt='-L$ENV{NGX_WASM_RUNTIME_DIR}'" make}
--- grep_nginxV
ngx_wasm_module [dev debug wasmer]
built by
--- grep_libs
libwasmer



=== TEST 5: dynamically linked libwee8
--- SKIP: NYI - the V8 build system builds libwee8 statically
--- skip_eval: 4: $ENV{NGX_WASM_RUNTIME} ne 'v8'
--- build: make
--- grep_nginxV
ngx_wasm_module [dev debug v8]
built by
--- grep_libs
libwee8



=== TEST 6: statically linked libwasmtime, libwasmer (environment variables)
--- skip_eval: 4: ($ENV{NGX_WASM_RUNTIME} ne 'wasmtime' && $ENV{NGX_WASM_RUNTIME} ne 'wasmer')
--- build eval: qq{NGX_WASM_RUNTIME_INC="$ENV{NGX_WASM_RUNTIME_DIR}/include" NGX_WASM_RUNTIME_LD_OPT="$ENV{NGX_WASM_RUNTIME_DIR}/lib/lib$ENV{NGX_WASM_RUNTIME}.a" make}
--- no_grep_libs eval
[
    qr/libwasmtime/,
    qr/libwasmer/,
    qr/libwasm/,
]



=== TEST 7: statically linked libwasmtime, libwasmer (./configure arguments)
--- skip_eval: 4: ($ENV{NGX_WASM_RUNTIME} ne 'wasmtime' && $ENV{NGX_WASM_RUNTIME} ne 'wasmer')
--- build eval: qq{NGX_WASM_RUNTIME_INC= NGX_WASM_RUNTIME_LIB= NGX_WASM_RUNTIME_LD_OPT= NGX_BUILD_CONFIGURE_OPT="--with-cc-opt='-I$ENV{NGX_WASM_RUNTIME_DIR}/include' --with-ld-opt='$ENV{NGX_WASM_RUNTIME_DIR}/lib/lib$ENV{NGX_WASM_RUNTIME}.a'" make}
--- no_grep_libs eval
[
    qr/libwasmtime/,
    qr/libwasmer/,
    qr/libwasm/,
]



=== TEST 8: statically linked libwee8 (environment variables)
--- skip_eval: 4: $ENV{NGX_WASM_RUNTIME} ne 'v8'
--- build eval: qq{NGX_WASM_RUNTIME_LD_OPT="$ENV{NGX_WASM_RUNTIME_DIR}/lib/libwee8.a -L$ENV{NGX_WASM_RUNTIME_DIR}/lib" make}
--- grep_nginxV
ngx_wasm_module [dev debug v8]
--- run_cmd eval: qq{nm -g $::buildroot/nginx}
--- grep_cmd eval
[
    qr/T _?wasm_store_new/,
]
--- no_grep_libs
libwee8



=== TEST 9: statically linked libwee8 (./configure arguments)
--- skip_eval: 4: $ENV{NGX_WASM_RUNTIME} ne 'v8'
--- build eval: qq{NGX_WASM_RUNTIME_INC= NGX_WASM_RUNTIME_LIB= NGX_WASM_RUNTIME_LD_OPT= NGX_BUILD_CONFIGURE_OPT="--with-cc-opt='-I$ENV{NGX_WASM_RUNTIME_DIR}/include' --with-ld-opt='$ENV{NGX_WASM_RUNTIME_DIR}/lib/libwee8.a -L$ENV{NGX_WASM_RUNTIME_DIR}'" make}
--- grep_nginxV
ngx_wasm_module [dev debug v8]
--- run_cmd eval: qq{nm -g $::buildroot/nginx}
--- grep_cmd eval
[
    qr/T _?wasm_store_new/
]
--- no_grep_libs
libwee8
