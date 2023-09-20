# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

plan tests => repeat_each() * (blocks() * 4);

run_tests();

__DATA__

=== TEST 1: client connection drop - proxy_wasm & echo, chained filters
Filters executing on a request from the lost connection have their response
entrypoints invoked.
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: on_phases
--- config
    location /t {
        proxy_wasm on_phases;
        proxy_wasm on_phases;
        echo ok;
    }
--- shutdown
--- ignore_response
--- grep_error_log eval: qr/#\d+ on_(response|log|done).*/
--- grep_error_log_out eval
qr/\A#\d+ on_response_headers, 5 headers.*
#\d+ on_response_headers, 5 headers.*
#\d+ on_response_body, 3 bytes, eof: false.*
#\d+ on_response_body, 3 bytes, eof: false.*
#\d+ on_response_body, 0 bytes, eof: true.*
#\d+ on_response_body, 0 bytes, eof: true.*
#\d+ on_done.*
#\d+ on_log.*
#\d+ on_done.*
#\d+ on_log.*\Z/
--- no_error_log
[error]
[crit]
[emerg]



=== TEST 2: client connection drop - proxy_wasm & proxy_pass, chained filters
Filters executing a request from the lost connection have only their on_log and
on_done entrypoints invoked.
--- skip_eval: 4: $ENV{TEST_NGINX_USE_HUP} == 1
--- wasm_modules: on_phases
--- http_config eval
qq{
    server {
        listen unix:$ENV{TEST_NGINX_UNIX_SOCKET};

        location / {
            return 200;
        }
    }
}
--- config
    location /t {
        proxy_wasm on_phases;
        proxy_wasm on_phases;
        proxy_pass http://unix:$TEST_NGINX_UNIX_SOCKET:/;
    }
--- shutdown
--- ignore_response
--- grep_error_log eval: qr/\[info\] .*? on_(response|log|done).*/
--- grep_error_log_out eval
qr/\[info\] .*? on_done.*
\[info\] .*? on_log.*/
--- no_error_log eval
[
  qr/\[info\] .*? on_response.*/,
  "[error]",
  "[crit]"
]



=== TEST 3: client connection drop - proxy_wasm & dispatch_http_call(), chained filters
Filters executing on a request from the lost connection and that have dispatched
HTTP calls have their on_http_call_response entrypoint invoked.
--- skip_eval: 4: $ENV{TEST_NGINX_USE_HUP} == 1
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- http_config eval
qq{
    server {
        listen unix:$ENV{TEST_NGINX_UNIX_SOCKET};

        location /status/200 {
            return 200;
        }

        location /status/201 {
            return 201;
        }
    }
}
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=unix:$TEST_NGINX_UNIX_SOCKET \
                              path=/status/200';
        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=unix:$TEST_NGINX_UNIX_SOCKET \
                              path=/status/201';
        echo ok;
    }
