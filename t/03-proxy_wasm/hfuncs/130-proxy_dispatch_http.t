# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasmX;

our $ExtResolver = $t::TestWasmX::extresolver;
our $ExtTimeout = $t::TestWasmX::exttimeout;

plan_tests(5);
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
qr/(\[error\]|Uncaught RuntimeError|\s+).*?dispatch failed: tcp socket - no host/
--- no_error_log
[crit]
on_http_call_response



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
qr/(\[error\]|Uncaught RuntimeError|\s+).*?dispatch failed: tcp socket - no host/
--- no_error_log
[crit]
on_http_call_response



=== TEST 3: proxy_wasm - dispatch_http_call() IP + invalid port
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=127.0.0.1:66000';
    }
--- error_code: 500
--- response_body_like: 500 Internal Server Error
--- error_log eval
qr/(\[error\]|Uncaught RuntimeError|\s+).*?dispatch failed: tcp socket - invalid port/
--- no_error_log
[crit]
on_http_call_response



=== TEST 4: proxy_wasm - dispatch_http_call() invalid IPv6
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=[0:0:0:0:0:0:0:1:$TEST_NGINX_SERVER_PORT';
    }
--- error_code: 500
--- response_body_like: 500 Internal Server Error
--- error_log eval
qr/(\[error\]|Uncaught RuntimeError|\s+).*?dispatch failed: tcp socket - invalid host/
--- no_error_log
[crit]
on_http_call_response



=== TEST 5: proxy_wasm - dispatch_http_call() hostname + invalid port
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=localhost:66000';
    }
--- error_code: 500
--- response_body_like: 500 Internal Server Error
--- error_log eval
qr/(\[error\]|Uncaught RuntimeError|\s+).*?dispatch failed: tcp socket - invalid port/
--- no_error_log
[crit]
on_http_call_response



=== TEST 6: proxy_wasm - dispatch_http_call() missing :method
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
qr/(\[error\]|Uncaught RuntimeError|\s+).*?dispatch failed: no :method/
--- no_error_log
[crit]
on_http_call_response



=== TEST 7: proxy_wasm - dispatch_http_call() missing :method but has a :method-prefixed header
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
qr/(\[error\]|Uncaught RuntimeError|\s+).*?dispatch failed: no :method/
--- no_error_log
[crit]
on_http_call_response



=== TEST 8: proxy_wasm - dispatch_http_call() missing :path
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
qr/(\[error\]|Uncaught RuntimeError|\s+).*?dispatch failed: no :path/
--- no_error_log
[crit]
on_http_call_response



=== TEST 9: proxy_wasm - dispatch_http_call() connection refused
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=127.0.0.1:81';
        echo ok;
    }
--- response_body
ok
--- error_log eval
[
    qr/(\[error\]|Uncaught RuntimeError|\s+).*?dispatch failed: tcp socket - Connection refused/,
    "dispatch_status: broken connection"
]
--- no_error_log
[crit]



=== TEST 10: proxy_wasm - dispatch_http_call() no resolver
--- SKIP: we now fallback to a default resolver for non-HTTP contexts
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
[
    qr/(\[error\]|Uncaught RuntimeError|\s+).*?dispatch failed: no resolver defined to resolve "localhost"/,
    "dispatch_status: resolver failure"
]
--- no_error_log
[crit]



=== TEST 11: proxy_wasm - dispatch_http_call() unix domain socket, no path
--- wasm_modules: hostcalls
--- config eval
qq{
    location /t {
        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=unix:';
    }
}
--- error_code: 500
--- response_body_like: 500 Internal Server Error
--- error_log eval
[
    qr/\[error\]/,
    qr/dispatch failed: tcp socket - no path in the unix domain socket/
]
--- no_error_log
on_http_call_response



=== TEST 12: proxy_wasm - dispatch_http_call() unix domain socket, bad path (ENOENT)
--- wasm_modules: hostcalls
--- config eval
qq{
    location /t {
        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=unix:/tmp/inexistent_file.sock';
        return 201;
    }
}
--- error_code: 201
--- response_body
--- error_log eval
[
    qr/\[crit\] .*? connect\(\) to unix:\/tmp\/inexistent_file\.sock failed .*? No such file or directory/,
    qr/\[error\] .*? dispatch failed: tcp socket - No such file or directory/,
    "dispatch_status: broken connection"
]



