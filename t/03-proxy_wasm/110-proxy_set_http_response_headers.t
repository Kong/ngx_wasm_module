# vim:set ft= ts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

skip_valgrind();

plan tests => repeat_each() * (blocks() * 5);

run_tests();

__DATA__

=== TEST 1: proxy_wasm - set_http_response_headers() sets response headers
--- load_nginx_modules: ngx_http_headers_more_filter_module
--- wasm_modules: ngx_rust_tests hostcalls
--- config
    location /t {
        wasm_call content ngx_rust_tests say_nothing;

        more_set_headers 'Server: more_headers';
        more_set_headers 'Hello: here';
        proxy_wasm hostcalls 'on_phase=http_response_headers test_case=/t/set_http_response_headers';
        proxy_wasm hostcalls 'on_phase=http_response_headers test_case=/t/log/response_headers';
    }
--- raw_response_headers_like
HTTP\/.*? \d+ .*?
Connection: close\r
Hello: world\r
Welcome: wasm\r
--- grep_error_log eval: qr/\[wasm\] .*?(#\d+ on_resp|resp\s).*?(?=\s+<)/
--- grep_error_log_out eval
qr/\[wasm\] .*? on_response_headers, 5 headers
\[wasm\] .*? on_response_headers, 3 headers
\[wasm\] resp Connection: close
\[wasm\] resp Hello: world
\[wasm\] resp Welcome: wasm/
--- no_error_log
[error]
[crit]



=== TEST 2: proxy_wasm - set_http_response_headers() sets special response headers
--- load_nginx_modules: ngx_http_headers_more_filter_module
--- wasm_modules: ngx_rust_tests hostcalls
--- config
    location /t {
        wasm_call content ngx_rust_tests say_nothing;

        proxy_wasm hostcalls 'on_phase=http_response_headers test_case=/t/set_http_response_headers';
        proxy_wasm hostcalls 'on_phase=http_response_headers test_case=/t/log/response_headers';
    }
--- more_headers
pwm-set-resp-headers: Content-Type=text/none Server=proxy-wasm
--- raw_response_headers_like
HTTP\/.*? \d+ .*?
Content-Type: text\/none\r
Connection: close\r
Server: proxy-wasm\r
--- grep_error_log eval: qr/\[wasm\] .*?(#\d+ on_resp|resp\s).*?(?=\s+<)/
--- grep_error_log_out eval
qr/\[wasm\] .*? on_response_headers, 4 headers
\[wasm\] .*? on_response_headers, 3 headers
\[wasm\] resp Content-Type: text\/none
\[wasm\] resp Connection: close
\[wasm\] resp Server: proxy-wasm/
--- no_error_log
[error]
[crit]



=== TEST 3: proxy_wasm - set_http_response_headers() cannot set invalid Connection header
--- wasm_modules: ngx_rust_tests hostcalls
--- config
    location /t {
        wasm_call content ngx_rust_tests say_nothing;

        proxy_wasm hostcalls 'on_phase=http_response_headers test_case=/t/set_http_response_headers';
    }
--- more_headers
pwm-set-resp-headers: Connection=closed
--- error_log eval
[
    qr/\[error\] .*? \[wasm\] attempt to set invalid Connection response header: "closed"/,
    qr/\[crit\] .*? panicked at 'unexpected status: 10'/,
]
--- no_error_log
[emerg]
[alert]
