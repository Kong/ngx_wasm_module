# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

skip_valgrind('wasmtime');

plan tests => repeat_each() * (blocks() * 6);

run_tests();

__DATA__

=== TEST 1: proxy_wasm - set_property() wasmx - host property on: request_headers, request_body, response_headers, response_body, log
--- wasm_modules: hostcalls
--- load_nginx_modules: ngx_http_echo_module
--- config
    location /t {
        proxy_wasm hostcalls 'on=request_headers \
                              test=/t/set_property \
                              name=wasmx.my_var \
                              set=2';
        proxy_wasm hostcalls 'on=request_body \
                              test=/t/set_property \
                              name=wasmx.my_var \
                              set=3';
        echo ok;
        proxy_wasm hostcalls 'on=response_headers \
                              test=/t/set_property \
                              name=wasmx.my_var \
                              set=4';
        proxy_wasm hostcalls 'on=response_body \
                              test=/t/set_property \
                              name=wasmx.my_var \
                              set=5';
        proxy_wasm hostcalls 'on=log \
                              test=/t/set_property \
                              name=wasmx.my_var \
                              set=6';
    }
--- request
POST /t
hello
--- response_body
ok
--- grep_error_log eval: qr/\[info\] .*? (old|new): ".*?"/
--- grep_error_log_out eval
qr/\A\[info\] .*? new: "2"
\[info\] .*? old: "2"
\[info\] .*? new: "3"
\[info\] .*? old: "3"
\[info\] .*? new: "4"
\[info\] .*? old: "4"
\[info\] .*? new: "5"
\[info\] .*? old: "5"
\[info\] .*? new: "5"
\[info\] .*? old: "5"
\[info\] .*? new: "6"/
--- no_error_log
[error]
[crit]
[emerg]



=== TEST 2: proxy_wasm - set_property() wasmx - unset a host property on: request_headers
--- wasm_modules: hostcalls
--- load_nginx_modules: ngx_http_echo_module
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/set_property \
                              name=wasmx.something \
                              set=123';
        proxy_wasm hostcalls 'test=/t/set_property \
                              name=wasmx.something \
                              unset=1';
        echo ok;
    }
--- error_log eval
[
    qr/\[info\] .*? old: unset/,
    qr/\[info\] .*? new: "123"/,
    qr/\[info\] .*? old: "123"/,
    qr/\[info\] .*? new: unset/,
]
--- no_error_log
[error]



=== TEST 3: proxy_wasm - set_property() wasmx - set a host property to an empty string
--- wasm_modules: hostcalls
--- load_nginx_modules: ngx_http_echo_module
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/set_property \
                              name=wasmx.something \
                              set=123';
        proxy_wasm hostcalls 'test=/t/set_property \
                              name=wasmx.something \
                              set=';
        echo ok;
    }
--- error_log eval
[
    qr/\[info\] .*? old: unset/,
    qr/\[info\] .*? new: "123"/,
    qr/\[info\] .*? old: "123"/,
    qr/\[info\] .*? new: ""/,
]
--- no_error_log
[error]



=== TEST 4: proxy_wasm - set_property() wasmx - unknown host property on: request_headers

Does not fail when the property is not found.

--- wasm_modules: hostcalls
--- load_nginx_modules: ngx_http_echo_module
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/set_property \
                              name=wasmx.undefined_property';
        echo ok;
    }
--- error_log eval
[
    qr/\[info\] .*? old: unset/,
    qr/\[info\] .*? new: unset/,
]
--- no_error_log
[error]
[crit]
[emerg]



=== TEST 5: proxy_wasm - set_property() wasmx - not available on: tick (isolation: global)

HTTP 500 since instance recycling happens on next request, and
isolation is global (single instance for root/request).

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
    set $my_var 123;

    location /t {
        proxy_wasm hostcalls 'tick_period=10 \
                              on_tick=set_property \
                              name=wasmx.my_var \
                              show_old=false \
                              set=456';
        echo_sleep 0.150;
        echo ok;
    }
--- error_code: 500
--- ignore_response_body
--- error_log eval
[
    qr/\[info\] .*? \[hostcalls\] on_tick/,
    qr/\[error\] .*? cannot set host properties outside of a request/,
    qr/\[crit\] .*? panicked at 'unexpected status: 10'/,
    qr/(.*?(Uncaught RuntimeError: )?unreachable|wasm trap: wasm `unreachable` instruction executed).*/,
]
--- no_error_log
[emerg]



=== TEST 6: proxy_wasm - set_property() wasmx - not available on: tick (isolation: stream)

HTTP 200 since the root and request instances are different.

on_tick runs on the root context, so it does not have access to
ngx_http_* calls.

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
    set $my_var 123;

    location /t {
        proxy_wasm_isolation stream;

        proxy_wasm hostcalls 'tick_period=10 \
                              on_tick=set_property \
                              name=wasmx.my_var \
                              show_old=false \
                              set=456';
        echo_sleep 0.150;
        echo ok;
    }
--- ignore_response_body
--- error_log eval
[
    qr/\[info\] .*? \[hostcalls\] on_tick/,
    qr/\[error\] .*? cannot set host properties outside of a request/,
    qr/\[crit\] .*? panicked at 'unexpected status: 10'/,
    qr/(.*?(Uncaught RuntimeError: )?unreachable|wasm trap: wasm `unreachable` instruction executed).*/,
]
--- no_error_log
[emerg]
