# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

skip_valgrind('wasmtime');

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
qr/\[error\] .*? dispatch failed \(no host\)/
--- no_error_log
[crit]



=== TEST 2: proxy_wasm - dispatch_http_call() empty host
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
qr/\[error\] .*? dispatch failed \(invalid port\)/
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
                              path=/headers \
                              on_http_call_response=echo_response_body';
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
                              path=/ \
                              on_http_call_response=echo_response_body';
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



=== TEST 12: proxy_wasm - dispatch_http_call() many request headers (> 10)
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /dispatched {
        echo $echo_client_request_headers;
    }

    location /t {
        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=127.0.0.1:$TEST_NGINX_SERVER_PORT \
                              headers=A:1|B:2|C:3|D:4|E:5|F:6|G:7|H:8|I:9|J:10|K:11 \
                              path=/dispatched \
                              on_http_call_response=echo_response_body';
        echo ok;
    }
--- response_body_like
GET \/dispatched HTTP\/1\.1.*?
Host: .*?
Connection: close.*?
Content-Length: 0.*?
A: 1.*?
B: 2.*?
C: 3.*?
D: 4.*?
E: 5.*?
F: 6.*?
G: 7.*?
H: 8.*?
I: 9.*?
J: 10.*?
K: 11.*
--- no_error_log
[error]
[crit]



=== TEST 13: proxy_wasm - dispatch_http_call() empty response body (HTTP 204)
--- skip_no_debug: 4
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /dispatched {
        return 204;
    }

    location /t {
        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=127.0.0.1:$TEST_NGINX_SERVER_PORT \
                              path=/dispatched';
        echo ok;
    }
--- response_body
ok
--- grep_error_log eval: qr/(tcp socket (eof|no bytes|reading|closing)|wasm http reader status).*/
--- grep_error_log_out
wasm http reader status 204 "204 No Content"
tcp socket eof
tcp socket no bytes to parse
tcp socket reading done
tcp socket closing
--- no_error_log
[error]



=== TEST 14: proxy_wasm - dispatch_http_call() empty response body (Content-Length: 0)
--- skip_no_debug: 4
--- load_nginx_modules: ngx_http_echo_module ngx_http_headers_more_filter_module
--- wasm_modules: hostcalls
--- config
    location /dispatched {
        more_set_headers 'Content-Length: 0';
        return 200 OK;
    }

    location /t {
        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=127.0.0.1:$TEST_NGINX_SERVER_PORT \
                              path=/dispatched';
        echo ok;
    }
--- response_body
ok
--- grep_error_log eval: qr/(tcp socket (eof|no bytes|reading|closing)|wasm http reader status).*/
--- grep_error_log_out
wasm http reader status 200 "200 OK"
tcp socket reading done
tcp socket closing
--- no_error_log
[error]



=== TEST 15: proxy_wasm - dispatch_http_call() no response body (HEAD)
--- skip_no_debug: 4
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /dispatched {
        return 201 OK;
    }

    location /t {
        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=127.0.0.1:$TEST_NGINX_SERVER_PORT \
                              method=HEAD \
                              path=/dispatched';
        echo ok;
    }
--- response_body
ok
--- grep_error_log eval: qr/(tcp socket (eof|no bytes|reading|closing)|wasm http reader status).*/
--- grep_error_log_out
wasm http reader status 201 "201 Created"
tcp socket eof
tcp socket no bytes to parse
tcp socket reading done
tcp socket closing
--- no_error_log
[error]



=== TEST 16: proxy_wasm - dispatch_http_call() "Content-Length" response body
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
        echo fail;
    }
--- response_body_like
Hello world
--- no_error_log
[error]
[crit]



=== TEST 17: proxy_wasm - dispatch_http_call() "Transfer-Encoding: chunked" response body
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /dispatched {
        echo -n $echo_client_request_headers;
    }

    location /t {
        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=127.0.0.1:$TEST_NGINX_SERVER_PORT \
                              path=/dispatched \
                              on_http_call_response=echo_response_body';
    }
--- response_body_like
GET \/dispatched HTTP\/1\.1.*
Host: 127\.0\.0\.1:\d+.*
Connection: close.*
Content-Length: 0.*
--- no_error_log
[error]
[crit]



