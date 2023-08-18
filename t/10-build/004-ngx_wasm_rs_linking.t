# vim:set ft= ts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestBuild;

our $buildroot = $t::TestBuild::buildroot;

plan tests => 6 * blocks();

run_tests();

__DATA__

=== TEST 1: libwasmer without ngx-wasm-rs
--- skip_eval: 6: $ENV{NGX_WASM_RUNTIME} ne 'wasmer'
--- build: NGX_WASM_CARGO=0 make
--- grep_nginxV
ngx_wasm_module [dev debug wasmer]
--- grep_libs
libwasmer
--- no_grep_libs
libngx_wasm_rs
stub1
stub2



=== TEST 2: libwee8 without ngx-wasm-rs
--- skip_eval: 6: $ENV{NGX_WASM_RUNTIME} ne 'v8'
--- build: NGX_WASM_CARGO=0 make
--- exit_code: 2
--- expect_stderr
--- grep_build
./configure: error: ngx_wasm_module with v8 requires libngx_wasm_rs - aborting.
--- no_grep_build
stub1
stub2
stub3
stub4



=== TEST 3: dynamically linked libwasmer (environment variables) + dynamic ngx-wasm-rs built separately
--- skip_eval: 6: $ENV{NGX_WASM_RUNTIME} ne 'wasmer'
--- build eval: use Cwd qw(cwd); my $pwd = cwd(); qq{( cd $pwd/lib/ngx-wasm-rs; cargo build ) && NGX_WASM_CARGO=0 NGX_BUILD_CC_OPT="-I $pwd/lib/ngx-wasm-rs/include" NGX_BUILD_LD_OPT="-L$pwd/lib/ngx-wasm-rs/target/debug -Wl,-rpath=$pwd/lib/ngx-wasm-rs/target/debug -lngx_wasm_rs" make}
--- grep_nginxV
ngx_wasm_module [dev debug wasmer]
--- grep_libs
libwasmer
libngx_wasm_rs
--- no_grep_libs
stub1
stub2



=== TEST 4: dynamically linked libwasmer (./configure arguments) + dynamic ngx-wasm-rs built separately
--- skip_eval: 6: $ENV{NGX_WASM_RUNTIME} ne 'wasmer'
--- build eval: use Cwd qw(cwd); my $pwd = cwd(); qq{( cd $pwd/lib/ngx-wasm-rs; cargo build ) && NGX_WASM_CARGO=0 NGX_WASM_RUNTIME_INC= NGX_WASM_RUNTIME_LIB= NGX_WASM_RUNTIME_LD_OPT= NGX_BUILD_CONFIGURE_OPT="--with-cc-opt='-I$ENV{NGX_WASM_RUNTIME_DIR}/include -I$pwd/lib/ngx-wasm-rs/include' --with-ld-opt='-L$ENV{NGX_WASM_RUNTIME_DIR} -L$pwd/lib/ngx-wasm-rs/target/debug -Wl,-rpath=$pwd/lib/ngx-wasm-rs/target/debug -lngx_wasm_rs'" make}
--- grep_nginxV
ngx_wasm_module [dev debug wasmer]
--- grep_libs
libwasmer
libngx_wasm_rs
--- no_grep_libs
stub1
stub2



=== TEST 5: dynamically linked libwee8 (environment variables) + dynamic ngx-wasm-rs built separately
--- SKIP: NYI - the V8 build system builds libwee8 statically
--- skip_eval: 6: $ENV{NGX_WASM_RUNTIME} ne 'v8'
--- build eval: use Cwd qw(cwd); my $pwd = cwd(); qq{( cd $pwd/lib/ngx-wasm-rs; cargo build --features wat ) && NGX_WASM_CARGO=0 NGX_BUILD_CC_OPT="-I $pwd/lib/ngx-wasm-rs/include" NGX_BUILD_LD_OPT="-L$pwd/lib/ngx-wasm-rs/target/debug -Wl,-rpath=$pwd/lib/ngx-wasm-rs/target/debug -lngx_wasm_rs" make}
--- grep_nginxV
ngx_wasm_module [dev debug v8]
--- grep_libs
libwee8
libngx_wasm_rs
--- no_grep_libs
stub1
stub2