--- shutdown
--- ignore_response
--- grep_error_log eval: qr/\*\d+.*?on_http_call_response.*/
--- grep_error_log_out eval
qr/\A\*\d+ .*? on_http_call_response \(id: \d+, status: 200[^*].*
\*\d+ .*? on_http_call_response \(id: \d+, status: 201[^*].*\Z/
--- no_error_log
[error]
[crit]
[emerg]



=== TEST 4: client connection drop - proxy_wasm, send_local_response()
Filters running on a request from the lost connection have their response
entrypoints invoked.
--- wasm_modules: hostcalls on_phases
--- config
    location /t {
        proxy_wasm hostcalls;
        proxy_wasm hostcalls 'test=/t/send_local_response/body';
        proxy_wasm hostcalls;
    }
--- shutdown
--- ignore_response
--- grep_error_log eval: qr/\[info\] .*? on_(response|log|done).*/
--- grep_error_log_out eval
qr/.*\["hostcalls" #1\].* on_response_headers.*
.*\["hostcalls" #2\].* on_response_headers.*
.*\["hostcalls" #3\].* on_response_headers.*
.*\["hostcalls" #1\].* on_response_body.*
.*\["hostcalls" #2\].* on_response_body.*
.*\["hostcalls" #3\].* on_response_body.*
.*\["hostcalls" #1\].* on_done.*
.*\["hostcalls" #1\].* on_log.*
.*\["hostcalls" #2\].* on_done.*
.*\["hostcalls" #2\].* on_log.*
.*\["hostcalls" #3\].* on_done.*
.*\["hostcalls" #3\].* on_log.*/
--- no_error_log eval
[
  "[error]",
  "[crit]",
  "[emerg]"
]



=== TEST 5: client connection drop - wasm_call & echo, chained calls
Wasm functions configured to execute on content/header_filter/body_filter phases
are invoked.
--- skip_eval: 4: $ENV{TEST_NGINX_USE_HUP} == 1 or $::nginxV !~ m/debug/
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: ngx_rust_tests
--- config
    location /t {
        wasm_call content ngx_rust_tests log_notice_hello;
        wasm_call header_filter ngx_rust_tests log_notice_hello;
        wasm_call body_filter ngx_rust_tests log_notice_hello;
        echo ok;
    }
--- shutdown
--- ignore_response
--- grep_error_log eval: qr/\[debug\] .*? \*\d wasm ops calling .* in .* phase.*/
--- grep_error_log_out eval
qr/\A.* wasm ops calling "ngx_rust_tests.log_notice_hello" in "content" phase.*
.* wasm ops calling "ngx_rust_tests.log_notice_hello" in "header_filter" phase.*
.* wasm ops calling "ngx_rust_tests.log_notice_hello" in "body_filter" phase.*
.* wasm ops calling "ngx_rust_tests.log_notice_hello" in "body_filter" phase.*\Z/
--- no_error_log
[error]
[crit]
[emerg]



=== TEST 6: client connection drop - wasm_call & proxy_pass, chained calls
Only wasm functions configured to execute in content phase are invoked.
--- skip_eval: 4: $ENV{TEST_NGINX_USE_HUP} == 1 or $::nginxV !~ m/debug/
--- wasm_modules: ngx_rust_tests
--- tcp_listen: $TEST_NGINX_UNIX_SOCKET
--- tcp_reply eval
sub {
    return ["HTTP/1.1 200 OK\r\n",
            "Connection: close\r\n",
            "Content-Length: 0\r\n",
            "\r\n"];
}
--- config
    location /t {
        wasm_call content ngx_rust_tests log_notice_hello;
        wasm_call header_filter ngx_rust_tests log_notice_hello;
        wasm_call body_filter ngx_rust_tests log_notice_hello;
        proxy_pass http://unix:$TEST_NGINX_UNIX_SOCKET:/;
    }
--- shutdown
--- ignore_response
--- grep_error_log eval: qr/\[debug\] .*? \*\d wasm ops calling .* in .* phase.*/
--- grep_error_log_out eval
qr/\[debug\] .* wasm ops calling "ngx_rust_tests.log_notice_hello" in "content" phase.*/
--- no_error_log eval
[
  qr/\[debug\] .* wasm ops calling .* in "(header_filter|body_filter)" phase.*/,
  "[error]",
  "[crit]",
]



=== TEST 7: upstream connection drop - proxy_wasm & proxy_pass, chained filters
Client receives a 502 response, all response phases are invoked.
--- wasm_modules: on_phases
--- tcp_listen: $TEST_NGINX_UNIX_SOCKET
--- tcp_shutdown: 2
--- tcp_reply eval
sub {
    return ["HTTP/1.1 100 OK\r\n",
            "Connection: close\r\n",
            "Content-Length: 0\r\n",
            "\r\n"];
}
--- config
    location /t {
        proxy_wasm on_phases;
        proxy_wasm on_phases;
        proxy_pass http://unix:$TEST_NGINX_UNIX_SOCKET:/;
    }
--- error_code: 502
--- grep_error_log eval: qr/\[info\] .*? on_(response|log|done).*/
--- grep_error_log_out eval
qr/\A.*?\["on_phases" #1\].*? on_response_headers.*
.*?\["on_phases" #2\].*? on_response_headers.*
.*?\["on_phases" #1\].*? on_response_body.*
.*?\["on_phases" #2\].*? on_response_body.*
.*?\["on_phases" #1\].*? on_done.*
.*?\["on_phases" #1\].*? on_log.*
.*?\["on_phases" #2\].*? on_done.*
.*?\["on_phases" #2\].*? on_log.*\Z/
--- no_error_log
[crit]
[emerg]



=== TEST 8: upstream connection drop - proxy_wasm - dispatch_http_call(), on_request_headers
on_http_call_response is invoked along with all response entrypoints.
--- skip_valgrind: 4
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- tcp_listen: $TEST_NGINX_UNIX_SOCKET
--- tcp_shutdown: 2
--- tcp_reply eval
sub {
    return ["HTTP/1.1 100 OK\r\n",
            "Connection: close\r\n",
            "Content-Length: 0\r\n",
            "\r\n"];
}
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=unix:$TEST_NGINX_UNIX_SOCKET';
        proxy_wasm hostcalls;
        echo ok;
    }
--- ignore_response
--- grep_error_log eval: qr/\[info\] .*? on_(http_call_response|response|log|done).*/
--- grep_error_log_out eval
qr/\A.* on_http_call_response \(id: 0, status: , headers: 0, body_bytes: 0, trailers: 0, op: \).*
.* on_response_headers.*
.* on_response_headers.*
.* on_response_body.*
.* on_response_body.*
.* on_response_body.*
.* on_response_body.*
.* on_done.*
.* on_log.*
.* on_done.*
.* on_log.*\Z/
--- no_error_log
[crit]
[emerg]
[stub]



=== TEST 9: upstream connection drop - proxy_wasm - dispatch_http_call(), on_request_body
on_http_call_response is invoked along with all response entrypoints.
--- skip_valgrind: 4
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- tcp_listen: $TEST_NGINX_UNIX_SOCKET
--- tcp_shutdown: 2
--- tcp_reply eval
sub {
    return ["HTTP/1.1 100 OK\r\n",
            "Connection: close\r\n",
            "Content-Length: 0\r\n",
            "\r\n"];
}
--- config
    location /t {
        proxy_wasm hostcalls 'on=request_body \
                              test=/t/dispatch_http_call \
                              host=unix:$TEST_NGINX_UNIX_SOCKET';
        proxy_wasm hostcalls;
        echo ok;
    }
--- request
POST /t
Hello world
--- ignore_response
--- grep_error_log eval: qr/\[info\] .*? on_(http_call_response|response|log|done).*/
--- grep_error_log_out eval
qr/\A.* on_http_call_response \(id: 0, status: , headers: 0, body_bytes: 0, trailers: 0, op: \).*
.* on_response_headers.*
.* on_response_headers.*
.* on_response_body.*
.* on_response_body.*
.* on_response_body.*
.* on_response_body.*
.* on_done.*
.* on_log.*
.* on_done.*
.* on_log.*\Z/
--- no_error_log
[crit]
[emerg]
[stub]
