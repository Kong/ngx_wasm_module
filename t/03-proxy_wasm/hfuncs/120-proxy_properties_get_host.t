# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

skip_valgrind('wasmtime');

plan tests => repeat_each() * (blocks() * 5);

run_tests();

__DATA__

=== TEST 1: proxy_wasm - get_property() wasmx - property on: request_headers, request_body, response_headers, response_body, log
--- wasm_modules: hostcalls
--- load_nginx_modules: ngx_http_echo_module
--- config
    location /t {
        proxy_wasm hostcalls 'on=request_headers \
                              test=/t/set_property \
                              name=wasmx.something \
                              set=1';
        proxy_wasm hostcalls 'on=request_headers \
                              test=/t/log/property \
                              name=wasmx.something';
        proxy_wasm hostcalls 'on=request_body \
                              test=/t/set_property \
                              name=wasmx.something \
                              set=2';
        proxy_wasm hostcalls 'on=request_body \
                              test=/t/log/property \
                              name=wasmx.something';
        echo ok;
        proxy_wasm hostcalls 'on=response_headers \
                              test=/t/set_property \
                              name=wasmx.something \
                              set=3';
        proxy_wasm hostcalls 'on=response_headers \
                              test=/t/log/property \
                              name=wasmx.something';
        proxy_wasm hostcalls 'on=response_body \
                              test=/t/set_property \
                              name=wasmx.something \
                              set=4';
        proxy_wasm hostcalls 'on=response_body \
                              test=/t/log/property \
                              name=wasmx.something';
        proxy_wasm hostcalls 'on=log \
                              test=/t/set_property \
                              name=wasmx.something \
                              set=5';
        proxy_wasm hostcalls 'on=log \
                              test=/t/log/property \
                              name=wasmx.something';
    }
--- request
POST /t
hello
--- response_body
ok
--- grep_error_log eval: qr/\[info\].*?(RequestHeaders|RequestBody|ResponseHeaders|ResponseBody|Log|wasmx\.something).*/
--- grep_error_log_out eval
qr/.*?testing in "RequestHeaders".*
.*?testing in "RequestHeaders".*
.*?wasmx\.something: 1.*
.*?testing in "RequestBody".*
.*?testing in "RequestBody".*
.*?wasmx\.something: 2.*
.*?testing in "ResponseHeaders".*
.*?testing in "ResponseHeaders".*
.*?wasmx\.something: 3.*
.*?testing in "ResponseBody".*
.*?testing in "ResponseBody".*
.*?wasmx\.something: 4.*
.*?testing in "ResponseBody".*
.*?testing in "ResponseBody".*
.*?wasmx\.something: 4.*
.*?testing in "Log".*
.*?testing in "Log".*
.*?wasmx\.something: 5.*/
--- no_error_log
[error]
[crit]



=== TEST 2: proxy_wasm - get_property() wasmx - unknown host property on: request_headers

TODO: is this behavior correct?

--- wasm_modules: hostcalls
--- load_nginx_modules: ngx_http_echo_module
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/log/property \
                              name=wasmx.nonexistent_property';
        echo ok;
    }
--- ignore_response_body
--- error_log eval
qr/\[info\] .*? property not found: wasmx.nonexistent_property,/
--- no_error_log
[error]
[emerg]



=== TEST 3: proxy_wasm - get_property() wasmx - unknown host property with name shorter than 'host_prefix'

NGX_WASM_HOST_PROPERTY_NAMESPACE="wasmx"

--- wasm_modules: hostcalls
--- load_nginx_modules: ngx_http_echo_module
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/log/property \
                              name=was';
        echo ok;
    }
--- response_body
ok
--- error_log eval
qr/\[info\] .*? property not found: was,/
--- no_error_log
[crit]
[emerg]



=== TEST 4: proxy_wasm - get_property() wasmx - not available on: tick (isolation: global)

HTTP 500 since instance recycling happens on next request, and isolation
is global (single instance for root/request).

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
    qr/(.*?(Uncaught RuntimeError: )?unreachable|wasm trap: wasm `unreachable` instruction executed).*/,
]
--- no_error_log
[emerg]



=== TEST 5: proxy_wasm - get_property() wasmx - not available on: tick (isolation: stream)

HTTP 200 since the root and request instances are different.

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
    qr/(.*?(Uncaught RuntimeError: )?unreachable|wasm trap: wasm `unreachable` instruction executed).*/,
]
