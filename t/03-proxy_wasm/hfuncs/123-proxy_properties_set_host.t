# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

plan_tests(6);
run_tests();

__DATA__

=== TEST 1: proxy_wasm - set_property() wasmx - host property on: request_headers, request_body, response_headers, response_body, log
--- valgrind
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
[emerg]
[alert]
[stub]



=== TEST 2: proxy_wasm - set_property() wasmx - unset a host property on: request_headers
--- valgrind
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
[emerg]



=== TEST 3: proxy_wasm - set_property() wasmx - set a host property to an empty string
--- valgrind
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
[emerg]



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
[emerg]
[alert]
[stub]



=== TEST 5: proxy_wasm - set_property() wasmx - not available on: tick (isolation: global)
on_tick runs on the root context, so it does not have access to
ngx_http_* calls.
--- wasm_modules: hostcalls
--- load_nginx_modules: ngx_http_echo_module
--- config
    set $my_var 123;

    location /t {
        proxy_wasm hostcalls 'tick_period=100 \
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
    qr/\[crit\] .*? panicked at/,
    qr/unexpected status: 10/,
]
--- no_error_log
[emerg]



=== TEST 6: proxy_wasm - set_property() wasmx - not available on: tick (isolation: stream)
on_tick runs on the root context, so it does not have access to
ngx_http_* calls.
--- wasm_modules: hostcalls
--- load_nginx_modules: ngx_http_echo_module
--- config
    set $my_var 123;

    location /t {
        proxy_wasm_isolation stream;

        proxy_wasm hostcalls 'tick_period=100 \
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
    qr/\[crit\] .*? panicked at/,
    qr/unexpected status: 10/,
]
--- no_error_log
[emerg]