=== TEST 13: proxy_wasm - dispatch_http_call() unix domain socket, bad path (EISDIR)
Linux: "Connection refused"
macOS: "Socket operation on non-socket"
--- wasm_modules: hostcalls
--- config eval
qq{
    location /t {
        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=unix:/tmp';
        return 201;
    }
}
--- error_code: 201
--- response_body
--- error_log eval
[
    qr/\[(error|crit)\] .*? connect\(\) to unix:\/tmp failed .*? (Connection refused|Socket operation on non-socket)/,
    qr/\[error\] .*? dispatch failed: tcp socket - (Connection refused|Socket operation on non-socket)/,
    "dispatch_status: broken connection"
]



=== TEST 14: proxy_wasm - dispatch_http_call() unix domain socket, bad protocol prefix
--- wasm_modules: hostcalls
--- config eval
qq{
    location /t {
        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=uni:/tmp/file.sock';
    }
}
--- error_code: 500
--- response_body_like: 500 Internal Server Error
--- error_log eval
[
    qr/\[error\]/,
    qr/dispatch failed: tcp socket - invalid host/
]
--- no_error_log
on_http_call_response



=== TEST 15: proxy_wasm - dispatch_http_call() sanity (default resolver)
--- skip_eval: 5: defined $ENV{GITHUB_ACTIONS}
--- timeout eval: $::ExtTimeout
--- load_nginx_modules: ngx_http_echo_module
--- main_config eval
qq{
    wasm {
        resolver_timeout $::ExtTimeout;
        module hostcalls $t::TestWasmX::crates/hostcalls.wasm;
    }
}
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=httpbin.org';
        echo ok;
    }
--- response_body
ok
--- no_error_log
[error]
[crit]
[alert]



=== TEST 16: proxy_wasm - dispatch_http_call() on_request_body, no output
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
        echo ok;
    }
--- request
POST /t

Hello world
--- response_body
ok
--- no_error_log
[error]
[crit]
[alert]



=== TEST 17: proxy_wasm - dispatch_http_call() on_request_body, with output
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
                              path=/dispatched \
                              on_http_call_response=echo_response_body';
        echo failed;
    }
--- request
POST /t

Hello world
--- response_body
Hello back
--- no_error_log
[error]
[crit]
[alert]



=== TEST 18: proxy_wasm - dispatch_http_call() sanity unix domain socket
:authority set to "localhost"
--- valgrind
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- http_config eval
qq{
    server {
        listen unix:$ENV{TEST_NGINX_UNIX_SOCKET};

        location /headers {
            echo \$echo_client_request_headers;
        }
    }
}
--- config eval
qq{
    location /t {
        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=unix:$ENV{TEST_NGINX_UNIX_SOCKET} \
                              path=/headers \
                              headers=X-Thing:foo|X-Thing:bar|Hello:world \
                              on_http_call_response=echo_response_body';
        echo fail;
    }
}
--- response_body_like
Host: localhost\s*
.*?
\s*X-Thing: foo\s*
\s*X-Thing: bar\s*
\s*Hello: world\s*
--- no_error_log
[error]
[crit]
[alert]



=== TEST 19: proxy_wasm - dispatch_http_call() sanity resolver + hostname (IPv4), default port
macOS: intermitent failures
Needs IPv4 resolution + external I/O to succeed.
Succeeds on:
- HTTP 200 (httpbin.org/headers success)
- HTTP 502 (httpbin.org Bad Gateway)
- HTTP 503 (httpbin.org Service Temporarily Unavailable)
- HTTP 504 (httpbin.org Gateway timeout)
--- skip_eval: 5: $::osname =~ m/darwin/
--- valgrind
--- timeout eval: $::ExtTimeout
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config eval
qq{
    resolver          $::ExtResolver ipv6=off;
    resolver_timeout  $::ExtTimeout;

    location /t {
        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=httpbin.org \
                              path=/headers \
                              headers=X-Thing:foo|X-Thing:bar|Hello:world \
                              on_http_call_response=echo_response_body';
        echo fail;
    }
}
--- error_code_like: (200)
--- response_body_like
(\s*"Hello": "world",\s*
.*?
\s*"X-Thing": "foo,bar"\s*|.*?502 Bad Gateway.*|.*?503 Service Temporarily Unavailable.*|.*?504 Gateway Time-out.*)
--- no_error_log
[error]
[crit]
[alert]



=== TEST 20: proxy_wasm - dispatch_http_call() sanity resolver + hostname (IPv6), default port
Disabled on GitHub Actions due to IPv6 constraint.
--- skip_eval: 5: system("ping6 -c 1 ::1 >/dev/null 2>&1") ne 0 || defined $ENV{GITHUB_ACTIONS}
--- timeout eval: $::ExtTimeout
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config eval
qq{
    resolver          $::ExtResolver ipv6=on;
    resolver_timeout  $::ExtTimeout;

    location /t {
        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=ipv6.google.com \
                              path=/ \
                              on_http_call_response=echo_response_body';
        echo fail;
    }
}
--- response_body_like
\A\<!doctype html\>.*
--- no_error_log
[error]
[crit]
[alert]



