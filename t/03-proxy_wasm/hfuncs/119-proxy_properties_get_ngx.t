# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasmX;

our $nginx_variables = join(',', qw(
    ngx.pid
    ngx.hostname
));

no_shuffle();

plan_tests(5);
run_tests();

__DATA__

=== TEST 1: proxy_wasm - get_property() ngx.* - nginx variables on: request_headers

All get_property calls for Nginx variables return strings, so this should print
the $pid as ASCII numbers.

--- valgrind
--- wasm_modules: hostcalls
--- load_nginx_modules: ngx_http_echo_module
--- config eval
qq{
    location /t {
        proxy_wasm hostcalls 'on=request_headers \
                              test=/t/log/properties \
                              name=$::nginx_variables';
        echo ok;
    }
}
--- response_body
ok
--- grep_error_log eval: qr/.*?at RequestHeaders/
--- grep_error_log_out eval
qr/.*? ngx.pid: [0-9]+.*
.*? ngx.hostname: [a-z0-9]+/
--- no_error_log
[error]
[crit]



=== TEST 2: proxy_wasm - get_property() ngx.* - $hostname variable on: request_headers, request_body, response_headers, response_body, log
--- wasm_modules: hostcalls
--- load_nginx_modules: ngx_http_echo_module
--- config
    location /t {
        proxy_wasm hostcalls 'on=request_headers \
                              test=/t/log/property \
                              name=ngx.hostname';
        proxy_wasm hostcalls 'on=request_body \
                              test=/t/log/property \
                              name=ngx.hostname';
        echo ok;
        proxy_wasm hostcalls 'on=response_headers \
                              test=/t/log/property \
                              name=ngx.hostname';
        proxy_wasm hostcalls 'on=response_body \
                              test=/t/log/property \
                              name=ngx.hostname';
        proxy_wasm hostcalls 'on=log \
                              test=/t/log/property \
                              name=ngx.hostname';
    }
--- request
POST /t
hello
--- response_body
ok
--- grep_error_log eval: qr/\[info\].*?(RequestHeaders|RequestBody|ResponseHeaders|ResponseBody|Log|ngx\.hostname).*/
--- grep_error_log_out eval
qr/.*?testing in "RequestHeaders".*
.*? ngx\.hostname: [a-z0-9]+.*
.*?testing in "RequestBody".*
.*? ngx\.hostname: [a-z0-9]+.*
.*?testing in "ResponseHeaders".*
.*? ngx\.hostname: [a-z0-9]+.*
.*?testing in "ResponseBody".*
.*? ngx\.hostname: [a-z0-9]+.*
.*?testing in "ResponseBody".*
.*? ngx\.hostname: [a-z0-9]+.*
.*?testing in "Log".*
.*? ngx\.hostname: [a-z0-9]+/
--- no_error_log
[error]
[crit]



=== TEST 3: proxy_wasm - get_property() ngx.* - unknown variable on: request_headers
--- wasm_modules: hostcalls
--- load_nginx_modules: ngx_http_echo_module
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/log/property \
                              name=ngx.nonexistent_property';
        echo ok;
    }
--- response_body
ok
--- error_log eval
qr/\[info\] .*? property not found: ngx.nonexistent_property/
--- no_error_log
[error]
[crit]



=== TEST 4: proxy_wasm - get_property() ngx.* - unknown property with name shorter than 'ngx.' prefix
--- wasm_modules: hostcalls
--- load_nginx_modules: ngx_http_echo_module
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/log/property \
                              name=n';
        echo ok;
    }
--- response_body
ok
--- error_log eval
qr/\[info\] .*? property not found: n,/
--- no_error_log
[error]
[crit]



=== TEST 5: proxy_wasm - get_property() ngx.* - not available on: tick (isolation: global)
on_tick runs on the root context, so it does not have access to
ngx_http_* calls.
--- wasm_modules: hostcalls
--- load_nginx_modules: ngx_http_echo_module
--- config
    location /t {
        proxy_wasm hostcalls 'tick_period=200 \
                              on_tick=log_property \
                              name=ngx.hostname';
        echo_sleep 0.5;
        echo ok;
    }
--- ignore_response_body
--- error_log eval
[
    qr/\[info\] .*? \[hostcalls\] on_tick/,
    qr/\[error\] .*? cannot get scoped properties outside of a request/,
    qr/\[crit\] .*? panicked at/,
    qr/value: InternalFailure/,
]
