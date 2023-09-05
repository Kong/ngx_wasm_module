# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

skip_valgrind('wasmtime');

plan tests => repeat_each() * (blocks() * 5);

run_tests();

__DATA__

=== TEST 1: proxy_wasm - set_property() ngx.* - indexed variable on: request_headers, request_body, response_headers, response_body, log
--- wasm_modules: hostcalls
--- load_nginx_modules: ngx_http_echo_module
--- config
    set $my_var 1;

    location /t {
        proxy_wasm hostcalls 'on=request_headers \
                              test=/t/set_property \
                              name=ngx.my_var \
                              set=2';
        proxy_wasm hostcalls 'on=request_body \
                              test=/t/set_property \
                              name=ngx.my_var \
                              set=3';
        echo ok;
        proxy_wasm hostcalls 'on=response_headers \
                              test=/t/set_property \
                              name=ngx.my_var \
                              set=4';
        proxy_wasm hostcalls 'on=response_body \
                              test=/t/set_property \
                              name=ngx.my_var \
                              set=5';
        proxy_wasm hostcalls 'on=log \
                              test=/t/set_property \
                              name=ngx.my_var \
                              set=6';
    }
--- request
POST /t
hello
--- response_body
ok
--- grep_error_log eval: qr/\[info\] .*? (old|new): ".*?"/
--- grep_error_log_out eval
qr/\A\[info\] .*? old: "1"
\[info\] .*? new: "2"
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
[emerg]
[alert]



=== TEST 2: proxy_wasm - set_property() ngx.* - indexed variable with set_handler on: request_headers
--- wasm_modules: hostcalls
--- load_nginx_modules: ngx_http_echo_module
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/set_property \
                              name=ngx.args \
                              set=456';
        echo ok;
    }
--- request
GET /t?hello=world
--- error_log eval
[
    qr/\[info\] .*? old: "hello=world"/,
    qr/\[info\] .*? new: "456"/,
]
--- no_error_log
[error]
[crit]



=== TEST 3: proxy_wasm - set_property() ngx.* - unset an indexed variable on: request_headers
--- wasm_modules: hostcalls
--- load_nginx_modules: ngx_http_echo_module
--- config
    set $my_var 123;

    location /t {
        proxy_wasm hostcalls 'test=/t/set_property \
                              name=ngx.my_var \
                              unset=1';
        echo ok;
    }
--- error_log eval
[
    qr/\[info\] .*? old: "123"/,
    qr/\[info\] .*? new: unset/,
]
--- no_error_log
[error]
[crit]



=== TEST 4: proxy_wasm - set_property() ngx.* - unset a variable with set_handler on: request_headers

All set_property calls for Nginx variables return strings, so this
should print the $pid as ASCII numbers.

--- wasm_modules: hostcalls
--- load_nginx_modules: ngx_http_echo_module
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/set_property \
                              name=ngx.args \
                              unset=1';
        echo ok;
    }
--- request
GET /t?hello=world
--- error_log eval
[
    qr/\[info\] .*? old: "hello=world"/,
    qr/\[info\] .*? new: unset/,
]
--- no_error_log
[emerg]
[alert]



=== TEST 5: proxy_wasm - set_property() ngx.* - non-changeable variable on: request_headers
--- wasm_modules: hostcalls
--- load_nginx_modules: ngx_http_echo_module
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/set_property \
                              name=ngx.query_string \
                              set=123';
        echo ok;
    }
--- error_code: 500
--- response_body_like eval: qr/500 Internal Server Error/
--- error_log eval
[
    qr/\[error\] .*? variable "query_string" is not changeable/,
    qr/\[crit\] .*? panicked at/,
    qr/unexpected status: 10/,
]



=== TEST 6: proxy_wasm - set_property() ngx.* - unknown variable on: request_headers

In spite of the panic below, our proxy-wasm implementation does not
cause a critical failure when a property is not found.

The Rust SDK, which we're using here, panics with "unexpected status:
1", (where 1 means NotFound), but other SDKs, such as the Go SDK,
forward the NotFound status back to the caller.

--- wasm_modules: hostcalls
--- load_nginx_modules: ngx_http_echo_module
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/set_property \
                              name=ngx.nonexistent_property';
        echo ok;
    }
--- error_code: 500
--- response_body_like eval: qr/500 Internal Server Error/
--- error_log eval
[
    qr/\[crit\] .*? panicked at/,
    qr/unexpected status: 1/
]
--- no_error_log
[emerg]



=== TEST 7: proxy_wasm - set_property() ngx.* - not available on: tick (isolation: global)

on_tick runs on the root context, so it does not have access to
ngx_http_* calls.

HTTP 500 since instance recycling happens on next
request, and isolation is global (single instance for root/request)

--- wasm_modules: hostcalls
--- load_nginx_modules: ngx_http_echo_module
--- config
    set $my_var 123;

    location /t {
        proxy_wasm hostcalls 'tick_period=10 \
                              on_tick=set_property \
                              name=ngx.my_var \
                              set=456 \
                              show_old=false';
        echo_sleep 0.150;
        echo ok;
    }
--- error_code: 500
--- ignore_response_body
--- error_log eval
[
    qr/\[info\] .*? \[hostcalls\] on_tick/,
    qr/\[error\] .*? cannot set ngx properties outside of a request/,
    qr/\[crit\] .*? panicked at/,
    qr/unexpected status: 10/,
]



=== TEST 8: proxy_wasm - set_property() ngx.* - not available on: tick (isolation: stream)

on_tick runs on the root context, so it does not have access to
ngx_http_* calls.

HTTP 200 since the root and request instances are different.

--- wasm_modules: hostcalls
--- load_nginx_modules: ngx_http_echo_module
--- config
    set $my_var 123;

    location /t {
        proxy_wasm_isolation stream;

        proxy_wasm hostcalls 'tick_period=200 \
                              on_tick=set_property \
                              name=ngx.my_var \
                              set=456 \
                              show_old=false';
        echo_sleep 0.150;
        echo ok;
    }
--- ignore_response_body
--- error_log eval
[
    qr/\[info\] .*? \[hostcalls\] on_tick/,
    qr/\[error\] .*? cannot set ngx properties outside of a request/,
    qr/\[crit\] .*? panicked at/,
    qr/unexpected status: 10/,
]
