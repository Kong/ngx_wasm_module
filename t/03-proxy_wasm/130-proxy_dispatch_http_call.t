# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

skip_valgrind();

plan tests => repeat_each() * (blocks() * 4);

run_tests();

__DATA__

=== TEST 1: proxy_wasm - dispatch_http_call() missing host
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/dispatch_http_call';
    }
--- error_code: 500
--- response_body_like: 500 Internal Server Error
--- error_log eval
qr/\[error\] .*? dispatch failed: no host/
--- no_error_log
[crit]



=== TEST 2: proxy_wasm - dispatch_http_call() invalid host
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=:80';
    }
--- error_code: 500
--- response_body_like: 500 Internal Server Error
--- error_log eval
qr/\[error\] .*? dispatch failed: no host/
--- no_error_log
[crit]



=== TEST 3: proxy_wasm - dispatch_http_call() invalid port
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=127.0.0.1:66000';
    }
--- error_code: 500
--- response_body_like: 500 Internal Server Error
--- error_log eval
qr/\[error\] .*? dispatch failed: invalid port/
--- no_error_log
[crit]



=== TEST 4: proxy_wasm - dispatch_http_call() missing :method
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
qr/\[error\] .*? dispatch failed: no :method/
--- no_error_log
[crit]



=== TEST 5: proxy_wasm - dispatch_http_call() missing :path
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
qr/\[error\] .*? dispatch failed: no :path/
--- no_error_log
[crit]



=== TEST 6: proxy_wasm - dispatch_http_call() missing :authority
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
qr/\[error\] .*? dispatch failed: no :authority/
--- no_error_log
[crit]



=== TEST 7: proxy_wasm - dispatch_http_call() connection refused
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=127.0.0.10';
        echo ok;
    }
--- error_code: 500
--- response_body_like: 500 Internal Server Error
--- error_log eval
qr/\[error\] .*? dispatch failed \(Connection refused\)/
--- no_error_log
[crit]



=== TEST 8: proxy_wasm - dispatch_http_call() no resolver
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=localhost';
        echo ok;
    }
--- error_code: 500
--- response_body_like: 500 Internal Server Error
--- error_log eval
qr/\[error\] .*? dispatch failed \(no resolver defined to resolve "localhost"\)/
--- no_error_log
[crit]



=== TEST 9: proxy_wasm - dispatch_http_call() resolver + hostname
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    resolver 1.1.1.1;

    location /t {
        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=httpbin.org \
                              headers=X-Thing:foo|X-Thing:bar|Hello:world
                              path=/headers';
        echo fail;
    }
--- response_body_like
\s*"Hello": "world",\s*
.*?
\s*"X-Thing": "foo,bar"\s*
--- no_error_log
[error]
[crit]



=== TEST 10: proxy_wasm - dispatch_http_call() no response body
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=127.0.0.1:$TEST_NGINX_SERVER_PORT \
                              path=/dispatched';
        echo ok;
    }

    location /dispatched {
        return 201;
    }
--- response_body
ok
--- no_error_log
[error]
[crit]



=== TEST 11: proxy_wasm - dispatch_http_call() "Content-Length" response body
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=127.0.0.1:$TEST_NGINX_SERVER_PORT \
                              path=/dispatched';
        echo fail;
    }

    location /dispatched {
        return 200 "Hello world";
    }
--- response_body_like
Hello world
--- no_error_log
[error]
[crit]



=== TEST 12: proxy_wasm - dispatch_http_call() "Transfer-Encoding: chunked" response body
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
--- response_body_like
GET \/dispatched HTTP\/1\.1.*
Host: 127\.0\.0\.1:\d+.*
Connection: close.*
Content-Length: 0.*
--- no_error_log
[error]
[crit]



=== TEST 13: proxy_wasm - dispatch_http_call() small wasm_socket_buffer_size
--- skip_no_debug: 4
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        wasm_socket_buffer_reuse off;
        wasm_socket_buffer_size 1;

        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=127.0.0.1:$TEST_NGINX_SERVER_PORT \
                              path=/dispatched';
        echo fail;
    }

    location /dispatched {
        return 200 "Hello world";
    }
--- response_body_like
Hello world
--- error_log
wasm tcp socket trying to receive data (max: 1)
--- no_error_log
[error]



=== TEST 14: proxy_wasm - dispatch_http_call() small wasm_socket_large_buffer_size
--- skip_no_debug: 4
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        wasm_socket_buffer_reuse off;
        wasm_socket_buffer_size 1;
        wasm_socket_large_buffer_size 4 12;

        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=127.0.0.1:$TEST_NGINX_SERVER_PORT \
                              path=/dispatched';
        echo fail;
    }

    location /dispatched {
        return 200 "Hello world";
    }
--- error_code: 500
--- ignore_response_body
--- error_log
tcp socket trying to receive data (max: 1)
tcp socket trying to receive data (max: 11)
tcp socket - upstream response headers too large, increase wasm_socket_large_buffer_size directive



=== TEST 15: proxy_wasm - dispatch_http_call() resolver error (host not found)
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    resolver 1.1.1.1;
    resolver_timeout 1s;

    location /t {
        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=zzzzzzz.zz';
        echo fail;
    }
--- error_code: 500
--- response_body_like: 500 Internal Server Error
--- error_log
[wasm] dispatch failed (tcp socket - resolver error: Host not found)
--- no_error_log
[crit]



=== TEST 16: proxy_wasm - dispatch_http_call() connection timeout
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    resolver 1.1.1.1;

    location /t {
        wasm_socket_connect_timeout 1ms;

        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=httpbin.org';
        echo fail;
    }

    location /dispatched {
        return 200 "Hello world";
    }
--- error_code: 500
--- response_body_like: 500 Internal Server Error
--- error_log eval
qr/\[error\] .*? \[wasm\] dispatch failed \(tcp socket - timed out connecting to \"\d+\.\d+\.\d+\.\d+:\d+\"\)/
--- no_error_log
[crit]



=== TEST 17: proxy_wasm - dispatch_http_call() connection timeout
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    resolver 1.1.1.1;

    location /t {
        wasm_socket_connect_timeout 1ms;

        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=httpbin.org \
                              path=/status/200';
        echo fail;
    }
--- error_code: 500
--- response_body_like: 500 Internal Server Error
--- error_log eval
qr/\[error\] .*? \[wasm\] dispatch failed \(tcp socket - timed out connecting to \".*?"\)/
--- no_error_log
[crit]



=== TEST 18: proxy_wasm - dispatch_http_call() read timeout
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
        echo_sleep 1;
        echo ok;
    }
--- error_code: 500
--- response_body_like: 500 Internal Server Error
--- error_log eval
qr/\[error\] .*? \[wasm\] dispatch failed \(tcp socket - timed out reading from \"127\.0\.0\.1:\d+\"\)/
--- no_error_log
[crit]



=== TEST 19: proxy_wasm - dispatch_http_call() write timeout
--- SKIP
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    resolver 1.1.1.1;

    location /t {
        wasm_socket_send_timeout 1ms;

        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=httpbin.org \
                              method=POST \
                              path=/anything \
                              headers=Content-Length:30 \
                              body=helloworld';
        echo fail;
    }

    location /dispatched {
        echo ok;
    }
--- error_code: 500
--- response_body_like: 500 Internal Server Error
--- error_log eval
qr/\[error\] .*? \[wasm\] dispatch failed \(tcp socket - timed out writing to \"\d+\.\d+\.\d+\.\d+:\d+\"\)/
--- no_error_log
[crit]
