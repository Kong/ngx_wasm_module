# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

skip_valgrind('wasmtime');

plan tests => repeat_each() * (blocks() * 6);

run_tests();

__DATA__

=== TEST 1: proxy_wasm - dispatch_http_call() on dispatch response (1 subsequent call)
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /dispatched {
        echo "Hello world";
    }

    location /t {
        proxy_wasm hostcalls 'on=request_headers \
                              test=/t/dispatch_http_call \
                              host=127.0.0.1:$TEST_NGINX_SERVER_PORT \
                              path=/dispatched \
                              on_http_call_response=call_again';
        echo fail;
    }
--- response_headers_like
pwm-call-1: Hello world
--- response_body
called 2 times
--- no_error_log
[error]
[crit]
[emerg]



=== TEST 2: proxy_wasm - dispatch_http_call() on dispatch response (2 subsequent calls)
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /dispatched {
        echo "dispatch 1";
    }

    location /dispatched1 {
        echo "dispatch 2";
    }

    location /dispatched2 {
        echo "dispatch 3";
    }

    location /t {
        proxy_wasm hostcalls 'on=request_headers \
                              test=/t/dispatch_http_call \
                              host=127.0.0.1:$TEST_NGINX_SERVER_PORT \
                              path=/dispatched \
                              on_http_call_response=call_again \
                              ncalls=2';
        echo fail;
    }
--- response_headers_like
pwm-call-1: dispatch 1
pwm-call-2: dispatch 2
pwm-call-3: dispatch 3
--- response_body
called 3 times
--- no_error_log
[error]



=== TEST 3: proxy_wasm - dispatch_http_call() on_tick (1 call)
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /dispatched {
        echo "Hello world";
    }

    location /t {
        proxy_wasm hostcalls 'tick_period=5 \
                              on_tick=dispatch \
                              host=127.0.0.1:$TEST_NGINX_SERVER_PORT \
                              path=/dispatched';
        echo ok;
    }
--- response_body
ok
--- error_log eval
qr/on_root_http_call_response \(id: 0, status: 200, headers: 5, body_bytes: 12, trailers: 0\)/
--- no_error_log
[error]
[crit]
[emerg]



=== TEST 4: proxy_wasm - dispatch_http_call() on_tick (10 calls)
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /dispatched {
        echo "Hello world";
    }

    location /t {
        proxy_wasm hostcalls 'tick_period=5 \
                              on_tick=dispatch \
                              host=127.0.0.1:$TEST_NGINX_SERVER_PORT \
                              path=/dispatched \
                              ncalls=10';
        echo ok;
    }
--- response_body
ok
--- grep_error_log eval: qr/on_root_http_call_response \(.*?\)/
--- grep_error_log_out eval
qr/\Aon_root_http_call_response \(id: 0, status: 200, headers: 5, body_bytes: 12, trailers: 0\)
on_root_http_call_response \(id: 1, status: 200, headers: 5, body_bytes: 12, trailers: 0\)
on_root_http_call_response \(id: 2, status: 200, headers: 5, body_bytes: 12, trailers: 0\)
on_root_http_call_response \(id: 3, status: 200, headers: 5, body_bytes: 12, trailers: 0\)
on_root_http_call_response \(id: 4, status: 200, headers: 5, body_bytes: 12, trailers: 0\)
on_root_http_call_response \(id: 5, status: 200, headers: 5, body_bytes: 12, trailers: 0\)
on_root_http_call_response \(id: 6, status: 200, headers: 5, body_bytes: 12, trailers: 0\)
on_root_http_call_response \(id: 7, status: 200, headers: 5, body_bytes: 12, trailers: 0\)
on_root_http_call_response \(id: 8, status: 200, headers: 5, body_bytes: 12, trailers: 0\)
on_root_http_call_response \(id: 9, status: 200, headers: 5, body_bytes: 12, trailers: 0\)\Z/
--- no_error_log
[error]
[crit]
[emerg]



=== TEST 5: proxy_wasm - dispatch_http_call() on_request_headers, filter chain execution sanity
should execute all filters request and response steps
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /dispatched {
        return 200 "Hello back";
    }

    location /t {
        proxy_wasm hostcalls 'on=request_headers \
                              test=/t/dispatch_http_call \
                              host=127.0.0.1:$TEST_NGINX_SERVER_PORT \
                              path=/dispatched';
        proxy_wasm hostcalls;
        echo ok;
    }
--- request
GET /t

Hello world
--- response_body
ok
--- grep_error_log eval: qr/\[info\] .*? on_(http_call_response|request|response|log).*/
--- grep_error_log_out eval
qr/\A.*? on_request_headers.*
.*? on_http_call_response \(id: 0, status: 200, headers: 5, body_bytes: 10, trailers: 0, op: \).*
.*? on_request_headers.*
.*? on_request_body.*
.*? on_request_body.*
.*? on_response_headers.*
.*? on_response_headers.*
.*? on_response_body.*
.*? on_response_body.*
.*? on_response_body.*
.*? on_response_body.*
.*? on_log.*
.*? on_log/
--- no_error_log
[error]
[crit]
[emerg]



=== TEST 6: proxy_wasm - dispatch_http_call() on_request_body, filter chain execution sanity
should execute all filters request and response steps
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /dispatched {
        return 200 "Hello back";
    }

    location /t {
        proxy_wasm hostcalls 'on=request_body \
                              test=/t/dispatch_http_call \
                              host=127.0.0.1:$TEST_NGINX_SERVER_PORT \
                              path=/dispatched';
        proxy_wasm hostcalls;
        echo ok;
    }
--- request
GET /t

Hello world
--- response_body
ok
--- grep_error_log eval: qr/\[info\] .*? on_(http_call_response|request|response|log).*/
--- grep_error_log_out eval
qr/\A.*? on_request_headers.*
.*? on_request_headers.*
.*? on_request_body.*
.*? on_http_call_response \(id: 0, status: 200, headers: 5, body_bytes: 10, trailers: 0, op: \).*
.*? on_request_body.*
.*? on_response_headers.*
.*? on_response_headers.*
.*? on_response_body.*
.*? on_response_body.*
.*? on_response_body.*
.*? on_response_body.*
.*? on_log.*
.*? on_log/
--- no_error_log
[error]
[crit]
[emerg]
