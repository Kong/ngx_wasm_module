# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

#skip_valgrind('wasmtime');

our $nginxV = $t::TestWasm::nginxV;

our $n = 30;
plan tests => repeat_each() * (blocks() * $n);

run_tests();

__DATA__

=== TEST 1: backtraces directive - invalid arguments
--- main_config
    wasm {
        backtraces foo;
    }
--- must_die
--- error_log eval
qr/\[emerg\] .*? invalid value "foo" in "backtraces" directive, it must be "on" or "off"/
--- no_error_log eval
my @checks;
for my $i (3..$::n) {
    push(@checks, "stub$i\n");
}
[@checks]



=== TEST 2: backtraces directive - wasmtime 'auto', backtraces 'off'
--- skip_eval: 30: $::nginxV !~ m/wasmtime/
--- load_nginx_modules: ngx_http_echo_module
--- main_config eval
qq{
    wasm {
        module hostcalls $ENV{TEST_NGINX_CRATES_DIR}/hostcalls.wasm;
        compiler auto;
        backtraces off;
    }
}
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/trap';
        echo ok;
    }
--- error_code: 500
--- response_body_like: 500 Internal Server Error
--- error_log eval
[
    qr/\[info\] .*? \[wasm\] using wasmtime with compiler: "auto" \(backtraces: 0\)/,
    qr/\[crit\] .*? panicked at/,
    qr/custom trap/,
    qr/\[error\] .*? \[wasm\] error while executing at wasm backtrace:/,
    qr#    0: 0x[0-9a-f]+ - <unknown>!__rust_start_panic#,
    qr#    1: 0x[0-9a-f]+ - <unknown>!rust_panic#,
    qr#    2: 0x[0-9a-f]+ - <unknown>!std::panicking::rust_panic_with_hook::h[0-9a-f]+#,
    qr#    3: 0x[0-9a-f]+ - <unknown>!std::panicking::begin_panic::\{\{closure\}\}::h[0-9a-f]+#,
    qr#    4: 0x[0-9a-f]+ - <unknown>!std::sys_common::backtrace::__rust_end_short_backtrace::h[0-9a-f]+#,
    qr#    5: 0x[0-9a-f]+ - <unknown>!std::panicking::begin_panic::h[0-9a-f]+#,
    qr#    6: 0x[0-9a-f]+ - <unknown>!hostcalls::types::test_http::TestHttp::exec_tests::h[0-9a-f]+#,
    qr#    7: 0x[0-9a-f]+ - <unknown>!hostcalls::filter::<impl proxy_wasm::traits::HttpContext for hostcalls::types::test_http::TestHttp>::on_http_request_headers::h[0-9a-f]+#,
    qr#    8: 0x[0-9a-f]+ - <unknown>!proxy_wasm::dispatcher::Dispatcher::on_http_request_headers::h[0-9a-f]+#,
    qr#    9: 0x[0-9a-f]+ - <unknown>!proxy_on_request_headers#,
    qr#note: using the `WASMTIME_BACKTRACE_DETAILS=1` environment variable may show more debugging informatio#,
    qr#^$#,
    qr#Caused by:#,
    qr#    wasm trap: wasm `unreachable` instruction executed#,
]
--- no_access_log eval
my @checks;
for my $i (21..$::n) {
    push(@checks, "stub$i\n");
}
[@checks]



=== TEST 3: backtraces directive - wasmtime 'cranelift', backtraces 'on'
Also assert that WASMTIME_BACKTRACE_DETAILS is overriden.
--- skip_eval: 30: $::nginxV !~ m/wasmtime/
--- load_nginx_modules: ngx_http_echo_module
--- main_config eval
qq{
    env WASMTIME_BACKTRACE_DETAILS=0;

    wasm {
        module hostcalls $ENV{TEST_NGINX_CRATES_DIR}/hostcalls.wasm;
        compiler cranelift;
        backtraces on;
    }
}
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/trap';
        echo ok;
    }
