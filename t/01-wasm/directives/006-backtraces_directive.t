# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

skip_valgrind();

our $nginxV = $t::TestWasm::nginxV;

plan tests => repeat_each() * (blocks() * 30);

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
--- no_error_log
stub3
stub4
stub5
stub6
stub7
stub8
stub9
stub10
stub11
stub12
stub13
stub14
stub15
stub16
stub17
stub18
stub19
stub20
stub21
stub22
stub23
stub24
stub25
stub26
stub27
stub28
stub29
stub30



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
        proxy_wasm hostcalls 'test=/t/set_property name=nonexistent_property';
        echo ok;
    }
--- error_code: 500
--- response_body eval
qr/500 Internal Server Error/
--- error_log eval
[
    qr/\[crit\] .*? panicked at 'unexpected status: 1'/,
    qr/\[error\] .*? \[wasm\] error while executing at wasm backtrace:/,
    qr#    0: 0x[0-9a-f]+ - <unknown>!__rust_start_panic#,
    qr#    1: 0x[0-9a-f]+ - <unknown>!rust_panic#,
    qr#    2: 0x[0-9a-f]+ - <unknown>!std::panicking::rust_panic_with_hook::h[0-9a-f]+#,
    qr#    3: 0x[0-9a-f]+ - <unknown>!std::panicking::begin_panic_handler::\{\{closure\}\}::h[0-9a-f]+#,
    qr#    4: 0x[0-9a-f]+ - <unknown>!std::sys_common::backtrace::__rust_end_short_backtrace::h[0-9a-f]+#,
    qr#    5: 0x[0-9a-f]+ - <unknown>!rust_begin_unwind#,
    qr#    6: 0x[0-9a-f]+ - <unknown>!core::panicking::panic_fmt::h[0-9a-f]+#,
    qr#    7: 0x[0-9a-f]+ - <unknown>!proxy_wasm::hostcalls::set_property::h[0-9a-f]+#,
    qr#    8: 0x[0-9a-f]+ - <unknown>!hostcalls::tests::test_set_property::h[0-9a-f]+#,
    qr#    9: 0x[0-9a-f]+ - <unknown>!hostcalls::types::test_http::TestHttp::exec_tests::h[0-9a-f]+#,
    qr#   10: 0x[0-9a-f]+ - <unknown>!hostcalls::filter::<impl proxy_wasm::traits::HttpContext for hostcalls::types::test_http::TestHttp>::on_http_request_headers::h[0-9a-f]+#,
    qr#   11: 0x[0-9a-f]+ - <unknown>!proxy_wasm::dispatcher::Dispatcher::on_http_request_headers::h[0-9a-f]+#,
    qr#   12: 0x[0-9a-f]+ - <unknown>!proxy_on_request_headers#,
]
--- no_error_log eval
qr/\[info\] .*? \[wasm\] wasmtime enabling detailed backtraces/,
--- no_access_log
stub19
stub20
stub21
stub22
stub23
stub24
stub25
stub26
stub27
stub28
stub29
stub30



=== TEST 3: backtraces directive - wasmtime 'cranelift', backtraces 'on'
--- skip_eval: 30: $::nginxV !~ m/wasmtime/
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
        proxy_wasm hostcalls 'test=/t/set_property name=nonexistent_property';
        echo ok;
    }
