# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasmX;

plan_tests(5);
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
        proxy_wasm hostcalls 'on=response_headers \
                              test=/t/set_response_headers';
        proxy_wasm hostcalls 'on=response_headers \
                              test=/t/log/response_headers';
    }
--- raw_response_headers_like
HTTP\/.*? \d+ .*?
Transfer-Encoding: chunked\r
Connection: close\r
Hello: world\r
Welcome: wasm\r
--- grep_error_log eval: qr/(testing in|resp|on_response_headers,) .*/
--- grep_error_log_out eval
qr/on_response_headers, 5 headers.*
testing in "ResponseHeaders".*
on_response_headers, 4 headers.*
testing in "ResponseHeaders".*
resp :status: 200.*
resp Transfer-Encoding: chunked.*
resp Connection: close.*
resp Hello: world.*
resp Welcome: wasm.*/
--- no_error_log
[error]
[crit]



=== TEST 2: proxy_wasm - set_http_response_headers() sets special response headers
--- valgrind
--- wasm_modules: ngx_rust_tests hostcalls
--- config
    location /t {
        wasm_call content ngx_rust_tests say_nothing;

        proxy_wasm hostcalls 'on=response_headers \
                              test=/t/set_response_headers \
                              value=Content-Type:text/none+Server:proxy-wasm';
        proxy_wasm hostcalls 'on=response_headers \
                              test=/t/log/response_headers';
    }
--- raw_response_headers_like
HTTP\/.*? \d+ .*?
Content-Type: text\/none\r
Transfer-Encoding: chunked\r
Connection: close\r
Server: proxy-wasm\r
--- grep_error_log eval: qr/(testing in|resp|on_response_headers,) .*/
--- grep_error_log_out eval
qr/on_response_headers, 4 headers.*
testing in "ResponseHeaders".*
on_response_headers, 4 headers.*
testing in "ResponseHeaders".*
resp :status: 200.*
resp Content-Type: text\/none.*
resp Transfer-Encoding: chunked.*
resp Connection: close.*
resp Server: proxy-wasm.*/
--- no_error_log
[error]
[crit]



=== TEST 3: proxy_wasm - set_http_response_headers() cannot set invalid Connection header
should log an error but not produce a trap
--- valgrind
--- wasm_modules: ngx_rust_tests hostcalls
--- config
    location /t {
        wasm_call content ngx_rust_tests say_nothing;

        proxy_wasm hostcalls 'on=response_headers \
                              test=/t/set_response_headers \
                              value=Connection:closed';
    }
--- response_body
--- error_log eval
qr/\[error\] .*? \[wasm\] attempt to set invalid Connection response header: "closed"/
--- no_error_log
[crit]
[alert]



=== TEST 4: proxy_wasm - set_http_response_headers() sets ':status' pseudo-header on request_headers
--- wasm_modules: ngx_rust_tests hostcalls
--- config
    location /t {
        wasm_call content ngx_rust_tests say_nothing;

        proxy_wasm hostcalls 'on=request_headers \
                              test=/t/set_response_headers \
                              value=:status:201';
    }
--- error_code: 201
--- response_body
--- raw_response_headers_like
HTTP\/.*? 201 .*?
Connection: close\r
Content-Length: 0\r
--- no_error_log
[error]
[crit]



=== TEST 5: proxy_wasm - set_http_response_headers() sets ':status' pseudo-header on response_headers
--- wasm_modules: ngx_rust_tests hostcalls
--- config
    location /t {
        wasm_call content ngx_rust_tests say_nothing;

        proxy_wasm hostcalls 'on=response_headers \
                              test=/t/set_response_headers \
                              value=:status:201';
    }
--- error_code: 201
--- response_body
--- raw_response_headers_like
HTTP\/.*? 201 .*?
Transfer-Encoding: chunked\r
Connection: close\r
--- no_error_log
[error]
[crit]



=== TEST 6: proxy_wasm - set_http_response_headers() ignores out-of-range ':status' values
--- wasm_modules: ngx_rust_tests hostcalls
--- config
    location /t {
        wasm_call content ngx_rust_tests say_nothing;

        proxy_wasm hostcalls 'on=response_headers \
                              test=/t/set_response_headers \
                              value=:status:999999';
    }
--- error_code: 200
--- response_body
--- raw_response_headers_like
HTTP\/.*? 200 .*?
Transfer-Encoding: chunked\r
Connection: close\r
--- error_log eval
qr/\[warn\] .*? ignoring attempt to set invalid response status: "999999"/
--- no_error_log
[error]



=== TEST 7: proxy_wasm - set_http_response_headers() ignores invalid ':status' values
--- wasm_modules: ngx_rust_tests hostcalls
--- config
    location /t {
        wasm_call content ngx_rust_tests say_nothing;

        proxy_wasm hostcalls 'on=response_headers \
                              test=/t/set_response_headers \
                              value=:status:xyz';
    }
--- error_code: 200
--- response_body
--- raw_response_headers_like
HTTP\/.*? 200 .*?
Transfer-Encoding: chunked\r
Connection: close\r
--- error_log eval
qr/\[warn\] .*? ignoring attempt to set invalid response status: "xyz"/
--- no_error_log
[error]



=== TEST 8: proxy_wasm - set_http_response_headers() ignores empty ':status' values
--- wasm_modules: ngx_rust_tests hostcalls
--- config
    location /t {
        wasm_call content ngx_rust_tests say_nothing;

        proxy_wasm hostcalls 'on=response_headers \
                              test=/t/set_response_headers \
                              value=:status:';
    }
--- error_code: 200
--- response_body
--- raw_response_headers_like
HTTP\/.*? 200 .*?
Transfer-Encoding: chunked\r
Connection: close\r
--- error_log eval
qr/\[warn\] .*? ignoring attempt to set invalid response status: ""/
--- no_error_log
[error]
