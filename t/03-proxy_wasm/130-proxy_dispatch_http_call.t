# vim:set ft= ts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

skip_valgrind();

plan tests => repeat_each() * (blocks() * 3);

run_tests();

__DATA__

=== TEST 1: proxy_wasm - dispatch_http_call() sanity
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=127.0.0.1:$TEST_NGINX_SERVER_PORT \
                              path=/dispatched';
    }

    location /dispatched {
        echo -n $echo_client_request_headers;
    }
--- response_body eval
"GET /dispatched HTTP/1.1\r
Host: 127.0.0.1:1984\r
Connection: close\r
Content-Length: 0
"
--- no_error_log
[error]



=== TEST 2: proxy_wasm - dispatch_http_call() missing host
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/dispatch_http_call';
    }
--- error_code: 500
--- response_body_like: 500 Internal Server Error
--- error_log eval
qr/\[error\] .*? dispatch failed \(no host\)/



=== TEST 3: proxy_wasm - dispatch_http_call() invalid host
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=:80';
    }
--- error_code: 500
--- response_body_like: 500 Internal Server Error
--- error_log eval
qr/\[error\] .*? dispatch failed \(no host\)/



=== TEST 4: proxy_wasm - dispatch_http_call() invalid port
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=127.0.0.1:66000';
    }
--- error_code: 500
--- response_body_like: 500 Internal Server Error
--- error_log eval
qr/\[error\] .*? dispatch failed \(invalid port\)/



=== TEST 5: proxy_wasm - dispatch_http_call() missing :method
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=127.0.0.1 \
                              no_method=true';
    }
--- error_code: 500
--- response_body_like: 500 Internal Server Error
--- error_log eval
qr/\[error\] .*? dispatch failed \(no :method\)/



=== TEST 6: proxy_wasm - dispatch_http_call() missing :path
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=127.0.0.1 \
                              no_path=true';
    }
--- error_code: 500
--- response_body_like: 500 Internal Server Error
--- error_log eval
qr/\[error\] .*? dispatch failed \(no :path\)/



=== TEST 7: proxy_wasm - dispatch_http_call() missing :authority
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=127.0.0.1 \
                              no_authority=true';
    }
--- error_code: 500
--- response_body_like: 500 Internal Server Error
--- error_log eval
qr/\[error\] .*? dispatch failed \(no :authority\)/



=== TEST 8: proxy_wasm - dispatch_http_call() connection refused
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=127.0.0.10';
        echo ok;
    }
--- response_body
ok
--- error_log eval
qr/\[error\] .*? dispatch failed \(Connection refused\)/



=== TEST 9: proxy_wasm - dispatch_http_call() no resolver
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=localhost';
        echo ok;
    }
--- response_body
ok
--- error_log eval
qr/\[error\] .*? dispatch failed \(no resolver defined to resolve "localhost"\)/
