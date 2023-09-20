# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

skip_valgrind();

if ($ENV{TEST_NGINX_USE_HUP}) {
    plan(skip_all => "unavailable in HUP mode");

} else {
    plan tests => repeat_each() * (blocks() * 4);
}

run_tests();

__DATA__

=== TEST 1: upstream connection abort - wasm_call
Calls on aborted upstream connections execute response phases on the produced
HTTP 502 response.
--- skip_no_debug: 4
--- wasm_modules: ngx_rust_tests
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
        wasm_call header_filter ngx_rust_tests log_notice_hello;
        wasm_call body_filter ngx_rust_tests log_notice_hello;
        proxy_pass http://unix:$TEST_NGINX_UNIX_SOCKET:/;
    }
--- error_code: 502
--- ignore_response
--- grep_error_log eval: qr/(upstream prematurely closed|(wasm ops calling .*? in .*? phase))/
--- grep_error_log_out eval
qr/\Aupstream prematurely closed
wasm ops calling "ngx_rust_tests\.log_notice_hello" in "header_filter" phase
wasm ops calling "ngx_rust_tests\.log_notice_hello" in "body_filter" phase\Z/
--- no_error_log
[crit]
[emerg]
[alert]



=== TEST 2: upstream connection abort - chained filters
Filters on aborted upstream connections execute response phases on the produced
HTTP 502 response.
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
--- ignore_response
--- grep_error_log eval: qr/\[(error|info)\] .*? ((upstream prematurely closed)|on_request_headers|on_response_headers|on_log|on_done|on_http_call_response.*)/
--- grep_error_log_out eval
qr/\A\[info\] .*? on_request_headers
\[info\] .*? on_request_headers
\[error\] .*? upstream prematurely closed
\[info\] .*? on_response_headers
\[info\] .*? on_response_headers
\[info\] .*? on_done
\[info\] .*? on_log
\[info\] .*? on_done
\[info\] .*? on_log\Z/
--- no_error_log
[crit]
[emerg]
[alert]
