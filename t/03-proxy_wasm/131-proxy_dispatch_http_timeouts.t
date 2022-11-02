# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

BEGIN {
    $ENV{MOCKEAGAIN} = 'rw';
    $ENV{MOCKEAGAIN_VERBOSE} ||= 0;
    $ENV{MOCKEAGAIN_WRITE_TIMEOUT_PATTERN} = 'timeout_trigger';
    $ENV{TEST_NGINX_EVENT_TYPE} = 'poll';
}

use strict;
use lib '.';
use t::TestWasm;

our $ExtResolver = $t::TestWasm::extresolver;
our $ExtTimeout = $t::TestWasm::exttimeout;

skip_valgrind('wasmtime');

plan tests => repeat_each() * (blocks() * 4);

run_tests();

__DATA__

=== TEST 1: proxy_wasm - dispatch_http_call() connection timeout
--- timeout eval: $::ExtTimeout
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config eval
qq{
    resolver         $::ExtResolver;
    resolver_timeout $::ExtTimeout;

    location /t {
        wasm_socket_connect_timeout 1ms;

        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=httpbin.org \
                              path=/status/200';
        echo fail;
    }
}
--- error_code: 500
--- response_body_like: 500 Internal Server Error
--- error_log eval
qr/\[error\] .*? \[wasm\] dispatch failed \(tcp socket - timed out connecting to \".*?"\)/
--- no_error_log
[crit]



=== TEST 2: proxy_wasm - dispatch_http_call() resolver timeout
--- timeout_no_valgrind: 1
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config eval
qq{
    resolver          $::ExtResolver;
    resolver_timeout  1ms;

    location /t {
        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=httpbin.org \
                              path=/status/200';
        echo fail;
    }
}
--- error_code: 500
--- response_body_like: 500 Internal Server Error
--- error_log eval
qr/\[error\] .*? \[wasm\] dispatch failed \(tcp socket - resolver error: Operation timed out\)/
--- no_error_log
[crit]



=== TEST 3: proxy_wasm - dispatch_http_call() read timeout
--- timeout_no_valgrind: 1
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        wasm_socket_read_timeout 1ms;

        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=127.0.0.1:$TEST_NGINX_SERVER_PORT \
                              path=/dispatched';
        echo fail;
    }

    location /dispatched {
        echo -n timeout_trigger;
    }
--- error_code: 500
--- response_body_like: 500 Internal Server Error
--- error_log eval
qr/\[error\] .*? \[wasm\] dispatch failed \(tcp socket - timed out reading from \"127\.0\.0\.1:\d+\"\)/
--- no_error_log
[crit]



=== TEST 4: proxy_wasm - dispatch_http_call() write timeout
--- timeout_no_valgrind: 1
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
        echo fail;
    }

    location /post {
        echo -n $request_body;
    }
--- error_code: 500
--- response_body_like: 500 Internal Server Error
--- error_log eval
qr/\[error\] .*? \[wasm\] dispatch failed \(tcp socket - timed out writing to \".*?"\)/
--- no_error_log
[crit]