--- error_code: 500
--- response_body eval
qr/500 Internal Server Error/
--- error_log eval
[
    qr/\[crit\] .*? panicked at 'unexpected status: 1'/,
    qr/\[error\] .*? \[wasm\] error while executing at wasm backtrace:/,
    qr#    0: 0x[0-9a-f]+ - panic_abort::__rust_start_panic::abort::h[0-9a-f]+#,
    qr#                    at /rustc/[0-9a-f]+/library/panic_abort/src/lib.rs:[0-9]+:[0-9]+ *- __rust_start_panic#,
    qr#                    at /rustc/[0-9a-f]+/library/panic_abort/src/lib.rs:[0-9]+:[0-9]+#,
    qr#    1: 0x[0-9a-f]+ - rust_panic#,
    qr#                    at /rustc/[0-9a-f]+/library/std/src/panicking.rs:[0-9]+:[0-9]+#,
    qr#    2: 0x[0-9a-f]+ - std::panicking::rust_panic_with_hook::h[0-9a-f]+#,
    qr#                    at /rustc/[0-9a-f]+/library/std/src/panicking.rs:[0-9]+:[0-9]+#,
    qr#    3: 0x[0-9a-f]+ - std::panicking::begin_panic_handler::\{\{closure\}\}::h[0-9a-f]+#,
    qr#                    at /rustc/[0-9a-f]+/library/std/src/panicking.rs:[0-9]+:[0-9]+#,
    qr#    4: 0x[0-9a-f]+ - std::sys_common::backtrace::__rust_end_short_backtrace::h[0-9a-f]+#,
    qr#                    at /rustc/[0-9a-f]+/library/std/src/sys_common/backtrace.rs:[0-9]+:[0-9]+#,
    qr#    5: 0x[0-9a-f]+ - rust_begin_unwind#,
    qr#                    at /rustc/[0-9a-f]+/library/std/src/panicking.rs:[0-9]+:[0-9]+#,
    qr#    6: 0x[0-9a-f]+ - core::panicking::panic_fmt::h[0-9a-f]+#,
    qr#                    at /rustc/[0-9a-f]+/library/core/src/panicking.rs:[0-9]+:[0-9]+#,
    qr#    7: 0x[0-9a-f]+ - <unknown>!proxy_wasm::hostcalls::set_property::h[0-9a-f]+#,
    qr#    8: 0x[0-9a-f]+ - <unknown>!hostcalls::tests::test_set_property::h[0-9a-f]+#,
    qr#    9: 0x[0-9a-f]+ - <unknown>!hostcalls::types::test_http::TestHttp::exec_tests::h[0-9a-f]+#,
    qr#   10: 0x[0-9a-f]+ - <unknown>!hostcalls::filter::<impl proxy_wasm::traits::HttpContext for hostcalls::types::test_http::TestHttp>::on_http_request_headers::h[0-9a-f]+#,
    qr#   11: 0x[0-9a-f]+ - <unknown>!proxy_wasm::dispatcher::Dispatcher::on_http_request_headers::h[0-9a-f]+#,
    qr#   12: 0x[0-9a-f]+ - <unknown>!proxy_on_request_headers#,
]
--- no_error_log
stub26
stub27
stub28
stub29
stub30



=== TEST 4: backtraces directive - wasmer 'auto', backtraces 'off'
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
        proxy_wasm hostcalls 'test=/t/set_property name=nonexistent_property';
        echo ok;
    }
--- error_code: 500
--- response_body eval
qr/500 Internal Server Error/
--- error_log eval
[
    qr/\[crit\] .*? panicked at 'unexpected status: 1'/,
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
    qr#	10: func [0-9]+ @ 0x[0-9a-f]+#,
    qr#	11: func [0-9]+ @ 0x[0-9a-f]+#,
    qr#	12: func [0-9]+ @ 0x[0-9a-f]+#,
]
--- no_error_log
stub19
stub20
stub21
stub22
stub23
stub24
stub25
stub26
stub27
stub28
stub29
stub30



=== TEST 5: backtraces directive - wasmer 'cranelift', backtraces 'on'
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
        proxy_wasm hostcalls 'test=/t/set_property name=nonexistent_property';
        echo ok;
    }
--- error_code: 500
--- response_body eval
qr/500 Internal Server Error/
--- error_log eval
[
    qr/\[crit\] .*? panicked at 'unexpected status: 1'/,
    qr/\[error\] .*? \[wasm\] unreachable/,
    qr/Backtrace:/,
    qr#	0: __rust_start_panic @ 0x[0-9a-f]+#,
    qr#	1: rust_panic @ 0x[0-9a-f]+#,
    qr#	2: std::panicking::rust_panic_with_hook::h[0-9a-f]+ @ 0x[0-9a-f]+#,
    qr#	3: std::panicking::begin_panic_handler::\{\{closure\}\}::h[0-9a-f]+ @ 0x[0-9a-f]+#,
    qr#	4: std::sys_common::backtrace::__rust_end_short_backtrace::h[0-9a-f]+ @ 0x[0-9a-f]+#,
    qr#	5: rust_begin_unwind @ 0x[0-9a-f]+#,
    qr#	6: core::panicking::panic_fmt::h[0-9a-f]+ @ 0x[0-9a-f]+#,
    qr#	7: proxy_wasm::hostcalls::set_property::h[0-9a-f]+ @ 0x[0-9a-f]+#,
    qr#	8: hostcalls::tests::test_set_property::h[0-9a-f]+ @ 0x[0-9a-f]+#,
    qr#	9: hostcalls::types::test_http::TestHttp::exec_tests::h[0-9a-f]+ @ 0x[0-9a-f]+#,
    qr#	10: hostcalls::filter::<impl proxy_wasm::traits::HttpContext for hostcalls::types::test_http::TestHttp>::on_http_request_headers::h[0-9a-f]+ @ 0x[0-9a-f]+#,
    qr#	11: proxy_wasm::dispatcher::Dispatcher::on_http_request_headers::h[0-9a-f]+ @ 0x[0-9a-f]+#,
    qr#	12: proxy_on_request_headers @ 0x[0-9a-f]+#,
]
--- no_error_log
stub19
stub20
stub21
stub22
stub23
stub24
stub25
stub26
stub27
stub28
stub29
stub30