=== TEST 21: proxy_wasm - dispatch_http_call() sanity resolver + hostname (IPv4) + port
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- http_config
server {
    listen       $TEST_NGINX_SERVER_PORT2;
    server_name  localhost;

    location /headers {
        echo $echo_client_request_headers;
    }
}
--- config eval
qq{
    resolver      $::ExtResolver ipv6=off;
    resolver_add  127.0.0.1 localhost;

    location /t {
        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=localhost:$ENV{TEST_NGINX_SERVER_PORT2} \
                              path=/headers \
                              headers=X-Thing:foo|X-Thing:bar|Hello:world \
                              on_http_call_response=echo_response_body';
        echo fail;
    }
}
--- response_body_like
.*?
\s*X-Thing: foo\s*
\s*X-Thing: bar\s*
\s*Hello: world\s*
--- no_error_log
[error]
[crit]
[alert]



=== TEST 22: proxy_wasm - dispatch_http_call() resolver error (host not found)
--- valgrind
--- timeout eval: $::ExtTimeout
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config eval
qq{
    resolver          $::ExtResolver;
    resolver_timeout  $::ExtTimeout;

    location /t {
        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=nosuchdomainexists.org';
        echo ok;
    }
}
--- response_body
ok
--- error_log eval
[
    qr/(\[error\]|Uncaught RuntimeError|\s+).*?dispatch failed: tcp socket - resolver error: Host not found/,
    "dispatch_status: resolver failure"
]
--- no_error_log
[crit]



=== TEST 23: proxy_wasm - dispatch_http_call() sanity IP + port (IPv4)
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=127.0.0.1:$TEST_NGINX_SERVER_PORT \
                              path=/dispatch \
                              on_http_call_response=echo_response_body';
        echo fail;
    }

    location /dispatch {
        echo ok;
    }
--- response_body
ok
--- no_error_log
[error]
[crit]
[alert]



=== TEST 24: proxy_wasm - dispatch_http_call() sanity IP + port (IPv6)
--- valgrind
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    listen  [::]:$TEST_NGINX_SERVER_PORT;

    location /t {
        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=[0:0:0:0:0:0:0:1]:$TEST_NGINX_SERVER_PORT \
                              path=/dispatch \
                              on_http_call_response=echo_response_body';
        echo fail;
    }

    location /dispatch {
        echo ok;
    }
--- response_body
ok
--- no_error_log
[error]
[crit]
[alert]



=== TEST 25: proxy_wasm - dispatch_http_call() :authority overrides Host with TCP/IP
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- http_config
server {
    listen       $TEST_NGINX_SERVER_PORT2;
    server_name  hostname;

    location /headers {
        echo $echo_client_request_headers;
    }
}
--- config eval
qq{
    resolver      $::ExtResolver ipv6=off;
    resolver_add  127.0.0.1 localhost;

    location /t {
        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=localhost:$ENV{TEST_NGINX_SERVER_PORT2} \
                              authority=hostname:1234 \
                              path=/headers \
                              on_http_call_response=echo_response_body';
        echo fail;
    }
}
--- response_body_like
\s*Host: hostname:1234\s*
--- no_error_log
[error]
[crit]
[alert]



=== TEST 26: proxy_wasm - dispatch_http_call() :authority overrides Host with unix domain socket
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- http_config eval
qq{
    server {
        listen       unix:$ENV{TEST_NGINX_UNIX_SOCKET};
        server_name  hostname;

        location /headers {
            echo \$echo_client_request_headers;
        }
    }
}
--- config eval
qq{
    location /t {
        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=unix:$ENV{TEST_NGINX_UNIX_SOCKET} \
                              authority=localhost:1234 \
                              path=/headers \
                              headers=X-Thing:foo|X-Thing:bar|Hello:world \
                              on_http_call_response=echo_response_body';
        echo fail;
    }
}
--- response_body_like
\s*Host: localhost:1234\s*
.*?
\s*X-Thing: foo\s*
\s*X-Thing: bar\s*
\s*Hello: world\s*
--- no_error_log
[error]
[crit]
[alert]



