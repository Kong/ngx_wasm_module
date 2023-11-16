# vim:set ft= ts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestBuild;

skip_valgrind();

our $buildroot = $t::TestBuild::buildroot;

plan tests => blocks() * 4;

run_tests();

__DATA__

=== TEST 1: dynamic module with dynamically linked runtime - wasmtime, wasmer (environment variables)
--- skip_eval: 4: $ENV{NGX_WASM_RUNTIME} eq 'v8'
--- build: NGX_BUILD_DYNAMIC_MODULE=1 make
--- grep_nginxV eval
[
    qr/ngx_wasm_module \[dev debug $ENV{NGX_WASM_RUNTIME} dyn\]/
]
--- run_cmd eval: qq{find $::buildroot -name 'ngx_wasm_module.*' | xargs -I{} nm -g {}}
--- grep_cmd eval
[
    qr/T _?ngx_wavm/,
    qr/U _?wasm(time)?_store_new/,
]



=== TEST 2: dynamic module with dynamically linked runtime - wasmtime, wasmer (./configure arguments)
--- skip_eval: 4: $ENV{NGX_WASM_RUNTIME} eq 'v8'
--- build eval: qq{NGX_WASM_RUNTIME_INC= NGX_WASM_RUNTIME_LIB= NGX_WASM_RUNTIME_LD_OPT= NGX_BUILD_DYNAMIC_MODULE=1 NGX_BUILD_CONFIGURE_OPT="--with-cc-opt='-I$ENV{NGX_WASM_RUNTIME_DIR}/include' --with-ld-opt='-L$ENV{NGX_WASM_RUNTIME_DIR}'" make}
--- grep_nginxV eval
[
    qr/ngx_wasm_module \[dev debug $ENV{NGX_WASM_RUNTIME} dyn\]/
]
--- run_cmd eval: qq{find $::buildroot -name 'ngx_wasm_module.*' | xargs -I{} nm -g {}}
--- grep_cmd eval
[
    qr/T _?ngx_wavm/,
    qr/U _?wasm(time)?_store_new/,
]



=== TEST 3: dynamic module with dynamically linked runtime - v8
--- SKIP: NYI - the V8 build system builds libwee8 statically
--- skip_eval: 4: $ENV{NGX_WASM_RUNTIME} ne 'v8'
--- build: NGX_BUILD_DYNAMIC_MODULE=1 make
--- grep_nginxV
ngx_wasm_module [dev debug v8 dyn]
--- run_cmd eval: qq{find $::buildroot -name 'ngx_wasm_module.*' | xargs -I{} nm -g {}}
--- grep_cmd eval
[
    qr/T _?ngx_wavm/,
    qr/U _?wasm_store_new/,
]



=== TEST 4: dynamic module with statically linked runtime - wasmtime, wasmer (environment variables)
--- skip_eval: 4: ($ENV{NGX_WASM_RUNTIME} ne 'wasmtime' && $ENV{NGX_WASM_RUNTIME} ne 'wasmer')
--- build eval: qq{NGX_WASM_RUNTIME_INC="$ENV{NGX_WASM_RUNTIME_DIR}/include" NGX_WASM_RUNTIME_LD_OPT="$ENV{NGX_WASM_RUNTIME_DIR}/lib/lib$ENV{NGX_WASM_RUNTIME}.a" NGX_BUILD_DYNAMIC_MODULE=1 make}
--- grep_nginxV eval
[
    qr/ngx_wasm_module \[dev debug $ENV{NGX_WASM_RUNTIME} dyn\]/
]
--- run_cmd eval: qq{find $::buildroot -name 'ngx_wasm_module.*' | xargs -I{} nm -g {}}
--- grep_cmd eval
[
    qr/T _?ngx_wavm/,
    qr/T _?wasm(time)?_store_new/,
]



=== TEST 5: dynamic module with statically linked runtime - wasmtime, wasmer (./configure arguments)
--- skip_eval: 4: ($ENV{NGX_WASM_RUNTIME} ne 'wasmtime' && $ENV{NGX_WASM_RUNTIME} ne 'wasmer')
--- build eval: qq{NGX_WASM_RUNTIME_INC= NGX_WASM_RUNTIME_LIB= NGX_WASM_RUNTIME_LD_OPT= NGX_BUILD_DYNAMIC_MODULE=1 NGX_BUILD_CONFIGURE_OPT="--with-cc-opt='-I$ENV{NGX_WASM_RUNTIME_DIR}/include' --with-ld-opt='$ENV{NGX_WASM_RUNTIME_DIR}/lib/lib$ENV{NGX_WASM_RUNTIME}.a'" make}
--- grep_nginxV eval
[
    qr/ngx_wasm_module \[dev debug $ENV{NGX_WASM_RUNTIME} dyn\]/
]
--- run_cmd eval: qq{find $::buildroot -name 'ngx_wasm_module.*' | xargs -I{} nm -g {}}
--- grep_cmd eval
[
    qr/T _?ngx_wavm/,
    qr/T _?wasm(time)?_store_new/,
]



=== TEST 6: dynamic module with statically linked runtime - v8 (environment variables)
--- skip_eval: 4: $ENV{NGX_WASM_RUNTIME} ne 'v8'
--- build eval: qq{NGX_WASM_RUNTIME_INC="$ENV{NGX_WASM_RUNTIME_DIR}/include" NGX_WASM_RUNTIME_LD_OPT="$ENV{NGX_WASM_RUNTIME_DIR}/lib/libwee8.a -L$ENV{NGX_WASM_RUNTIME_DIR}/lib" NGX_BUILD_DYNAMIC_MODULE=1 make}
--- run_cmd eval: qq{find $::buildroot -name 'ngx_wasm_module.*' | xargs -I{} nm -g {}}
--- grep_nginxV
ngx_wasm_module [dev debug v8 dyn]
--- grep_cmd eval
[
    qr/T _?ngx_wavm/,
    qr/T _?wasm_store_new/,
]



=== TEST 7: dynamic module with statically linked runtime - v8 (./configure arguments)
--- skip_eval: 4: $ENV{NGX_WASM_RUNTIME} ne 'v8'
--- build eval: qq{NGX_WASM_RUNTIME_INC= NGX_WASM_RUNTIME_LIB= NGX_WASM_RUNTIME_LD_OPT= NGX_BUILD_DYNAMIC_MODULE=1 NGX_BUILD_CONFIGURE_OPT="--with-cc-opt='-I$ENV{NGX_WASM_RUNTIME_DIR}/include' --with-ld-opt='$ENV{NGX_WASM_RUNTIME_DIR}/lib/libwee8.a -L$ENV{NGX_WASM_RUNTIME_DIR}/lib'" make}
--- run_cmd eval: qq{find $::buildroot -name 'ngx_wasm_module.*' | xargs -I{} nm -g {}}
--- grep_nginxV
ngx_wasm_module [dev debug v8 dyn]
--- grep_cmd eval
[
    qr/T _?ngx_wavm/,
    qr/T _?wasm_store_new/,
]
