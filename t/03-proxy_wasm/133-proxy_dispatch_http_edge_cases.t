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
                              call_again=2';
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