=== TEST 6: backtraces directive - wasmer 'singlepass', backtraces 'on'
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
        proxy_wasm hostcalls 'test=/t/set_property name=nonexistent_property';
        echo ok;
    }
--- error_code: 500
--- response_body eval
qr/500 Internal Server Error/
--- error_log eval
[
    qr/\[crit\] .*? panicked at 'unexpected status: 1'/,
    qr/\[error\] .*? \[wasm\] unreachable/,
    qr/Backtrace:/,
    qr#	0: __rust_start_panic @ 0x[0-9a-f]+#,
    qr#	1: rust_panic @ 0x[0-9a-f]+#,
    qr#	2: std::panicking::rust_panic_with_hook::h[0-9a-f]+ @ 0x[0-9a-f]+#,
    qr#	3: std::panicking::begin_panic_handler::\{\{closure\}\}::h[0-9a-f]+ @ 0x[0-9a-f]+#,
    qr#	4: std::sys_common::backtrace::__rust_end_short_backtrace::h[0-9a-f]+ @ 0x[0-9a-f]+#,
    qr#	5: rust_begin_unwind @ 0x[0-9a-f]+#,
    qr#	6: core::panicking::panic_fmt::h[0-9a-f]+ @ 0x[0-9a-f]+#,
    qr#	7: proxy_wasm::hostcalls::set_property::h[0-9a-f]+ @ 0x[0-9a-f]+#,
    qr#	8: hostcalls::tests::test_set_property::h[0-9a-f]+ @ 0x[0-9a-f]+#,
    qr#	9: hostcalls::types::test_http::TestHttp::exec_tests::h[0-9a-f]+ @ 0x[0-9a-f]+#,
    qr#	10: hostcalls::filter::<impl proxy_wasm::traits::HttpContext for hostcalls::types::test_http::TestHttp>::on_http_request_headers::h[0-9a-f]+ @ 0x[0-9a-f]+#,
    qr#	11: proxy_wasm::dispatcher::Dispatcher::on_http_request_headers::h[0-9a-f]+ @ 0x[0-9a-f]+#,
    qr#	12: proxy_on_request_headers @ 0x[0-9a-f]+#,
]
--- no_error_log
stub19
stub20
stub21
stub22
stub23
stub24
stub25
stub26
stub27
stub28
stub29
stub30



=== TEST 7: backtraces directive - wasmer 'llvm', backtraces 'on'
--- skip_eval: 30: $::nginxV !~ m/wasmer/
--- load_nginx_modules: ngx_http_echo_module
--- SKIP
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
        proxy_wasm hostcalls 'test=/t/set_property name=nonexistent_property';
        echo ok;
    }