--- error_code: 500
--- response_body_like: 500 Internal Server Error
--- error_log eval
[
    qr/\[info\] .*? \[wasm\] using wasmtime with compiler: "cranelift" \(backtraces: 1\)/,
    qr/\[crit\] .*? panicked at/,
    qr/custom trap/,
    qr/\[error\] .*? \[wasm\] error while executing at wasm backtrace:/,
    qr#    0: 0x[0-9a-f]+ - .*?abort#,
    qr#                    at /rustc/[0-9a-f]+/library/panic_abort/src/lib.rs:[0-9]+:[0-9]+ *- __rust_start_panic#,
    qr#                    at /rustc/[0-9a-f]+/library/panic_abort/src/lib.rs:[0-9]+:[0-9]+#,
    qr#    1: 0x[0-9a-f]+ - .*?rust_panic#,
    qr#                    at /rustc/[0-9a-f]+/library/std/src/panicking.rs:[0-9]+:[0-9]+#,
    qr#    2: 0x[0-9a-f]+ - .*?rust_panic_with_hook#,
    qr#                    at /rustc/[0-9a-f]+/library/std/src/panicking.rs:[0-9]+:[0-9]+#,
    qr#    3: 0x[0-9a-f]+ - <unknown>!std::panicking::begin_panic::\{\{closure\}\}::h[0-9a-f]+#,
    qr#    4: 0x[0-9a-f]+ - <unknown>!std::sys_common::backtrace::__rust_end_short_backtrace::h[0-9a-f]+#,
    qr#    5: 0x[0-9a-f]+ - <unknown>!std::panicking::begin_panic::h[0-9a-f]+#,
    qr#    6: 0x[0-9a-f]+ - <unknown>!hostcalls::types::test_http::TestHttp::exec_tests::h[0-9a-f]+#,
    qr#    7: 0x[0-9a-f]+ - <unknown>!hostcalls::filter::<impl proxy_wasm::traits::HttpContext for hostcalls::types::test_http::TestHttp>::on_http_request_headers::h[0-9a-f]+#,
    qr#    8: 0x[0-9a-f]+ - <unknown>!proxy_wasm::dispatcher::Dispatcher::on_http_request_headers::h[0-9a-f]+#,
    qr#    9: 0x[0-9a-f]+ - <unknown>!proxy_on_request_headers#,
    qr#^$#,
    qr#Caused by:#,
    qr#    wasm trap: wasm `unreachable` instruction executed#,
]
--- no_error_log eval
my @checks;
for my $i (24..$::n) {
    push(@checks, "stub$i\n");
}
[@checks]



=== TEST 4: backtraces directive - wasmtime 'cranelift', backtraces 'off', WASMTIME_BACKTRACE_DETAILS=1
--- skip_eval: 30: $::nginxV !~ m/wasmtime/
--- load_nginx_modules: ngx_http_echo_module
--- main_config eval
qq{
    env WASMTIME_BACKTRACE_DETAILS=1;

    wasm {
        module hostcalls $ENV{TEST_NGINX_CRATES_DIR}/hostcalls.wasm;
        compiler cranelift;
        backtraces off;
    }
}
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/trap';
        echo ok;
    }
--- error_code: 500
--- response_body_like: 500 Internal Server Error
--- error_log eval
[
    qr/\[info\] .*? \[wasm\] using wasmtime with compiler: "cranelift" \(backtraces: 0\)/,
    qr/\[crit\] .*? panicked at/,
    qr/custom trap/,
    qr/\[error\] .*? \[wasm\] error while executing at wasm backtrace:/,
    qr#    0: 0x[0-9a-f]+ - .*?abort#,
    qr#                    at /rustc/[0-9a-f]+/library/panic_abort/src/lib.rs:[0-9]+:[0-9]+ *- __rust_start_panic#,
    qr#                    at /rustc/[0-9a-f]+/library/panic_abort/src/lib.rs:[0-9]+:[0-9]+#,
    qr#    1: 0x[0-9a-f]+ - .*?rust_panic#,
    qr#                    at /rustc/[0-9a-f]+/library/std/src/panicking.rs:[0-9]+:[0-9]+#,
    qr#    2: 0x[0-9a-f]+ - .*?rust_panic_with_hook#,
    qr#                    at /rustc/[0-9a-f]+/library/std/src/panicking.rs:[0-9]+:[0-9]+#,
    qr#    3: 0x[0-9a-f]+ - <unknown>!std::panicking::begin_panic::\{\{closure\}\}::h[0-9a-f]+#,
    qr#    4: 0x[0-9a-f]+ - <unknown>!std::sys_common::backtrace::__rust_end_short_backtrace::h[0-9a-f]+#,
    qr#    5: 0x[0-9a-f]+ - <unknown>!std::panicking::begin_panic::h[0-9a-f]+#,
    qr#    6: 0x[0-9a-f]+ - <unknown>!hostcalls::types::test_http::TestHttp::exec_tests::h[0-9a-f]+#,
    qr#    7: 0x[0-9a-f]+ - <unknown>!hostcalls::filter::<impl proxy_wasm::traits::HttpContext for hostcalls::types::test_http::TestHttp>::on_http_request_headers::h[0-9a-f]+#,
    qr#    8: 0x[0-9a-f]+ - <unknown>!proxy_wasm::dispatcher::Dispatcher::on_http_request_headers::h[0-9a-f]+#,
    qr#    9: 0x[0-9a-f]+ - <unknown>!proxy_on_request_headers#,
    qr#^$#,
    qr#Caused by:#,
    qr#    wasm trap: wasm `unreachable` instruction executed#,
]
--- no_error_log eval
my @checks;
for my $i (24..$::n) {
    push(@checks, "stub$i\n");
}
[@checks]



