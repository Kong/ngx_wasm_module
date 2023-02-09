# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

skip_valgrind();

plan tests => repeat_each() * (blocks() * 5);

run_tests();

__DATA__

=== TEST 1: proxy_wasm - get_property() reports if a host property is not found
--- wasm_modules: hostcalls
--- load_nginx_modules: ngx_http_echo_module
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/log/property name=wasmx.nonexistent_property';
        echo ok;
    }
--- ignore_response_body
--- error_log eval
qr/\[info\] .*? property not found: wasmx.nonexistent_property/
--- no_error_log
[error]
[emerg]



=== TEST 2: proxy_wasm - get_property() handles non-existent variable shorter than host_prefix
--- wasm_modules: hostcalls
--- load_nginx_modules: ngx_http_echo_module
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/log/property name=was';
        echo ok;
    }
--- response_body
ok
--- error_log
[wasm] property "was" not found
--- no_error_log
[crit]
[emerg]



=== TEST 3: proxy_wasm - get_property() works on_request_headers
--- wasm_modules: hostcalls
--- load_nginx_modules: ngx_http_echo_module
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/set_property name=wasmx.something set=123';
        proxy_wasm hostcalls 'on=request_headers test=/t/log/property name=wasmx.something';
        echo ok;
    }
--- ignore_response_body
--- error_log eval
[
    qr/\[info\] .*? \[hostcalls\] on_request_headers/,
    qr/\[info\] .*? wasmx.something: 123/,
]
--- no_error_log
[error]
[emerg]



=== TEST 4: proxy_wasm - get_property() works on_request_body
--- wasm_modules: hostcalls
--- load_nginx_modules: ngx_http_echo_module
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/set_property name=wasmx.something set=123';
        proxy_wasm hostcalls 'on=request_body test=/t/log/property name=wasmx.something';
        echo ok;
    }
--- request
POST /t/echo/body
hello world
--- ignore_response_body
--- error_log eval
[
    qr/\[info\] .*? \[hostcalls\] on_request_body/,
    qr/\[info\] .*? wasmx.something: 123/,
]
--- no_error_log
[error]
[emerg]



=== TEST 5: proxy_wasm - get_property() works on_response_headers
--- wasm_modules: hostcalls
--- load_nginx_modules: ngx_http_echo_module
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/set_property name=wasmx.something set=123';
        proxy_wasm hostcalls 'on=response_headers test=/t/log/property name=wasmx.something';
        echo ok;
    }
--- ignore_response_body
--- error_log eval
[
    qr/\[info\] .*? \[hostcalls\] on_response_headers/,
    qr/\[info\] .*? wasmx.something: 123/,
]
--- no_error_log
[error]
[emerg]



=== TEST 6: proxy_wasm - get_property() works on_response_body
--- wasm_modules: hostcalls
--- load_nginx_modules: ngx_http_echo_module
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/set_property name=wasmx.something set=123';
        proxy_wasm hostcalls 'on=response_body test=/t/log/property name=wasmx.something';
        echo ok;
    }
--- ignore_response_body
--- error_log eval
[
    qr/\[info\] .*? \[hostcalls\] on_response_body/,
    qr/\[info\] .*? wasmx.something: 123/,
]
--- no_error_log
[error]
[emerg]



=== TEST 7: proxy_wasm - get_property() works on_log
--- wasm_modules: hostcalls
--- load_nginx_modules: ngx_http_echo_module
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/set_property name=wasmx.something set=123';
        proxy_wasm hostcalls 'on=log test=/t/log/property name=wasmx.something';
        echo ok;
    }
--- ignore_response_body
--- error_log eval
[
    qr/\[info\] .*? \[hostcalls\] on_log/,
    qr/\[info\] .*? wasmx.something: 123/,
]
--- no_error_log
[error]
[emerg]



=== TEST 8: proxy_wasm - get_property() for host properties does not work on_tick (isolation: global)
HTTP 500 since instance recycling happens on next request, and isolation is global (single instance for root/request)

Wasmtime trap format:
    [error] error while executing ...
    [stacktrace]
    Caused by:
        [trap msg]

Wasmer trap format:
    [error] [trap msg]

V8 trap format:
    [error] Uncaught RuntimeError: [trap msg]
--- wasm_modules: hostcalls
--- load_nginx_modules: ngx_http_echo_module
--- config
    location /t {
        proxy_wasm hostcalls 'tick_period=10 \
                              on_tick=log_property \
                              name=wasmx.my_var';
        echo_sleep 0.150;
        echo ok;
    }
--- error_code: 500
--- ignore_response_body
--- error_log eval
[
    qr/\[info\] .*? \[hostcalls\] on_tick/,
    qr/\[error\] .*? cannot get host properties outside of a request/,
    qr/\[crit\] .*? panicked at 'unexpected status: 10'/,
    qr/(.*?(Uncaught RuntimeError: )?unreachable|wasm trap: wasm `unreachable` instruction executed).*/
]
--- no_error_log
[emerg]



=== TEST 9: proxy_wasm - set_property() for host properties does not work on_tick (isolation: stream)
Wasmtime trap format:
    [error] error while executing ...
    [stacktrace]
    Caused by:
        [trap msg]

Wasmer trap format:
    [error] [trap msg]

V8 trap format:
    [error] Uncaught RuntimeError: [trap msg]
--- wasm_modules: hostcalls
--- load_nginx_modules: ngx_http_echo_module
--- config
    location /t {
        proxy_wasm_isolation stream;

        proxy_wasm hostcalls 'tick_period=10 \
                              on_tick=log_property \
                              name=wasmx.my_var';
        echo_sleep 0.150;
        echo ok;
    }
--- ignore_response_body
--- error_log eval
[
    qr/\[info\] .*? \[hostcalls\] on_tick/,
    qr/\[error\] .*? cannot get host properties outside of a request/,
    qr/\[crit\] .*? panicked at 'unexpected status: 10'/,
    qr/(.*?(Uncaught RuntimeError: )?unreachable|wasm trap: wasm `unreachable` instruction executed).*/

]
