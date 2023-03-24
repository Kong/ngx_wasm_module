# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

skip_valgrind();

plan tests => repeat_each() * (blocks() * 5);

run_tests();

__DATA__

=== TEST 1: proxy_wasm - set_property() can set Nginx indexed variable
--- wasm_modules: hostcalls
--- load_nginx_modules: ngx_http_echo_module
--- config
    set $my_var 123;
    location /t {
        proxy_wasm hostcalls 'test=/t/set_property \
                              name=ngx.my_var \
                              set=456';
        echo ok;
    }
--- error_log eval
[
    qr/\[info\] .*? old: "123"/,
    qr/\[info\] .*? new: "456"/,
]
--- no_error_log
[error]
[crit]



=== TEST 2: proxy_wasm - set_property() can set Nginx variable with set_handler
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



=== TEST 3: proxy_wasm - set_property() can set proxy-wasm variable with set_handler
--- wasm_modules: hostcalls
--- load_nginx_modules: ngx_http_echo_module
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/set_property \
                              name=request.query \
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



=== TEST 4: proxy_wasm - set_property() can unset Nginx indexed variable
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



=== TEST 5: proxy_wasm - set_property() can unset Nginx variable with set_handler
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
[error]
[crit]



=== TEST 6: proxy_wasm - set_property() fails when a variable that is not changeable
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
--- response_body eval
qr/500 Internal Server Error/
--- error_log eval
[
    qr/\[error\] .*? variable "query_string" is not changeable/,
    qr/\[crit\] .*? panicked at 'unexpected status: 10'/,
]
--- no_error_log
[alert]



=== TEST 7: proxy_wasm - set_property() fails when an ngx.* property is not found
In spite of the panic below, our proxy-wasm implementation does not cause a
critical failure when a property is not found.

The Rust SDK, which we're using here, panics with "unexpected status: 1",
(where 1 means NotFound), but other SDKs, such as the Go SDK, forward the
NotFound status back to the caller.

--- wasm_modules: hostcalls
--- load_nginx_modules: ngx_http_echo_module
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/set_property \
                              name=ngx.nonexistent_property';
        echo ok;
    }
--- error_code: 500
--- response_body eval
qr/500 Internal Server Error/
--- error_log eval
[
    qr/\[crit\] .*? panicked at 'unexpected status: 1'/,
]
--- no_error_log
[emerg]
[alert]



=== TEST 8: proxy_wasm - set_property() fails if a generic property is not found
In spite of the panic below, our proxy-wasm implementation does not cause a
critical failure when a property is not found.

The Rust SDK, which we're using here, panics with "unexpected status: 1",
(where 1 means NotFound), but other SDKs, such as the Go SDK, forward the
NotFound status back to the caller.

--- wasm_modules: hostcalls
--- load_nginx_modules: ngx_http_echo_module
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
]
--- no_error_log
[emerg]
[alert]



=== TEST 9: proxy_wasm - set_property() works on_request_headers
--- wasm_modules: hostcalls
--- load_nginx_modules: ngx_http_echo_module
--- config
    set $my_var 123;
    location /t {
        proxy_wasm hostcalls 'on=request_headers \
                              test=/t/set_property \
                              name=ngx.my_var \
                              set=456';
        echo ok;
    }
--- error_log eval
[
    qr/\[info\] .*? old: "123"/,
    qr/\[info\] .*? new: "456"/,
]
--- no_error_log
[error]
[crit]



=== TEST 10: proxy_wasm - set_property() works on_request_body
--- wasm_modules: hostcalls
--- load_nginx_modules: ngx_http_echo_module
--- config
    set $my_var 123;
    location /t {
        proxy_wasm hostcalls 'on=request_body \
                              test=/t/set_property \
                              name=ngx.my_var \
                              set=456';
        echo ok;
    }
--- request
POST /t/echo/body
hello world
--- error_log eval
[
    qr/\[info\] .*? old: "123"/,
    qr/\[info\] .*? new: "456"/,
]
--- no_error_log
[error]
[crit]



=== TEST 11: proxy_wasm - set_property() works on_response_headers
--- wasm_modules: hostcalls
--- load_nginx_modules: ngx_http_echo_module
--- config
    set $my_var 123;
    location /t {
        proxy_wasm hostcalls 'on=response_headers \
                              test=/t/set_property \
                              name=ngx.my_var \
                              set=456';
        echo ok;
    }
--- error_log eval
[
    qr/\[info\] .*? old: "123"/,
    qr/\[info\] .*? new: "456"/,
]
--- no_error_log
[error]
[crit]



=== TEST 12: proxy_wasm - set_property() works on_response_body
--- wasm_modules: hostcalls
--- load_nginx_modules: ngx_http_echo_module
--- config
    set $my_var 123;
    location /t {
        proxy_wasm hostcalls 'on=response_body \
                              test=/t/set_property \
                              name=ngx.my_var \
                              set=456';
        echo ok;
    }
--- error_log eval
[
    qr/\[info\] .*? old: "123"/,
    qr/\[info\] .*? new: "456"/,
]
--- no_error_log
[error]
[crit]



=== TEST 13: proxy_wasm - set_property() works on_log
--- wasm_modules: hostcalls
--- load_nginx_modules: ngx_http_echo_module
--- config
    set $my_var 123;
    location /t {
        proxy_wasm hostcalls 'on=log \
                              test=/t/set_property \
                              name=ngx.my_var \
                              set=456';
        echo ok;
    }
--- error_log eval
[
    qr/\[info\] .*? old: "123"/,
    qr/\[info\] .*? new: "456"/,
]
--- no_error_log
[error]
[crit]



=== TEST 14: proxy_wasm - set_property() for ngx.* does not work on_tick (isolation: global)
on_tick runs on the root context, so it does not have access to ngx_http_* calls.
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
    qr/\[crit\] .*? panicked at 'unexpected status: 10'/,
    qr/(.*?(Uncaught RuntimeError: )?unreachable|wasm trap: wasm `unreachable` instruction executed).*/
]



=== TEST 15: proxy_wasm - set_property() for ngx.* does not work on_tick (isolation: stream)
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
    set $my_var 123;

    location /t {
        proxy_wasm_isolation stream;

        proxy_wasm hostcalls 'tick_period=10 \
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
    qr/\[crit\] .*? panicked at 'unexpected status: 10'/,
    qr/(.*?(Uncaught RuntimeError: )?unreachable|wasm trap: wasm `unreachable` instruction executed).*/

]