=== TEST 5: backtraces directive - wasmer 'auto', backtraces 'off'
--- skip_eval: 30: $::nginxV !~ m/wasmer/
--- load_nginx_modules: ngx_http_echo_module
--- main_config eval
qq{
    wasm {
        module hostcalls $ENV{TEST_NGINX_CRATES_DIR}/hostcalls.wasm;
        compiler auto;
        backtraces off;
    }
}
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/trap';
        echo ok;
    }
--- error_code: 500
--- response_body_like: 500 Internal Server Error
--- error_log eval
[
    qr/\[info\] .*? \[wasm\] using wasmer with compiler: "auto" \(backtraces: 0\)/,
    qr/\[crit\] .*? panicked at/,
    qr/custom trap/,
    qr/\[error\] .*? \[wasm\] unreachable/,
    qr/Backtrace:/,
    qr#	0: func [0-9]+ @ 0x[0-9a-f]+#,
    qr#	1: func [0-9]+ @ 0x[0-9a-f]+#,
    qr#	2: func [0-9]+ @ 0x[0-9a-f]+#,
    qr#	3: func [0-9]+ @ 0x[0-9a-f]+#,
    qr#	4: func [0-9]+ @ 0x[0-9a-f]+#,
    qr#	5: func [0-9]+ @ 0x[0-9a-f]+#,
    qr#	6: func [0-9]+ @ 0x[0-9a-f]+#,
    qr#	7: func [0-9]+ @ 0x[0-9a-f]+#,
    qr#	8: func [0-9]+ @ 0x[0-9a-f]+#,
    qr#	9: func [0-9]+ @ 0x[0-9a-f]+#,
]
--- no_error_log eval
my @checks;
for my $i (18..$::n) {
    push(@checks, "stub$i\n");
}
[@checks]



=== TEST 6: backtraces directive - wasmer 'cranelift', backtraces 'on'
--- skip_eval: 30: $::nginxV !~ m/wasmer/
--- load_nginx_modules: ngx_http_echo_module
--- main_config eval
qq{
    wasm {
        module hostcalls $ENV{TEST_NGINX_CRATES_DIR}/hostcalls.wasm;
        compiler cranelift;
        backtraces on;
    }
}
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/trap';
        echo ok;
    }
--- error_code: 500
--- response_body eval
qr/500 Internal Server Error/
--- error_log eval
[
    qr/\[info\] .*? \[wasm\] using wasmer with compiler: "cranelift" \(backtraces: 1\)/,
    qr/\[crit\] .*? panicked at/,
    qr/custom trap/,
    qr/\[error\] .*? \[wasm\] unreachable/,
    qr/Backtrace:/,
    qr#	0: __rust_start_panic @ 0x[0-9a-f]+#,
    qr#	1: rust_panic @ 0x[0-9a-f]+#,
    qr#	2: std::panicking::rust_panic_with_hook::h[0-9a-f]+ @ 0x[0-9a-f]+#,
    qr#	4: std::sys_common::backtrace::__rust_end_short_backtrace::h[0-9a-f]+ @ 0x[0-9a-f]+#,
    qr#	5: std::panicking::begin_panic::h[0-9a-f]+ @ 0x[0-9a-f]+#,
    qr#	6: hostcalls::types::test_http::TestHttp::exec_tests::h[0-9a-f]+ @ 0x[0-9a-f]+#,
    qr#	7: hostcalls::filter::<impl proxy_wasm::traits::HttpContext for hostcalls::types::test_http::TestHttp>::on_http_request_headers::h[0-9a-f]+ @ 0x[0-9a-f]+#,
    qr#	8: proxy_wasm::dispatcher::Dispatcher::on_http_request_headers::h[0-9a-f]+ @ 0x[0-9a-f]+#,
    qr#	9: proxy_on_request_headers @ 0x[0-9a-f]+#,
]
--- no_error_log eval
my @checks;
for my $i (17..$::n) {
    push(@checks, "stub$i\n");
}
[@checks]



