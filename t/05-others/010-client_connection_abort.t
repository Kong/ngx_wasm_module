# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

skip_hup();

plan_tests(4);
run_tests();

__DATA__

=== TEST 1: client connection abort - wasm_call
Nginx response filters are not invoked on the produced HTTP 499 response.
--- skip_no_debug
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
--- grep_error_log eval: qr/(client prematurely closed|(wasm ops calling .*? in .*? phase))/
--- grep_error_log_out eval
qr/wasm ops calling "ngx_rust_tests\.log_notice_hello" in "content" phase
client prematurely closed\Z/
--- no_error_log
header_filter
body_filter
[error]



=== TEST 2: client connection abort - chained filters
Filters on aborted client connections do not execute response phases.
--- wasm_modules: on_phases
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
        proxy_wasm on_phases;
        proxy_wasm on_phases;
        proxy_pass http://unix:$TEST_NGINX_UNIX_SOCKET:/;
    }
--- shutdown
--- ignore_response
--- grep_error_log eval: qr/\[info\] .*? ((client prematurely closed)|on_request_headers|on_response_headers|on_log|on_done|on_http_call_response.*)/
--- grep_error_log_out eval
qr/\A\[info\] .*? on_request_headers
\[info\] .*? on_request_headers
\[info\] .*? reported that client prematurely closed
\[info\] .*? on_done
\[info\] .*? on_log
\[info\] .*? on_done
\[info\] .*? on_log\Z/
--- no_error_log
on_response
[error]
[crit]



=== TEST 3: client connection abort - chained filters with HTTP dispatch
Filters with pending dispatch calls on aborted client connections await for
response.
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- http_config eval
qq{
    server {
        listen unix:$ENV{TEST_NGINX_UNIX_SOCKET};

        location / {
            return 200;
        }

        location /sleep {
            echo_sleep 0.3;
            echo_status 201;
        }
    }
}
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=unix:$TEST_NGINX_UNIX_SOCKET \
                              path=/';
        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=unix:$TEST_NGINX_UNIX_SOCKET \
                              path=/sleep';
        proxy_pass http://unix:$TEST_NGINX_UNIX_SOCKET:/;
    }
--- shutdown
--- ignore_response
--- grep_error_log eval: qr/\[info\] .*? ((client prematurely closed)|on_request_headers|on_response_headers|on_log|on_done|on_http_call_response.*)/
--- grep_error_log_out eval
qr/\A\[info\] .*? on_request_headers
\[info\] .*? on_http_call_response \(id: \d+, status: 200[^*].*
\[info\] .*? on_request_headers
\[info\] .*? on_http_call_response \(id: \d+, status: 201[^*].*
\[info\] .*? reported that client prematurely closed
\[info\] .*? on_done
\[info\] .*? on_log
\[info\] .*? on_done
\[info\] .*? on_log\Z/
--- no_error_log
[error]
[crit]
[emerg]