=== TEST 18: proxy_wasm - dispatch_http_call() large response
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        wasm_socket_buffer_reuse off;
        wasm_socket_buffer_size 256;

        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=127.0.0.1:$TEST_NGINX_SERVER_PORT \
                              path=/bytes \
                              on_http_call_response=echo_response_body';
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



=== TEST 19: proxy_wasm - dispatch_http_call() small wasm_socket_buffer_size
--- skip_no_debug: 4
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /dispatched {
        return 200 "Hello world";
    }

    location /t {
        wasm_socket_buffer_reuse off;
        wasm_socket_buffer_size 1;

        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=127.0.0.1:$TEST_NGINX_SERVER_PORT \
                              path=/dispatched \
                              on_http_call_response=echo_response_body';
        echo fail;
    }
--- response_body_like
Hello world
--- error_log
tcp socket trying to receive data (max: 1)
--- no_error_log
[error]



=== TEST 20: proxy_wasm - dispatch_http_call() small wasm_socket_large_buffers
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /dispatched {
        return 200 "Hello world";
    }

    location /t {
        wasm_socket_buffer_reuse off;
        wasm_socket_buffer_size 1;
        wasm_socket_large_buffers 4 1024;

        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=127.0.0.1:$TEST_NGINX_SERVER_PORT \
                              path=/dispatched \
                              on_http_call_response=echo_response_body';
        echo fail;
    }
--- response_body
Hello world
--- no_error_log
[error]
[crit]



=== TEST 21: proxy_wasm - dispatch_http_call() too small wasm_socket_large_buffers
--- skip_no_debug: 4
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /dispatched {
        return 200 "Hello world";
    }

    location /t {
        wasm_socket_buffer_reuse off;
        wasm_socket_buffer_size 1;
        wasm_socket_large_buffers 4 12;

        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=127.0.0.1:$TEST_NGINX_SERVER_PORT \
                              path=/dispatched';
        echo ok;
    }
--- error_code: 500
--- ignore_response_body
--- error_log
tcp socket trying to receive data (max: 1)
tcp socket trying to receive data (max: 11)
tcp socket - upstream response headers too large, increase wasm_socket_large_buffers size



=== TEST 22: proxy_wasm - dispatch_http_call() too small wasm_socket_large_buffers size
--- skip_no_debug: 4
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /dispatched {
        return 200 "Hello world";
    }

    location /t {
        wasm_socket_buffer_reuse off;
        wasm_socket_buffer_size 24;  # enough to fit status line
        wasm_socket_large_buffers 4 12;

        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=127.0.0.1:$TEST_NGINX_SERVER_PORT \
                              path=/dispatched';
        echo ok;
    }
--- error_code: 500
--- ignore_response_body
--- error_log
tcp socket trying to receive data (max: 24)
tcp socket trying to receive data (max: 5)
tcp socket - upstream response headers too large, increase wasm_socket_large_buffers size



=== TEST 23: proxy_wasm - dispatch_http_call() not enough wasm_socket_large_buffers
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- tcp_listen: 12345
--- tcp_reply eval
sub {
    return ["HTTP/1.1 200 OK\r\n",
            "Connection: close\r\n",
            "Content-Length: 0\r\n",
            "\r\n"];
}
--- config
    location /t {
        wasm_socket_buffer_reuse off;
        wasm_socket_buffer_size 24;  # enough to fit status line
        wasm_socket_large_buffers 1 24;

        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=127.0.0.1:12345';
        echo fail;
    }
--- error_code: 500
--- response_body_like: 500 Internal Server Error
--- error_log
tcp socket - not enough large buffers available, increase wasm_socket_large_buffers number
dispatch failed (tcp socket - parser error)



=== TEST 24: proxy_wasm - dispatch_http_call() scarce wasm_socket_large_buffers
--- skip_no_debug: 4
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /dispatched {
        return 200 "Hello world";
    }

    location /t {
        wasm_socket_buffer_reuse off;
        wasm_socket_buffer_size 1;
        wasm_socket_large_buffers 1 1024;

        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=127.0.0.1:$TEST_NGINX_SERVER_PORT \
                              path=/dispatched \
                              on_http_call_response=echo_response_body';
        echo fail;
    }
