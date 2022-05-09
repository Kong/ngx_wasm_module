# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

#skip_valgrind();
no_long_string();

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



=== TEST 9: proxy_wasm - dispatch_http_call() resolver + hostname (IPv4)
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    resolver 1.1.1.1 ipv6=off;

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



=== TEST 10: proxy_wasm - dispatch_http_call() resolver + hostname (IPv6)
--- skip_eval: 4: defined $ENV{GITHUB_ACTIONS}
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    resolver 1.1.1.1 ipv6=on;

    location /t {
        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=ipv6.google.com \
                              path=/';
        echo fail;
    }
--- response_body_like
\A\<!doctype html\>.*
--- no_error_log
[error]
[crit]



=== TEST 11: proxy_wasm - dispatch_http_call() resolver error (host not found)
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    resolver 1.1.1.1;

    location /t {
        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=foo';
        echo ok;
    }
--- error_code: 500
--- response_body_like: 500 Internal Server Error
--- error_log
[wasm] dispatch failed (tcp socket - resolver error: Host not found)
--- no_error_log
[crit]



=== TEST 12: proxy_wasm - dispatch_http_call() empty response body
--- skip_no_debug: 4
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
--- grep_error_log eval: qr/(wasm tcp socket (eof|no bytes|reading|closing)|wasm http reader status).*/
--- grep_error_log_out
wasm http reader status 201 "201 Created"
wasm tcp socket reading done
wasm tcp socket closing
--- no_error_log
[error]



=== TEST 13: proxy_wasm - dispatch_http_call() no response body (HEAD)
--- skip_no_debug: 4
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=127.0.0.1:$TEST_NGINX_SERVER_PORT \
                              method=HEAD \
                              path=/dispatched';
        echo ok;
    }

    location /dispatched {
        return 201 OK;
    }
--- response_body
ok
--- grep_error_log eval: qr/(wasm tcp socket (eof|no bytes|reading|closing)|wasm http reader status).*/
--- grep_error_log_out
wasm http reader status 201 "201 Created"
wasm tcp socket eof
wasm tcp socket no bytes to parse
wasm tcp socket reading done
wasm tcp socket closing
--- no_error_log
[error]



=== TEST 14: proxy_wasm - dispatch_http_call() "Content-Length" response body
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



=== TEST 15: proxy_wasm - dispatch_http_call() "Transfer-Encoding: chunked" response body
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



=== TEST 16: proxy_wasm - dispatch_http_call() large response
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        wasm_socket_buffer_reuse off;
        wasm_socket_buffer_size 256;

        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=127.0.0.1:$TEST_NGINX_SERVER_PORT \
                              path=/bytes';
        echo fail;
    }

    location /bytes {
        echo_duplicate 512 "a";
    }
--- response_body_like
[a]{512}
--- no_error_log
[error]
[crit]



=== TEST 17: proxy_wasm - dispatch_http_call() small wasm_socket_buffer_size
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



=== TEST 18: proxy_wasm - dispatch_http_call() small wasm_socket_large_buffers
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        wasm_socket_buffer_reuse off;
        wasm_socket_buffer_size 1;
        wasm_socket_large_buffers 4 1024;

        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=127.0.0.1:$TEST_NGINX_SERVER_PORT \
                              path=/dispatched';
        echo fail;
    }

    location /dispatched {
        return 200 "Hello world";
    }
--- response_body
Hello world
--- no_error_log
[error]
[crit]



=== TEST 19: proxy_wasm - dispatch_http_call() too small wasm_socket_large_buffers
--- skip_no_debug: 4
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        wasm_socket_buffer_reuse off;
        wasm_socket_buffer_size 1;
        wasm_socket_large_buffers 4 12;

        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=127.0.0.1:$TEST_NGINX_SERVER_PORT \
                              path=/dispatched';
        echo ok;
    }

    location /dispatched {
        return 200 "Hello world";
    }
--- error_code: 500
--- ignore_response_body
--- error_log
tcp socket trying to receive data (max: 1)
tcp socket trying to receive data (max: 11)
tcp socket - upstream response headers too large, increase wasm_socket_large_buffers size