=== TEST 6: dynamically linked libwee8 (./configure arguments) + dynamic ngx-wasm-rs built separately
--- SKIP: NYI - the V8 build system builds libwee8 statically
--- skip_eval: 6: $ENV{NGX_WASM_RUNTIME} ne 'v8'
--- build eval: use Cwd qw(cwd); my $pwd = cwd(); qq{( cd $pwd/lib/ngx-wasm-rs; cargo build --features wat ) && NGX_WASM_CARGO=0 NGX_WASM_RUNTIME_INC= NGX_WASM_RUNTIME_LIB= NGX_WASM_RUNTIME_LD_OPT= NGX_BUILD_CONFIGURE_OPT="--with-cc-opt='-I$ENV{NGX_WASM_RUNTIME_DIR}/include -I$pwd/lib/ngx-wasm-rs/include' --with-ld-opt='-L$ENV{NGX_WASM_RUNTIME_DIR} -L$pwd/lib/ngx-wasm-rs/target/debug -Wl,-rpath=$pwd/lib/ngx-wasm-rs/target/debug -lngx_wasm_rs'" make}
--- grep_nginxV
ngx_wasm_module [dev debug v8]
--- grep_libs
libwee8
libngx_wasm_rs
--- no_grep_libs
stub1
stub2



=== TEST 7: statically linked libwasmer (environment variables) + static ngx-wasm-rs built separately
--- SKIP: Cannot link two static Rust libraries linked with different compilers
--- skip_eval: 6: $ENV{NGX_WASM_RUNTIME} ne 'wasmer'
--- build eval: use Cwd qw(cwd); my $pwd = cwd(); qq{( cd $pwd/lib/ngx-wasm-rs; cargo build ) && NGX_WASM_CARGO=0 NGX_BUILD_CC_OPT="-I $pwd/lib/ngx-wasm-rs/include" NGX_BUILD_LD_OPT="$pwd/lib/ngx-wasm-rs/target/debug/libngx_wasm_rs.a" NGX_WASM_RUNTIME_INC="$ENV{NGX_WASM_RUNTIME_DIR}/include" NGX_WASM_RUNTIME_LD_OPT="$ENV{NGX_WASM_RUNTIME_DIR}/lib/lib$ENV{NGX_WASM_RUNTIME}.a" make}
--- grep_nginxV
ngx_wasm_module [dev debug wasmer]
--- run_cmd eval: qq{nm -g $::buildroot/nginx}
--- grep_cmd eval
[
    qr/T _?wasm_store_new/,
    qr/T _?ngx_wasm_backtrace_demangle/,
]
--- no_grep_libs
libwasmer
libngx_wasm_rs



=== TEST 8: statically linked libwasmer (./configure arguments) + static ngx-wasm-rs built separately
--- SKIP: Cannot link two static Rust libraries linked with different compilers
--- skip_eval: 6: $ENV{NGX_WASM_RUNTIME} ne 'wasmer'
--- build eval: use Cwd qw(cwd); my $pwd = cwd(); qq{( cd $pwd/lib/ngx-wasm-rs; cargo build ) && NGX_WASM_CARGO=0 NGX_WASM_RUNTIME_INC= NGX_WASM_RUNTIME_LIB= NGX_WASM_RUNTIME_LD_OPT= NGX_BUILD_CONFIGURE_OPT="--with-cc-opt='-I$ENV{NGX_WASM_RUNTIME_DIR}/include -I$pwd/lib/ngx-wasm-rs/include' --with-ld-opt='$ENV{NGX_WASM_RUNTIME_DIR}/lib/lib$ENV{NGX_WASM_RUNTIME}.a $pwd/lib/ngx-wasm-rs/target/debug/libngx_wasm_rs.a'" make}
--- grep_nginxV
ngx_wasm_module [dev debug wasmer]
--- run_cmd eval: qq{nm -g $::buildroot/nginx}
--- grep_cmd eval
[
    qr/T _?wasm_store_new/,
    qr/T _?ngx_wasm_backtrace_demangle/,
]
--- no_grep_libs
libwasmer
libngx_wasm_rs



