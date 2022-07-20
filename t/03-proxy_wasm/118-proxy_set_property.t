# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

skip_valgrind();

plan tests => repeat_each() * (blocks() * 4 + 2);

run_tests();

__DATA__

=== TEST 1: proxy_wasm - set_property() sets Nginx $hostname variable
--- wasm_modules: hostcalls
--- load_nginx_modules: ngx_http_echo_module
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/log/set_property name=ngx.hostname';
        echo ok;
    }
--- ignore_response_body
--- error_log eval
qr/\[info\] .*? property can not change: ngx.hostname/
--- no_error_log
[error]
[crit]



=== TEST 2: proxy_wasm - set_property() sets Nginx $test variable
--- wasm_modules: hostcalls
--- load_nginx_modules: ngx_http_echo_module
--- config
    set $test 1;
    location /t {
        proxy_wasm hostcalls 'test=/t/log/set_property name=ngx.test';
        echo ok;
    }
--- ignore_response_body
--- error_log eval
qr/\[info\] .*? property change sucess: ngx.test/
--- no_error_log
[error]
[crit]



=== TEST 3: proxy_wasm - set_property() sets Nginx $no_exist_value variable
--- wasm_modules: hostcalls
--- load_nginx_modules: ngx_http_echo_module
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/log/set_property name=ngx.no_exist_value';
        echo ok;
    }
--- ignore_response_body
--- error_log eval
[
    qr/\[info\] .*? property not found: ngx.no_exist_value/,
    qr/\[info\] .*? property can not change: ngx.no_exist_value/,
]
--- no_error_log
[error]
[crit]



=== TEST 4: proxy_wasm - set_property() reports if a generic property is not found
--- wasm_modules: hostcalls
--- load_nginx_modules: ngx_http_echo_module
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/log/set_property name=nonexistent_property';
        echo ok;
    }
--- ignore_response_body
--- error_log eval
    qr/\[info\] .*? property not found: nonexistent_property/,
    qr/\[info\] .*? property can not change: ngx.nonexistent_property/,
--- no_error_log
[error]
[crit]



=== TEST 5: proxy_wasm - set_property() works on_request_headers
--- wasm_modules: hostcalls
--- load_nginx_modules: ngx_http_echo_module
--- config
    set $test 1;
    location /t {
        proxy_wasm hostcalls 'on=request_headers test=/t/log/set_property name=ngx.test';
        echo ok;
    }
--- ignore_response_body
--- error_log eval
[
    qr/\[info\] .*? \[hostcalls\] on_request_headers/,
    qr/\[info\] .*? property change sucess: ngx.test/,
]
--- no_error_log
[error]



=== TEST 6: proxy_wasm - set_property() works on_request_body
--- wasm_modules: hostcalls
--- load_nginx_modules: ngx_http_echo_module
--- config
    set $test 1;
    location /t {
        proxy_wasm hostcalls 'on=request_body test=/t/log/set_property name=ngx.test';
        echo ok;
    }
--- request
POST /t/echo/body
hello world
--- ignore_response_body
--- error_log eval
[
    qr/\[info\] .*? \[hostcalls\] on_request_body/,
    qr/\[info\] .*? property change sucess: ngx.test/,
]
--- no_error_log
[error]



=== TEST 7: proxy_wasm - set_property() works on_response_headers
--- wasm_modules: hostcalls
--- load_nginx_modules: ngx_http_echo_module
--- config
    set $test 1;
    location /t {
        proxy_wasm hostcalls 'on=response_headers test=/t/log/set_property name=ngx.test';
        echo ok;
    }
--- ignore_response_body
--- error_log eval
[
    qr/\[info\] .*? \[hostcalls\] on_response_headers/,
    qr/\[info\] .*? property change sucess: ngx.test/,
]
--- no_error_log
[error]



=== TEST 8: proxy_wasm - set_property() works on_response_body
--- wasm_modules: hostcalls
--- load_nginx_modules: ngx_http_echo_module
--- config
    set $test 1;
    location /t {
        proxy_wasm hostcalls 'on=response_body test=/t/log/set_property name=ngx.test';
        echo ok;
    }
--- ignore_response_body
--- error_log eval
[
    qr/\[info\] .*? \[hostcalls\] on_response_body/,
    qr/\[info\] .*? property change sucess: ngx.test/,
]
--- no_error_log
[error]



=== TEST 9: proxy_wasm - set_property() works on_log
--- wasm_modules: hostcalls
--- load_nginx_modules: ngx_http_echo_module
--- config
    set $test 1;
    location /t {
        proxy_wasm hostcalls 'on=log test=/t/log/set_property name=ngx.test';
        echo ok;
    }
--- ignore_response_body
--- error_log eval
[
    qr/\[info\] .*? \[hostcalls\] on_log/,
    qr/\[info\] .*? property change sucess: ngx.test/,
]
--- no_error_log
[error]



=== TEST 10: proxy_wasm - set_property() for ngx.* does not work on_tick
on_tick runs on the root context, so it does not have access to ngx_http_* calls.
--- wasm_modules: hostcalls
--- load_nginx_modules: ngx_http_echo_module
--- config
    set $test 1;
    location /t {
        proxy_wasm hostcalls 'tick_period=10 on_tick=log_set_property name=ngx.test';
        echo_sleep 0.150;
        echo ok;
    }
--- ignore_response_body
--- error_log eval
[
    qr/\[info\] .*? \[hostcalls\] on_tick/,
    qr/\[info\] .*? property not found: ngx.test/,
    qr/\[info\] .*? property can not change: ngx.test/,
]
--- no_error_log
[error]
