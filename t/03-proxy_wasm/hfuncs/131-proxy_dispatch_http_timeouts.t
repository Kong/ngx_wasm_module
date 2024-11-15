# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

BEGIN {
    $ENV{MOCKEAGAIN} = 'rw';
    $ENV{MOCKEAGAIN_VERBOSE} ||= 0;
    $ENV{MOCKEAGAIN_WRITE_TIMEOUT_PATTERN} = 'timeout_trigger';
    $ENV{TEST_NGINX_EVENT_TYPE} = 'poll';
}

use strict;
use lib '.';
use t::TestWasmX;

our $ExtResolver = $t::TestWasmX::extresolver;
our $ExtTimeout = $t::TestWasmX::exttimeout;

plan_tests(6);
run_tests();

__DATA__

=== TEST 1: proxy_wasm - dispatch_http_call() connection timeout
--- timeout eval: $::ExtTimeout
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config eval
qq{
    resolver         $::ExtResolver ipv6=off;
    resolver_timeout $::ExtTimeout;

    location /t {
        wasm_socket_connect_timeout 1ms;

        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=google.com \
                              on_http_call_response=echo_response_headers';
        echo ok;
    }
}
--- response_body_like
:dispatch_status: timeout
--- error_log eval
[
    qr/(\[error\]|Uncaught RuntimeError|\s+).*?dispatch failed: tcp socket - timed out connecting to \".*?\"/,
    qr/\*\d+ .*? on_http_call_response \(id: \d+, status: , headers: 0, body_bytes: 0/,
    qr/dispatch_status: timeout/
]
--- no_error_log
[crit]



=== TEST 2: proxy_wasm - dispatch_http_call() resolver timeout
Using a non-local resolver
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config eval
qq{
    # a non-local resolver
    resolver          8.8.8.8;
    resolver_timeout  1ms;

    location /t {
        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=httpbin.org \
                              path=/status/200';
        echo ok;
    }
}
--- response_body
ok
--- error_log eval
[
    qr/(\[error\]|Uncaught RuntimeError|\s+).*?dispatch failed: tcp socket - resolver error: Operation timed out/,
    qr/on_http_call_response \(id: \d+, status: , headers: 0, body_bytes: 0/,
    "dispatch_status: resolver failure"
]
--- no_error_log
[crit]



=== TEST 3: proxy_wasm - dispatch_http_call() read timeout
macOS: mockeagain NYI
--- skip_eval: 4: $::osname =~ m/darwin/
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        wasm_socket_read_timeout 1ms;

        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=127.0.0.1:$TEST_NGINX_SERVER_PORT \
                              path=/dispatched';
        echo ok;
    }

    location /dispatched {
        echo -n timeout_trigger;
    }
--- response_body
ok
--- error_log eval
[
    qr/(\[error\]|Uncaught RuntimeError|\s+).*?dispatch failed: tcp socket - timed out reading from \"127\.0\.0\.1:\d+\"/,
    qr/on_http_call_response \(id: \d+, status: (\d+)?, headers: \d+, body_bytes: \d+/,
    "dispatch_status: timeout"
]
--- no_error_log
[crit]



=== TEST 4: proxy_wasm - dispatch_http_call() write timeout
macOS: mockeagain NYI
--- skip_eval: 4: $::osname =~ m/darwin/
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        wasm_socket_send_timeout 1ms;

        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=127.0.0.1:$TEST_NGINX_SERVER_PORT \
                              method=POST \
                              path=/post \
                              body=timeout_trigger';
        echo ok;
    }

    location /post {
        echo -n $request_body;
    }
--- response_body
ok
--- error_log eval
[
    qr/(\[error\]|Uncaught RuntimeError|\s+).*?dispatch failed: tcp socket - timed out writing to \".*?\"/,
    qr/on_http_call_response \(id: \d+, status: , headers: 0, body_bytes: 0/,
    "dispatch_status: timeout"
]
--- no_error_log
[crit]



=== TEST 5: proxy_wasm - dispatch_http_call() on_request_headers EAGAIN
dispatch_http_call() EAGAIN
local_response() EAGAIN
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /dispatched {
        return 200 "Hello world";
    }

    location /t {
        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=127.0.0.1:$TEST_NGINX_SERVER_PORT \
                              path=/dispatched \
                              on_http_call_response=echo_response_body';
        echo failed;
    }
--- response_body
Hello world
--- no_error_log
[error]
[crit]
[alert]
[emerg]



=== TEST 6: proxy_wasm - dispatch_http_call() on_request_body EAGAIN
dispatch_http_call() EAGAIN
local_response() EAGAIN
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /dispatched {
        return 200 "Hello world";
    }

    location /t {
        proxy_wasm hostcalls 'on=request_body \
                              test=/t/dispatch_http_call \
                              host=127.0.0.1:$TEST_NGINX_SERVER_PORT \
                              path=/dispatched \
                              on_http_call_response=echo_response_body';
        echo failed;
    }
--- request
GET /t

Hello world
--- response_body
Hello world
--- no_error_log
[error]
[crit]
[alert]
[emerg]