=== TEST 9: statically linked libwee8 (environment variables) + static ngx-wasm-rs built separately
--- skip_eval: 6: $ENV{NGX_WASM_RUNTIME} ne 'v8'
--- build eval: use Cwd qw(cwd); my $pwd = cwd(); qq{( cd $pwd/lib/ngx-wasm-rs; cargo build --features wat ) && NGX_WASM_CARGO=0 NGX_BUILD_CC_OPT="-I $pwd/lib/ngx-wasm-rs/include" NGX_BUILD_LD_OPT="$pwd/lib/ngx-wasm-rs/target/debug/libngx_wasm_rs.a" NGX_WASM_RUNTIME_LD_OPT="$ENV{NGX_WASM_RUNTIME_DIR}/lib/libwee8.a -L$ENV{NGX_WASM_RUNTIME_DIR}/lib" make}
--- grep_nginxV
ngx_wasm_module [dev debug v8]
--- run_cmd eval: qq{nm -g $::buildroot/nginx}
--- grep_cmd eval
[
    qr/T _?wasm_store_new/,
    qr/T _?ngx_wasm_wat_to_wasm/,
]
--- no_grep_libs
libwee8
libngx_wasm_rs



=== TEST 10: statically linked libwee8 (./configure arguments) + static ngx-wasm-rs built separately
--- skip_eval: 6: $ENV{NGX_WASM_RUNTIME} ne 'v8'
--- build eval: use Cwd qw(cwd); my $pwd = cwd(); qq{( cd $pwd/lib/ngx-wasm-rs; cargo build --features wat ) && NGX_WASM_CARGO=0 NGX_WASM_RUNTIME_INC= NGX_WASM_RUNTIME_LIB= NGX_WASM_RUNTIME_LD_OPT= NGX_BUILD_CONFIGURE_OPT="--with-cc-opt='-I$ENV{NGX_WASM_RUNTIME_DIR}/include -I$pwd/lib/ngx-wasm-rs/include' --with-ld-opt='$ENV{NGX_WASM_RUNTIME_DIR}/lib/libwee8.a -L$ENV{NGX_WASM_RUNTIME_DIR} $pwd/lib/ngx-wasm-rs/target/debug/libngx_wasm_rs.a' " make}
--- grep_nginxV
ngx_wasm_module [dev debug v8]
--- run_cmd eval: qq{nm -g $::buildroot/nginx}
--- grep_cmd eval
[
    qr/T _?wasm_store_new/,
    qr/T _?ngx_wasm_wat_to_wasm/,
]
--- no_grep_libs
libwee8
libngx_wasm_rs



=== TEST 11: statically linked libwasmer (environment variables) + dynamic ngx-wasm-rs built separately
--- skip_eval: 6: $ENV{NGX_WASM_RUNTIME} ne 'wasmer'
--- build eval: use Cwd qw(cwd); my $pwd = cwd(); qq{( cd $pwd/lib/ngx-wasm-rs; cargo build ) && NGX_WASM_CARGO=0 NGX_BUILD_CC_OPT="-I $pwd/lib/ngx-wasm-rs/include" NGX_BUILD_LD_OPT="-L$pwd/lib/ngx-wasm-rs/target/debug -Wl,-rpath=$pwd/lib/ngx-wasm-rs/target/debug -lngx_wasm_rs" NGX_WASM_RUNTIME_INC="$ENV{NGX_WASM_RUNTIME_DIR}/include" NGX_WASM_RUNTIME_LD_OPT="$ENV{NGX_WASM_RUNTIME_DIR}/lib/lib$ENV{NGX_WASM_RUNTIME}.a" make}
--- grep_nginxV
ngx_wasm_module [dev debug wasmer]
--- run_cmd eval: qq{nm -g $::buildroot/nginx}
--- grep_cmd eval
[
    qr/T _?wasm_store_new/,
]
--- no_grep_cmd eval
[
    qr/T _?ngx_wasm_backtrace_demangle/,
]
--- grep_libs
libngx_wasm_rs
--- no_grep_libs
libwasmer