=== TEST 7: backtraces directive - wasmer 'singlepass', backtraces 'on'
--- skip_eval: 30: $::nginxV !~ m/wasmer/
--- load_nginx_modules: ngx_http_echo_module
--- main_config eval
qq{
    wasm {
        module hostcalls $ENV{TEST_NGINX_CRATES_DIR}/hostcalls.wasm;
        compiler singlepass;
        backtraces on;
    }
}
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/trap';
        echo ok;
    }
--- error_code: 500
--- response_body_like: 500 Internal Server Error
--- error_log eval
[
    qr/\[info\] .*? \[wasm\] using wasmer with compiler: "singlepass" \(backtraces: 1\)/,
    qr/\[crit\] .*? panicked at/,
    qr/custom trap/,
    qr/\[error\] .*? \[wasm\] unreachable/,
    qr/Backtrace:/,
    qr#	0: __rust_start_panic @ 0x[0-9a-f]+#,
    qr#	1: rust_panic @ 0x[0-9a-f]+#,
    qr#	2: std::panicking::rust_panic_with_hook::h[0-9a-f]+ @ 0x[0-9a-f]+#,
    qr#	3: std::panicking::begin_panic::\{\{closure\}\}::h[0-9a-f]+ @ 0x[0-9a-f]+#,
    qr#	4: std::sys_common::backtrace::__rust_end_short_backtrace::h[0-9a-f]+ @ 0x[0-9a-f]+#,
    qr#	5: std::panicking::begin_panic::h[0-9a-f]+ @ 0x[0-9a-f]+#,
    qr#	6: hostcalls::types::test_http::TestHttp::exec_tests::h[0-9a-f]+ @ 0x[0-9a-f]+#,
    qr#	7: hostcalls::filter::<impl proxy_wasm::traits::HttpContext for hostcalls::types::test_http::TestHttp>::on_http_request_headers::h[0-9a-f]+ @ 0x[0-9a-f]+#,
    qr#	8: proxy_wasm::dispatcher::Dispatcher::on_http_request_headers::h[0-9a-f]+ @ 0x[0-9a-f]+#,
    qr#	9: proxy_on_request_headers @ 0x[0-9a-f]+#,
]
--- no_error_log eval
my @checks;
for my $i (18..$::n) {
    push(@checks, "stub$i\n");
}
[@checks]



=== TEST 8: backtraces directive - wasmer 'llvm', backtraces 'on'
--- SKIP: no llvm support in upstream releases
--- skip_eval: 30: $::nginxV !~ m/wasmer/
--- load_nginx_modules: ngx_http_echo_module
--- main_config eval
qq{
    wasm {
        module hostcalls $ENV{TEST_NGINX_CRATES_DIR}/hostcalls.wasm;
        compiler llvm;
        backtraces on;
    }
}
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/trap';
        echo ok;
    }
--- error_code: 500
--- response_body_like: 500 Internal Server Error
--- error_log eval
[
    qr/\[info\] .*? \[wasm\] using wasmer with compiler: "llvm" \(backtraces: 1\)/,
    qr/\[crit\] .*? panicked at/,
    qr/custom trap/,
    qr/\[error\] .*? \[wasm\] unreachable/,
    qr/Backtrace:/,
    qr#	0: __rust_start_panic @ 0x[0-9a-f]+#,
    qr#	1: rust_panic @ 0x[0-9a-f]+#,
    qr#	2: std::panicking::rust_panic_with_hook::h[0-9a-f]+ @ 0x[0-9a-f]+#,
    qr#	3: std::panicking::begin_panic::\{\{closure\}\}::h[0-9a-f]+ @ 0x[0-9a-f]+#,
    qr#	4: std::sys_common::backtrace::__rust_end_short_backtrace::h[0-9a-f]+ @ 0x[0-9a-f]+#,
    qr#	5: std::panicking::begin_panic::h[0-9a-f]+ @ 0x[0-9a-f]+#,
    qr#	6: hostcalls::types::test_http::TestHttp::exec_tests::h[0-9a-f]+ @ 0x[0-9a-f]+#,
    qr#	7: hostcalls::filter::<impl proxy_wasm::traits::HttpContext for hostcalls::types::test_http::TestHttp>::on_http_request_headers::h[0-9a-f]+ @ 0x[0-9a-f]+#,
    qr#	8: proxy_wasm::dispatcher::Dispatcher::on_http_request_headers::h[0-9a-f]+ @ 0x[0-9a-f]+#,
    qr#	9: proxy_on_request_headers @ 0x[0-9a-f]+#,
]
--- no_error_log eval
my @checks;
for my $i (18..$::n) {
    push(@checks, "stub$i\n");
}
[@checks]



