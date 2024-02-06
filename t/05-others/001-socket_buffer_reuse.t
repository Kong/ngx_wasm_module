# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

plan_tests(4);
run_tests();

__DATA__

=== TEST 1: buffer reuse - wasm_socket_buffer_reuse on
--- valgrind
--- skip_no_debug
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /dispatched {
        echo "A";
    }

    location /dispatched1 {
        echo "B";
    }

    location /dispatched2 {
        echo "C";
    }

    location /t {
        proxy_wasm hostcalls 'on=request_headers \
                              test=/t/dispatch_http_call \
                              host=127.0.0.1:$TEST_NGINX_SERVER_PORT \
                              path=/dispatched \
                              on_http_call_response=call_again \
                              n_sync_calls=2';
        echo fail;
    }
--- response_body
called 3 times
--- grep_error_log eval: qr/wasm reuse free buf.*/
--- grep_error_log_out eval
qr/wasm reuse free buf memory \d+ >= 2,.*
wasm reuse free buf memory \d+ >= 2,.*
wasm reuse free buf memory \d+ >= 2,.*/
--- no_error_log
[error]



=== TEST 2: buffer reuse - wasm_socket_buffer_reuse off
ngx_wasm_socket buffers not reused (buffers of size 2)
--- skip_no_debug
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    wasm_socket_buffer_reuse off;

    location /dispatched {
        echo "A";
    }

    location /dispatched1 {
        echo "B";
    }

    location /dispatched2 {
        echo "C";
    }

    location /t {
        proxy_wasm hostcalls 'on=request_headers \
                              test=/t/dispatch_http_call \
                              host=127.0.0.1:$TEST_NGINX_SERVER_PORT \
                              path=/dispatched \
                              on_http_call_response=call_again \
                              n_sync_calls=2';
        echo fail;
    }
--- response_body
called 3 times
--- grep_error_log eval: qr/wasm reuse free buf.*/
--- grep_error_log_out eval
qr/wasm reuse free buf memory \d+ >= \d+/
--- no_error_log eval
[
    qr/wasm reuse free buf memory \d+ >= 2/,
    [error]
]



=== TEST 3: buffer reuse - wasm_socket_buffer_reuse on, realloc
--- valgrind
--- skip_no_debug
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    wasm_socket_buffer_reuse on;

    location /dispatched {
        echo "A";
    }

    location /dispatched1 {
        echo "B";
    }

    location /dispatched2 {
        echo_duplicate 1000 "C";
    }

    location /dispatched3 {
        echo "D";
    }

    location /t {
        proxy_wasm hostcalls 'on=request_headers \
                              test=/t/dispatch_http_call \
                              host=127.0.0.1:$TEST_NGINX_SERVER_PORT \
                              path=/dispatched \
                              on_http_call_response=call_again \
                              n_sync_calls=3';
        echo fail;
    }
--- response_body
called 4 times
--- grep_error_log eval: qr/wasm reuse free buf.*/
--- grep_error_log_out eval
qr/wasm reuse free buf memory \d+ >= 2,.*
wasm reuse free buf memory \d+ >= 2,.*
wasm reuse free buf chain, but reallocate memory because 1000 >= \d+,.*
wasm reuse free buf memory \d+ >= 2,/
--- no_error_log
[error]