--- error_code: 500
--- response_body eval
qr/500 Internal Server Error/
--- error_log eval
[
    qr/\[crit\] .*? panicked at 'unexpected status: 1'/,
    qr/\[error\] .*? \[wasm\] unreachable/,
    qr/Backtrace:/,
    qr#	0: __rust_start_panic @ 0x[0-9a-f]+#,
    qr#	1: rust_panic @ 0x[0-9a-f]+#,
    qr#	2: std::panicking::rust_panic_with_hook::h[0-9a-f]+ @ 0x[0-9a-f]+#,
    qr#	3: std::panicking::begin_panic_handler::\{\{closure\}\}::h[0-9a-f]+ @ 0x[0-9a-f]+#,
    qr#	4: std::sys_common::backtrace::__rust_end_short_backtrace::h[0-9a-f]+ @ 0x[0-9a-f]+#,
    qr#	5: rust_begin_unwind @ 0x[0-9a-f]+#,
    qr#	6: core::panicking::panic_fmt::h[0-9a-f]+ @ 0x[0-9a-f]+#,
    qr#	7: proxy_wasm::hostcalls::set_property::h[0-9a-f]+ @ 0x[0-9a-f]+#,
    qr#	8: hostcalls::tests::test_set_property::h[0-9a-f]+ @ 0x[0-9a-f]+#,
    qr#	9: hostcalls::types::test_http::TestHttp::exec_tests::h[0-9a-f]+ @ 0x[0-9a-f]+#,
    qr#	10: hostcalls::filter::<impl proxy_wasm::traits::HttpContext for hostcalls::types::test_http::TestHttp>::on_http_request_headers::h[0-9a-f]+ @ 0x[0-9a-f]+#,
    qr#	11: proxy_wasm::dispatcher::Dispatcher::on_http_request_headers::h[0-9a-f]+ @ 0x[0-9a-f]+#,
    qr#	12: proxy_on_request_headers @ 0x[0-9a-f]+#,
]
--- no_error_log
stub19
stub20
stub21
stub22
stub23
stub24
stub25
stub26
stub27
stub28
stub29
stub30



=== TEST 8: backtraces directive - v8 'auto', backtraces 'off'
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
        proxy_wasm hostcalls 'test=/t/set_property name=nonexistent_property';
        echo ok;
    }
--- error_code: 500
--- response_body eval
qr/500 Internal Server Error/
--- error_log eval
[
    qr/\[crit\] .*? panicked at 'unexpected status: 1'/,
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
    qr#	10: func [0-9]+ @ 0x[0-9a-f]+#,
    qr#	11: func [0-9]+ @ 0x[0-9a-f]+#,
    qr#	12: func [0-9]+ @ 0x[0-9a-f]+#,
]
--- no_error_log
stub19
stub20
stub21
stub22
stub23
stub24
stub25
stub26
stub27
stub28
stub29
stub30



=== TEST 9: backtraces directive - v8 'auto', backtraces 'on'
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
        proxy_wasm hostcalls 'test=/t/set_property name=nonexistent_property';
        echo ok;
    }
--- error_code: 500
--- response_body eval
qr/500 Internal Server Error/
--- error_log eval
[
    qr/\[crit\] .*? panicked at 'unexpected status: 1'/,
    qr/\[error\] .*? \[wasm\] Uncaught RuntimeError: unreachable/,
    qr/Backtrace:/,
    qr#	0: __rust_start_panic @ 0x[0-9a-f]+#,
    qr#	1: rust_panic @ 0x[0-9a-f]+#,
    qr#	2: std::panicking::rust_panic_with_hook::h[0-9a-f]+ @ 0x[0-9a-f]+#,
    qr#	3: std::panicking::begin_panic_handler::\{\{closure\}\}::h[0-9a-f]+ @ 0x[0-9a-f]+#,
    qr#	4: std::sys_common::backtrace::__rust_end_short_backtrace::h[0-9a-f]+ @ 0x[0-9a-f]+#,
    qr#	5: rust_begin_unwind @ 0x[0-9a-f]+#,
    qr#	6: core::panicking::panic_fmt::h[0-9a-f]+ @ 0x[0-9a-f]+#,
    qr#	7: proxy_wasm::hostcalls::set_property::h[0-9a-f]+ @ 0x[0-9a-f]+#,
    qr#	8: hostcalls::tests::test_set_property::h[0-9a-f]+ @ 0x[0-9a-f]+#,
    qr#	9: hostcalls::types::test_http::TestHttp::exec_tests::h[0-9a-f]+ @ 0x[0-9a-f]+#,
    qr#	10: hostcalls::filter::<impl proxy_wasm::traits::HttpContext for hostcalls::types::test_http::TestHttp>::on_http_request_headers::h[0-9a-f]+ @ 0x[0-9a-f]+#,
    qr#	11: proxy_wasm::dispatcher::Dispatcher::on_http_request_headers::h[0-9a-f]+ @ 0x[0-9a-f]+#,
    qr#	12: proxy_on_request_headers @ 0x[0-9a-f]+#,
]
--- no_error_log
stub19
stub20
stub21
stub22
stub23
stub24
stub25
stub26
stub27
stub28
stub29
stub30
