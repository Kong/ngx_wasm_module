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
        proxy_wasm hostcalls 'test=/t/log/levels';
        echo ok;
    }
--- response_body
ok
--- error_log eval
[
    qr/\[debug\] .*? \*\d+ proxy_log trace$/,
    qr/\[info\] .*? \*\d+ proxy_log info, client:/,
    qr/\[warn\] .*? \*\d+ proxy_log warn, client:/,
    qr/\[error\] .*? \*\d+ proxy_log error, client:/,
    qr/\[crit\] .*? \*\d+ proxy_log critical, client:/
]
--- no_error_log
[alert]
stub1



=== TEST 2: proxy_wasm - proxy_log() x on_phases
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t/A {
        proxy_wasm hostcalls 'on=request_headers \
                              test=/t/log/levels';
        echo A;
    }

    location /t/B {
        proxy_wasm hostcalls 'on=response_headers \
                              test=/t/log/levels';
        echo B;
    }

    location /t {
        proxy_wasm hostcalls 'on=log \
                              test=/t/log/levels';
        echo_location /t/A;
        echo_location /t/B;
        echo C;
    }
--- request
GET /t
--- response_body
A
B
C
--- error_log eval
[
    qr/testing in "RequestHeaders"/,
    qr/\[error\] .*? proxy_log error/,
    qr/testing in "ResponseHeaders"/,
    qr/\[error\] .*? proxy_log error/,
    qr/testing in "Log"/,
    qr/\[error\] .*? proxy_log error/,
]
--- no_error_log
[alert]