=== TEST 12: statically linked libwasmer (./configure arguments) + dynamic ngx-wasm-rs built separately
--- skip_eval: 6: $ENV{NGX_WASM_RUNTIME} ne 'wasmer'
--- build eval: use Cwd qw(cwd); my $pwd = cwd(); qq{( cd $pwd/lib/ngx-wasm-rs; cargo build ) && NGX_WASM_CARGO=0 NGX_WASM_RUNTIME_INC= NGX_WASM_RUNTIME_LIB= NGX_WASM_RUNTIME_LD_OPT= NGX_BUILD_CONFIGURE_OPT="--with-cc-opt='-I$ENV{NGX_WASM_RUNTIME_DIR}/include -I$pwd/lib/ngx-wasm-rs/include' --with-ld-opt='$ENV{NGX_WASM_RUNTIME_DIR}/lib/lib$ENV{NGX_WASM_RUNTIME}.a -L$pwd/lib/ngx-wasm-rs/target/debug/ -Wl,-rpath=$pwd/lib/ngx-wasm-rs/target/debug/ -lngx_wasm_rs'" make}
--- grep_nginxV
ngx_wasm_module [dev debug wasmer]
--- run_cmd eval: qq{nm -g $::buildroot/nginx}
--- grep_cmd eval
[
    qr/T _?wasm_store_new/,
]
--- no_grep_cmd eval
[
    qr/T _?ngx_wasm_backtrace_demangle/,
]
--- grep_libs
libngx_wasm_rs
--- no_grep_libs
libwasmer



=== TEST 13: statically linked libwee8 (environment variables) + dynamic ngx-wasm-rs built separately
--- skip_eval: 6: $ENV{NGX_WASM_RUNTIME} ne 'v8'
--- build eval: use Cwd qw(cwd); my $pwd = cwd(); qq{( cd $pwd/lib/ngx-wasm-rs; cargo build --features wat ) && NGX_WASM_CARGO=0 NGX_BUILD_CC_OPT="-I $pwd/lib/ngx-wasm-rs/include" NGX_BUILD_LD_OPT="-L$pwd/lib/ngx-wasm-rs/target/debug -Wl,-rpath=$pwd/lib/ngx-wasm-rs/target/debug -lngx_wasm_rs" NGX_WASM_RUNTIME_LD_OPT="$ENV{NGX_WASM_RUNTIME_DIR}/lib/libwee8.a -L$ENV{NGX_WASM_RUNTIME_DIR}/lib" make}
--- grep_nginxV
ngx_wasm_module [dev debug v8]
--- run_cmd eval: qq{nm -g $::buildroot/nginx}
--- grep_cmd eval
[
    qr/T _?wasm_store_new/,
]
--- no_grep_cmd eval
[
    qr/T _?ngx_wasm_backtrace_demangle/,
]
--- grep_libs
libngx_wasm_rs
--- no_grep_libs
libwee8



=== TEST 14: statically linked libwee8 (./configure arguments) + dynamic ngx-wasm-rs built separately
--- skip_eval: 6: $ENV{NGX_WASM_RUNTIME} ne 'v8'
--- build eval: use Cwd qw(cwd); my $pwd = cwd(); qq{( cd $pwd/lib/ngx-wasm-rs; cargo build --features wat ) && NGX_WASM_CARGO=0 NGX_WASM_RUNTIME_INC= NGX_WASM_RUNTIME_LIB= NGX_WASM_RUNTIME_LD_OPT= NGX_BUILD_CONFIGURE_OPT="--with-cc-opt='-I$ENV{NGX_WASM_RUNTIME_DIR}/include -I$pwd/lib/ngx-wasm-rs/include' --with-ld-opt='$ENV{NGX_WASM_RUNTIME_DIR}/lib/libwee8.a -L$pwd/lib/ngx-wasm-rs/target/debug/ -Wl,-rpath=$pwd/lib/ngx-wasm-rs/target/debug/ -lngx_wasm_rs'" make}
--- grep_nginxV
ngx_wasm_module [dev debug v8]
--- run_cmd eval: qq{nm -g $::buildroot/nginx}
--- grep_cmd eval
[
    qr/T _?wasm_store_new/,
]
--- no_grep_cmd eval
[
    qr/T _?ngx_wasm_backtrace_demangle/,
]
--- grep_libs
libngx_wasm_rs
--- no_grep_libs
libwee8