=== TEST 27: proxy_wasm - dispatch_http_call() many request headers (> 10)
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /dispatched {
        echo $echo_client_request_headers;
    }

    location /t {
        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=127.0.0.1:$TEST_NGINX_SERVER_PORT \
                              path=/dispatched \
                              headers=A:1|B:2|C:3|D:4|E:5|F:6|G:7|H:8|I:9|J:10|K:11 \
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
[alert]



=== TEST 28: proxy_wasm - dispatch_http_call() empty response body (HTTP 204)
--- skip_no_debug
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
[crit]



=== TEST 29: proxy_wasm - dispatch_http_call() empty response body (Content-Length: 0)
--- skip_no_debug
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
[crit]



=== TEST 30: proxy_wasm - dispatch_http_call() no response body (HEAD)
--- skip_no_debug
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
[crit]



=== TEST 31: proxy_wasm - dispatch_http_call() "Content-Length" response body
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
[alert]



=== TEST 32: proxy_wasm - dispatch_http_call() "Transfer-Encoding: chunked" response body
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
[alert]



=== TEST 33: proxy_wasm - dispatch_http_call() large response
--- valgrind
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
[alert]



=== TEST 34: proxy_wasm - dispatch_http_call() small wasm_socket_buffer_size
--- valgrind
--- skip_no_debug
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
[crit]



=== TEST 35: proxy_wasm - dispatch_http_call() small wasm_socket_large_buffers
--- valgrind
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
[alert]



=== TEST 36: proxy_wasm - dispatch_http_call() too small wasm_socket_large_buffers
--- valgrind
--- skip_no_debug
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
--- ignore_response_body
--- error_log
tcp socket trying to receive data (max: 1)
tcp socket trying to receive data (max: 11)
tcp socket - upstream response headers too large, increase wasm_socket_large_buffers size
dispatch_status: reader failure



=== TEST 37: proxy_wasm - dispatch_http_call() too small wasm_socket_large_buffers size
--- valgrind
--- skip_no_debug
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
--- ignore_response_body
--- error_log
tcp socket trying to receive data (max: 24)
tcp socket trying to receive data (max: 5)
tcp socket - upstream response headers too large, increase wasm_socket_large_buffers size
dispatch_status: reader failure



=== TEST 38: proxy_wasm - dispatch_http_call() not enough wasm_socket_large_buffers
--- valgrind
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
        echo ok;
    }
--- response_body
ok
--- error_log eval
[
    qr/tcp socket - not enough large buffers available, increase wasm_socket_large_buffers number/,
    qr/(\[error\]|Uncaught RuntimeError|\s+).*?dispatch failed: tcp socket - parser error/,
    "dispatch_status: reader failure"
]



=== TEST 39: proxy_wasm - dispatch_http_call() scarce wasm_socket_large_buffers
--- valgrind
--- skip_no_debug
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
on_http_call_response



=== TEST 40: proxy_wasm - dispatch_http_call() TCP reader error
--- valgrind
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
        echo ok;
    }
--- response_body
ok
--- error_log eval
[
    qr/(\[error\]|Uncaught RuntimeError|\s+).*?dispatch failed: tcp socket - parser error/,
    "dispatch_status: reader failure"
]
--- no_error_log
[crit]



=== TEST 41: proxy_wasm - dispatch_http_call() re-entrant after status line
--- skip_no_debug
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
on_http_call_response



=== TEST 42: proxy_wasm - dispatch_http_call() trap in dispatch handler
--- valgrind
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
        echo ok;
    }
--- response_body
ok
--- error_log eval
[
    "on_http_call_response",
    qr/\[crit\] .*? panicked at/,
    qr/trap!/,
]



=== TEST 43: proxy_wasm - dispatch_http_call() invalid response headers
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
        echo ok;
    }
--- response_body
ok
--- error_log eval
[
    qr/(\[error\]|Uncaught RuntimeError|\s+).*?dispatch failed: tcp socket - parser error/,
    "dispatch_status: reader failure"
]
--- no_error_log
[crit]



=== TEST 44: proxy_wasm - dispatch_http_call() async dispatch x2
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
[alert]



=== TEST 45: proxy_wasm - dispatch_http_call() can be chained by different filters
So long as these filters do not produce content.
--- valgrind
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /status/200 {
        return 200;
    }

    location /status/201 {
        return 201;
    }

    location /t {
        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=127.0.0.1:$TEST_NGINX_SERVER_PORT \
                              path=/status/200';
        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=127.0.0.1:$TEST_NGINX_SERVER_PORT \
                              path=/status/201';
        echo ok;
    }
