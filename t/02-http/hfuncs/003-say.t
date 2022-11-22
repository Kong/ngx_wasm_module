# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

plan tests => repeat_each() * (blocks() * 3);

run_tests();

__DATA__

=== TEST 1: say: produce response in 'rewrite' phase
--- wasm_modules: ngx_rust_tests
--- config
    location /t {
        wasm_call rewrite ngx_rust_tests say_hello;
    }
--- response_body
hello say
--- no_error_log
[error]



=== TEST 2: say: produce response in 'content' phase
--- wasm_modules: ngx_rust_tests
--- config
    location /t {
        wasm_call content ngx_rust_tests say_hello;
    }
--- response_body
hello say
--- no_error_log
[error]



=== TEST 3: say: produce response in 'header_filter' phase
--- load_nginx_modules: ngx_http_echo_module ngx_http_headers_more_filter_module
--- wasm_modules: ngx_rust_tests
--- config
    location /t {
        # force the Content-Length header value since say
        # will set it only once otherwise
        more_set_headers 'Content-Length: 20';
        wasm_call content ngx_rust_tests say_hello;
        wasm_call header_filter ngx_rust_tests say_hello;
    }
--- response_body
hello say
hello say
--- no_error_log
[error]



=== TEST 4: say: 'log' phase
Should produce a trap.

Wasmtime trap format:
    [error] error while executing ...
    [stacktrace]
    Caused by:
        [trap msg]

Wasmer trap format:
    [error] [trap msg]

V8 trap format:
    [error] Uncaught RuntimeError: [trap msg]
--- wasm_modules: ngx_rust_tests
--- config
    location /t {
        return 200;
        wasm_call log ngx_rust_tests say_hello;
    }
--- ignore_response_body
--- grep_error_log eval: qr/(\[error\] .*?|.*?host trap).*/
--- grep_error_log_out eval
qr/.*?(\[error\]|Uncaught RuntimeError: |\s+)host trap \(bad usage\): headers already sent.*/
--- no_error_log
[crit]
