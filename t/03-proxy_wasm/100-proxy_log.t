# vim:set ft= ts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

skip_valgrind();

plan tests => repeat_each() * (blocks() * 9);

run_tests();

__DATA__

=== TEST 1: proxy_wasm - proxy_log() logs all levels
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls;
        echo ok;
    }
--- request
GET /t/log/levels
--- response_body
ok
--- error_log eval
[
    qr/\[debug\] .*? \[wasm\] proxy_log trace$/,
    qr/\[info\] .*? \[wasm\] proxy_log info \</,
    qr/\[warn\] .*? \[wasm\] proxy_log warn \</,
    qr/\[error\] .*? \[wasm\] proxy_log error \</,
    qr/\[crit\] .*? \[wasm\] proxy_log critical \</
]
--- no_error_log
[alert]
stub1



=== TEST 2: proxy_wasm - proxy_log() x on_phases
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t/A {
        proxy_wasm hostcalls on_phase=http_request_headers;
        echo A;
    }

    location /t/B {
        proxy_wasm hostcalls on_phase=http_response_headers;
        echo B;
    }

    location /t {
        proxy_wasm hostcalls on_phase=log;
        echo_location /t/A;
        echo_location /t/B;
        echo C;
    }
--- request
GET /t
--- more_headers
PWM-Test-Case: /t/log/levels
--- response_body
A
B
C
--- error_log eval
[
    qr/\[wasm\] #\d+ entering "HttpRequestHeaders"/,
    qr/\[error\] .*? \[wasm\] proxy_log error/,
    qr/\[wasm\] #\d+ entering "HttpResponseHeaders"/,
    qr/\[error\] .*? \[wasm\] proxy_log error/,
    qr/\[wasm\] #\d+ entering "Log"/,
    qr/\[error\] .*? \[wasm\] proxy_log error/,
]
--- no_error_log
[alert]
