# vim:set ft= ts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

skip_valgrind();

plan tests => repeat_each() * (blocks() * 5);

run_tests();

__DATA__

=== TEST 1: proxy_wasm - set_http_response_headers() sets response headers
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test_case=/t/set_http_response_headers';
        proxy_wasm hostcalls 'test_case=/t/log/response_headers';
        return 200;
    }
--- raw_response_headers_like
HTTP\/.*? \d+ .*?
Content-Type: text\/plain\r
Content-Length: 0\r
Connection: close\r
Hello: world\r
Welcome: wasm\r
Server: .*?\r
Date: .*? GMT\r
--- grep_error_log eval: qr/\[wasm\] .*?(#\d+ on_|resp\s).*?(?=\s+<)/
--- grep_error_log_out eval
qr/\[wasm\] .*? on_request_headers, 2 headers
\[wasm\] .*? on_request_headers, 2 headers
\[wasm\] resp Connection: close
\[wasm\] resp Hello: world
\[wasm\] resp Welcome: wasm
\[wasm\] .*? on_response_headers, 7 headers
\[wasm\] .*? on_response_headers, 7 headers
\[wasm\] .*? on_log
\[wasm\] .*? on_log/
--- no_error_log
[error]
[crit]



=== TEST 2: proxy_wasm - set_http_response_headers() sets special response headers
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test_case=/t/set_http_response_headers/special';
        proxy_wasm hostcalls 'test_case=/t/log/response_headers';
        return 200;
    }
--- raw_response_headers_like
HTTP\/.*? \d+ .*?
Content-Type: text\/none\r
Content-Length: 0\r
Connection: close\r
Server: proxy-wasm\r
Date: .*? GMT\r
--- grep_error_log eval: qr/\[wasm\] .*?(#\d+ on_|resp\s).*?(?=\s+<)/
--- grep_error_log_out eval
qr/\[wasm\] .*? on_request_headers, 2 headers
\[wasm\] .*? on_request_headers, 2 headers
\[wasm\] resp Content-Type: text\/none
\[wasm\] resp Connection: close
\[wasm\] resp Server: proxy-wasm
\[wasm\] .*? on_response_headers, 5 headers
\[wasm\] .*? on_response_headers, 5 headers
\[wasm\] .*? on_log
\[wasm\] .*? on_log/
--- no_error_log
[error]
[crit]