--- ignore_response_body
--- grep_error_log eval: qr/\*\d+.*?on_http_call_response.*/
--- grep_error_log_out eval
qr/^\*\d+ .*? on_http_call_response \(id: \d+[^*]*
\*\d+ .*? on_http_call_response \(id: \d+[^*]*$/
--- no_error_log
[error]
[crit]
[alert]



=== TEST 46: proxy_wasm - dispatch_http_call() supports get_http_call_response_headers()
--- valgrind
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /dispatched {
        add_header X-Callout-Header callout-header-value;
        echo ok;
    }

    location /t {
        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=127.0.0.1:$TEST_NGINX_SERVER_PORT \
                              path=/dispatched \
                              on_http_call_response=echo_response_headers';
        echo failed;
    }
--- response_body_like
X-Callout-Header: callout-header-value
--- no_error_log
[error]
[crit]
[alert]



=== TEST 47: proxy_wasm - dispatch_http_call() get :status dispatch response header
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /status/200 {
        return 200;
    }

    location /status/201 {
        return 201;
    }

    location /status/500 {
        return 500;
    }

    location /t {
        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=127.0.0.1:$TEST_NGINX_SERVER_PORT \
                              path=/status/200';

        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=127.0.0.1:$TEST_NGINX_SERVER_PORT \
                              path=/status/201';

        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=127.0.0.1:$TEST_NGINX_SERVER_PORT \
                              path=/status/500';

        echo ok;
    }
--- ignore_response_body
--- grep_error_log eval: qr/\*\d+.*?on_http_call_response.*/
--- grep_error_log_out eval
qr/^\*\d+ .*? on_http_call_response \(id: \d+, status: 200[^*]*
\*\d+ .*? on_http_call_response \(id: \d+, status: 201[^*]*
\*\d+ .*? on_http_call_response \(id: \d+, status: 500[^*]*$/
--- no_error_log
[error]
[crit]
[alert]



=== TEST 48: proxy_wasm - dispatch_http_call() with dispatched request body
--- valgrind
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /echo {
        proxy_wasm hostcalls 'on=request_body \
                              test=/t/echo/body';
    }

    location /t {
        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=127.0.0.1:$TEST_NGINX_SERVER_PORT \
                              method=POST \
                              path=/echo \
                              body=helloworld \
                              on_http_call_response=echo_response_body';
        echo failed;
    }
--- response_body
helloworld
--- no_error_log
[error]
[crit]
[alert]



=== TEST 49: proxy_wasm - dispatch_http_call() override Content-Length
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /echo {
        proxy_wasm hostcalls 'on=request_body \
                              test=/t/echo/body';
    }

    location /t {
        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=127.0.0.1:$TEST_NGINX_SERVER_PORT \
                              method=POST \
                              path=/echo \
                              headers=Content-Length:9 \
                              body=helloworld \
                              on_http_call_response=echo_response_body';
        echo failed;
    }
--- response_body
helloworl
--- no_error_log
[error]
[crit]
[alert]



=== TEST 50: proxy_wasm - dispatch_http_call() cannot override hard-coded request headers
--- skip_no_debug
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /status/200 {
        return 200;
    }

    location /t {
        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=127.0.0.1:$TEST_NGINX_SERVER_PORT \
                              path=/status/200';
        echo ok;
    }
--- ignore_response_body
--- error_log
cannot override the "Host" header, skipping
cannot override the "Connection" header, skipping
--- no_error_log
[error]
[crit]



=== TEST 51: proxy_wasm - dispatch_http_call() can use ':path' with querystring
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /dispatched {
        return 200 "$request_uri $uri $is_args $args";
    }

    location /t {
        proxy_wasm hostcalls 'on=request_headers \
                              test=/t/dispatch_http_call \
                              host=127.0.0.1:$TEST_NGINX_SERVER_PORT \
                              path=/dispatched?foo=bar \
                              on_http_call_response=echo_response_body';
        echo failed;
    }
--- response_body
/dispatched?foo=bar /dispatched ? foo=bar
--- no_error_log
[error]
[crit]
[alert]



=== TEST 52: proxy_wasm - dispatch_http_call() can use ':path' with querystring, passes through invalid characters
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /dispatched {
        return 200 "Hello back $request_uri $uri $is_args $args";
    }

    location /t {
        proxy_wasm hostcalls 'on=request_headers \
                              test=/t/dispatch_http_call \
                              host=127.0.0.1:$TEST_NGINX_SERVER_PORT \
                              path=/dispatched?foo=bár%20bla \
                              on_http_call_response=echo_response_body';
        echo failed;
    }
--- response_body
Hello back /dispatched?foo=bár%20bla /dispatched ? foo=bár%20bla
--- no_error_log
[error]
[crit]
[alert]