--- response_body
Hello world
--- error_log
tcp socket trying to receive data (max: 1)
tcp socket trying to receive data (max: 1023)



=== TEST 25: proxy_wasm - dispatch_http_call() TCP reader error
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- tcp_listen: 12345
--- tcp_reply eval
sub {
    my $req = shift;
    return "hello, $req";
}
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=127.0.0.1:12345';
        echo fail;
    }
--- error_code: 500
--- response_body_like: 500 Internal Server Error
--- error_log
dispatch failed (tcp socket - parser error)
--- no_error_log
[crit]



=== TEST 26: proxy_wasm - dispatch_http_call() re-entrant after status line
--- skip_no_debug: 4
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /dispatched {
        return 200 "Hello world";
    }

    location /t {
        wasm_socket_buffer_reuse off;
        wasm_socket_buffer_size 24;  # enough to fit status line
        wasm_socket_large_buffers 4 1k;

        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=127.0.0.1:$TEST_NGINX_SERVER_PORT \
                              path=/dispatched \
                              on_http_call_response=echo_response_body';
        echo fail;
    }
--- response_body
Hello world
--- error_log
tcp socket trying to receive data (max: 24)
tcp socket trying to receive data (max: 1017)



=== TEST 27: proxy_wasm - dispatch_http_call() trap in dispatch handler
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
                              on_http_call_response=trap';
        echo fail;
    }
--- error_code: 500
--- response_body_like: 500 Internal Server Error
--- error_log
panicked at 'trap!'
trap in proxy_on_http_call_response



=== TEST 28: proxy_wasm - dispatch_http_call() invalid response headers
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- tcp_listen: 12345
--- tcp_reply eval
sub {
    return ["HTTP/1.1 200 OK\r\n",
            "Connection: \0",
            "\r\n\r\n"];
}
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=127.0.0.1:12345 \
                              on_http_call_response=echo_response_body';
        echo failed;
    }
--- error_code: 500
--- response_body_like: 500 Internal Server Error
--- error_log
dispatch failed (tcp socket - parser error)
--- no_error_log
[crit]



=== TEST 29: proxy_wasm - dispatch_http_call() async dispatch x2
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    wasm_socket_buffer_reuse on;
    wasm_socket_buffer_size 24;
    wasm_socket_large_buffers 32 128;

    location /dispatched {
        echo_sleep 0.5;
        echo_duplicate 12 "a";
    }

    location /dispatched2 {
        echo_duplicate 12 "b";
    }

    location /proxy_wasm1 {
        internal;
        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=127.0.0.1:$TEST_NGINX_SERVER_PORT \
                              path=/dispatched \
                              on_http_call_response=echo_response_body';
        echo fail;
    }

    location /proxy_wasm2 {
        internal;
        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=127.0.0.1:$TEST_NGINX_SERVER_PORT \
                              path=/dispatched2 \
                              on_http_call_response=echo_response_body';
        echo fail;
    }

    location /t {
        echo_subrequest_async GET /proxy_wasm1;
        echo_subrequest_async GET /proxy_wasm2;
    }
--- response_body_like
[a]{12}
[b]{12}
--- no_error_log
[error]
[crit]



=== TEST 30: proxy_wasm - dispatch_http_call() on log step
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /dispatched {
        return 200 "Hello world";
    }

    location /t {
        proxy_wasm hostcalls 'on=log \
                              test=/t/dispatch_http_call \
                              host=127.0.0.1:$TEST_NGINX_SERVER_PORT \
                              path=/dispatched';
        echo fail;
    }
--- response_body
fail
--- error_log eval
qr/trap in proxy_on_log:.*? dispatch failed: bad step/
--- no_error_log
[crit]



=== TEST 31: proxy_wasm - dispatch_http_call() missing :method but has a :method-prefixed header
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=127.0.0.1 \
                              no_method=true \
                              method_prefixed_header=yes';
    }
--- error_code: 500
--- response_body_like: 500 Internal Server Error
--- error_log eval
qr/\[error\] .*? dispatch failed: no :method/
--- no_error_log
[crit]