=== TEST 9: backtraces directive - v8 'auto', backtraces 'off'
--- skip_eval: 30: $::nginxV !~ m/v8/
--- load_nginx_modules: ngx_http_echo_module
--- main_config eval
qq{
    wasm {
        module hostcalls $ENV{TEST_NGINX_CRATES_DIR}/hostcalls.wasm;
        compiler auto;
        backtraces off;
    }
}
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/trap';
        echo ok;
    }
--- error_code: 500
--- response_body eval
qr/500 Internal Server Error/
--- error_log eval
[
    qr/\[info\] .*? \[wasm\] using v8 with compiler: "auto" \(backtraces: 0\)/,
    qr/\[crit\] .*? panicked at/,
    qr/custom trap/,
    qr/\[error\] .*? \[wasm\] Uncaught RuntimeError: unreachable/,
    qr/Backtrace:/,
    qr#	0: func [0-9]+ @ 0x[0-9a-f]+#,
    qr#	1: func [0-9]+ @ 0x[0-9a-f]+#,
    qr#	2: func [0-9]+ @ 0x[0-9a-f]+#,
    qr#	3: func [0-9]+ @ 0x[0-9a-f]+#,
    qr#	4: func [0-9]+ @ 0x[0-9a-f]+#,
    qr#	5: func [0-9]+ @ 0x[0-9a-f]+#,
    qr#	6: func [0-9]+ @ 0x[0-9a-f]+#,
    qr#	7: func [0-9]+ @ 0x[0-9a-f]+#,
    qr#	8: func [0-9]+ @ 0x[0-9a-f]+#,
    qr#	9: func [0-9]+ @ 0x[0-9a-f]+#,
]
--- no_error_log eval
my @checks;
for my $i (18..$::n) {
    push(@checks, "stub$i\n");
}
[@checks]



=== TEST 10: backtraces directive - v8 'auto', backtraces 'on'
--- skip_eval: 30: $::nginxV !~ m/v8/
--- load_nginx_modules: ngx_http_echo_module
--- main_config eval
qq{
    wasm {
        module hostcalls $ENV{TEST_NGINX_CRATES_DIR}/hostcalls.wasm;
        compiler auto;
        backtraces on;
    }
}
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/trap';
        echo ok;
    }
--- error_code: 500
--- response_body eval
qr/500 Internal Server Error/
--- error_log eval
[
    qr/\[info\] .*? \[wasm\] using v8 with compiler: "auto" \(backtraces: 1\)/,
    qr/\[crit\] .*? panicked at/,
    qr/custom trap/,
    qr/\[error\] .*? \[wasm\] Uncaught RuntimeError: unreachable/,
    qr/Backtrace:/,
    qr#	0: __rust_start_panic @ 0x[0-9a-f]+#,
    qr#	1: rust_panic @ 0x[0-9a-f]+#,
    qr#	2: std::panicking::rust_panic_with_hook::h[0-9a-f]+ @ 0x[0-9a-f]+#,
    qr#	3: std::panicking::begin_panic::\{\{closure\}\}::h[0-9a-f]+ @ 0x[0-9a-f]+#,
    qr#	4: std::sys_common::backtrace::__rust_end_short_backtrace::h[0-9a-f]+ @ 0x[0-9a-f]+#,
    qr#	5: std::panicking::begin_panic::h[0-9a-f]+ @ 0x[0-9a-f]+#,
    qr#	6: hostcalls::types::test_http::TestHttp::exec_tests::h[0-9a-f]+ @ 0x[0-9a-f]+#,
    qr#	7: hostcalls::filter::<impl proxy_wasm::traits::HttpContext for hostcalls::types::test_http::TestHttp>::on_http_request_headers::h[0-9a-f]+ @ 0x[0-9a-f]+#,
    qr#	8: proxy_wasm::dispatcher::Dispatcher::on_http_request_headers::h[0-9a-f]+ @ 0x[0-9a-f]+#,
    qr#	9: proxy_on_request_headers @ 0x[0-9a-f]+#,
]
--- no_error_log eval
my @checks;
for my $i (18..$::n) {
    push(@checks, "stub$i\n");
}
[@checks]