=== TEST 20: proxy_wasm - dispatch_http_call() too small wasm_socket_large_buffers after status line
--- skip_no_debug: 4
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        wasm_socket_buffer_reuse off;
        wasm_socket_buffer_size 24;  # enough to fit status line
        wasm_socket_large_buffers 4 12;

        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=127.0.0.1:$TEST_NGINX_SERVER_PORT \
                              path=/dispatched';
        echo ok;
    }

    location /dispatched {
        return 200 "Hello world";
    }
--- error_code: 500
--- ignore_response_body
--- error_log
tcp socket trying to receive data (max: 24)
tcp socket trying to receive data (max: 5)
tcp socket - upstream response headers too large, increase wasm_socket_large_buffers size



=== TEST 21: proxy_wasm - dispatch_http_call() few wasm_socket_large_buffers
--- skip_no_debug: 4
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        wasm_socket_buffer_reuse off;
        wasm_socket_buffer_size 1;
        wasm_socket_large_buffers 1 1024;

        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=127.0.0.1:$TEST_NGINX_SERVER_PORT \
                              path=/dispatched';
        echo fail;
    }

    location /dispatched {
        return 200 "Hello world";
    }
--- response_body
Hello world
--- error_log
tcp socket trying to receive data (max: 1)
tcp socket trying to receive data (max: 1023)



=== TEST 22: proxy_wasm - dispatch_http_call() re-entrant after status line
--- skip_no_debug: 4
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        wasm_socket_buffer_reuse off;
        wasm_socket_buffer_size 24;  # enough to fit status line
        wasm_socket_large_buffers 4 1k;

        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=127.0.0.1:$TEST_NGINX_SERVER_PORT \
                              path=/dispatched';
        echo fail;
    }

    location /dispatched {
        return 200 "Hello world";
    }
--- response_body
Hello world
--- error_log
tcp socket trying to receive data (max: 24)
tcp socket trying to receive data (max: 1017)



=== TEST 23: proxy_wasm - dispatch_http_call() trap in dispatch handler
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=127.0.0.1:$TEST_NGINX_SERVER_PORT \
                              path=/dispatched \
                              trap=on';
        echo fail;
    }

    location /dispatched {
        return 200 "Hello world";
    }
--- error_code: 500
--- response_body_like: 500 Internal Server Error
--- error_log
panicked at 'trap!'
trap in proxy_on_http_call_response



=== TEST 24: proxy_wasm - dispatch_http_call() buffer reuse
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    wasm_socket_buffer_reuse on;
    wasm_socket_buffer_size 24;
    wasm_socket_large_buffers 32 128;

    location /t {
        echo_subrequest_async GET /proxy_wasm1;
        echo_subrequest_async GET /proxy_wasm2;
    }

    location /proxy_wasm1 {
        internal;
        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=127.0.0.1:$TEST_NGINX_SERVER_PORT \
                              path=/dispatched \
                              echo_response=on';
        echo fail;
    }

    location /proxy_wasm2 {
        internal;
        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=127.0.0.1:$TEST_NGINX_SERVER_PORT \
                              path=/dispatched2 \
                              echo_response=on';
        echo fail;
    }

    location /dispatched {
        echo_sleep 0.5;
        echo_duplicate 12 "a";
    }

    location /dispatched2 {
        echo_duplicate 12 "b";
    }
--- response_body_like
[a]{12}
[b]{12}
--- no_error_log
[error]
[crit]



=== TEST 25: proxy_wasm - dispatch_http_call() in log phase
--- SKIP
--- skip_no_debug: 4
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=127.0.0.1:$TEST_NGINX_SERVER_PORT \
                              path=/dispatched';

        proxy_wasm hostcalls 'on=log \
                              test=/t/dispatch_http_call \
                              host=127.0.0.1:$TEST_NGINX_SERVER_PORT \
                              path=/dispatched';

        echo fail;
    }

    location /dispatched {
        return 200 "Hello world";
    }
--- response_body
Hello world
--- error_log
tcp socket trying to receive data (max: 1)
tcp socket trying to receive data (max: 1023)
