# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

skip_valgrind();

plan tests => repeat_each() * (blocks() * 4);

run_tests();

__DATA__

=== TEST 1: proxy_wasm - get_property() gets Nginx $hostname variable
--- wasm_modules: hostcalls
--- load_nginx_modules: ngx_http_echo_module
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/log/property name=ngx.hostname';
        echo ok;
    }
--- ignore_response_body
--- error_log eval
qr/\[info\] .*? ngx.hostname: [a-z0-9]+/
--- no_error_log
[error]
[crit]



=== TEST 2: proxy_wasm - get_property() gets an Nginx $pid variable
All get_property calls for Nginx variables return strings, so this
should print the $pid as ASCII numbers.
--- wasm_modules: hostcalls
--- load_nginx_modules: ngx_http_echo_module
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/log/property name=ngx.pid';
        echo ok;
    }
--- ignore_response_body
--- error_log eval
qr/\[info\] .*? ngx.pid: [0-9]+/
--- no_error_log
[error]
[crit]



=== TEST 3: proxy_wasm - get_property() reports if an ngx.* property is not found
--- wasm_modules: hostcalls
--- load_nginx_modules: ngx_http_echo_module
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/log/property name=ngx.nonexistent_property';
        echo ok;
    }
--- ignore_response_body
--- error_log eval
qr/\[info\] .*? property not found: ngx.nonexistent_property/
--- no_error_log
[error]
[crit]



=== TEST 4: proxy_wasm - get_property() reports if a generic property is not found
--- wasm_modules: hostcalls
--- load_nginx_modules: ngx_http_echo_module
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/log/property name=nonexistent_property';
        echo ok;
    }
--- response_body
ok
--- error_log eval
qr/\[info\] .*? property not found: nonexistent_property,/
--- no_error_log
[crit]



=== TEST 5: proxy_wasm - get_property() handles non-existent variable shorter than ngx_prefix
--- wasm_modules: hostcalls
--- load_nginx_modules: ngx_http_echo_module
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/log/property name=n';
        echo ok;
    }
--- response_body
ok
--- error_log eval
qr/\[info\] .*? property not found: n,/
--- no_error_log
[crit]



=== TEST 6: proxy_wasm - get_property() works on_request_headers
--- wasm_modules: hostcalls
--- load_nginx_modules: ngx_http_echo_module
--- config
    location /t {
        proxy_wasm hostcalls 'on=request_headers test=/t/log/property name=ngx.hostname';
        echo ok;
    }
--- ignore_response_body
--- error_log eval
[
    qr/\[info\] .*? \[hostcalls\] on_request_headers/,
    qr/\[info\] .*? ngx.hostname: [a-z0-9]+/,
]
--- no_error_log
[error]



=== TEST 7: proxy_wasm - get_property() works on_request_body
--- wasm_modules: hostcalls
--- load_nginx_modules: ngx_http_echo_module
--- config
    location /t {
        proxy_wasm hostcalls 'on=request_body test=/t/log/property name=ngx.hostname';
        echo ok;
    }
--- request
POST /t/echo/body
hello world
--- ignore_response_body
--- error_log eval
[
    qr/\[info\] .*? \[hostcalls\] on_request_body/,
    qr/\[info\] .*? ngx.hostname: [a-z0-9]+/,
]
--- no_error_log
[error]



=== TEST 8: proxy_wasm - get_property() works on_response_headers
--- wasm_modules: hostcalls
--- load_nginx_modules: ngx_http_echo_module
--- config
    location /t {
        proxy_wasm hostcalls 'on=response_headers test=/t/log/property name=ngx.hostname';
        echo ok;
    }
--- ignore_response_body
--- error_log eval
[
    qr/\[info\] .*? \[hostcalls\] on_response_headers/,
    qr/\[info\] .*? ngx.hostname: [a-z0-9]+/,
]
--- no_error_log
[error]



=== TEST 9: proxy_wasm - get_property() works on_response_body
--- wasm_modules: hostcalls
--- load_nginx_modules: ngx_http_echo_module
--- config
    location /t {
        proxy_wasm hostcalls 'on=response_body test=/t/log/property name=ngx.hostname';
        echo ok;
    }
--- ignore_response_body
--- error_log eval
[
    qr/\[info\] .*? \[hostcalls\] on_response_body/,
    qr/\[info\] .*? ngx.hostname: [a-z0-9]+/,
]
--- no_error_log
[error]



=== TEST 10: proxy_wasm - get_property() works on_log
--- wasm_modules: hostcalls
--- load_nginx_modules: ngx_http_echo_module
--- config
    location /t {
        proxy_wasm hostcalls 'on=log test=/t/log/property name=ngx.hostname';
        echo ok;
    }
--- ignore_response_body
--- error_log eval
[
    qr/\[info\] .*? \[hostcalls\] on_log/,
    qr/\[info\] .*? ngx.hostname: [a-z0-9]+/,
]
--- no_error_log
[error]



=== TEST 11: proxy_wasm - get_property() for ngx.* does not work on_tick
on_tick runs on the root context, so it does not have access to ngx_http_* calls.

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
        proxy_wasm hostcalls 'tick_period=10 on_tick=log_property name=ngx.hostname';
        echo_sleep 0.150;
        echo ok;
    }
--- error_code: 500
--- ignore_response_body
--- error_log eval
[
    qr/\[info\] .*? \[hostcalls\] on_tick/,
    qr/\[error\] .*? cannot get ngx properties outside of a request/,
    qr/\[crit\] .*? panicked at 'unexpected status: 10'/,
]
