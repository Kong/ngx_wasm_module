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
resp Transfer-Encoding: chunked.*
resp Connection: close.*
resp Hello: world.*
resp Welcome: wasm.*/
--- no_error_log
[error]
[crit]



=== TEST 2: proxy_wasm - set_http_response_headers() sets special response headers
--- load_nginx_modules: ngx_http_headers_more_filter_module
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
resp Content-Type: text\/none.*
resp Transfer-Encoding: chunked.*
resp Connection: close.*
resp Server: proxy-wasm.*/
--- no_error_log
[error]
[crit]



=== TEST 3: proxy_wasm - set_http_response_headers() cannot set invalid Connection header
should log an error but not produce a trap
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



=== TEST 4: proxy_wasm - set_http_response_headers() x on_phases
should log an error (but no trap) when headers are sent
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'on=request_headers \
                              test=/t/set_response_headers \
                              value=From:request_headers';

        proxy_wasm hostcalls 'on=response_headers \
                              test=/t/set_response_headers \
                              value=From:response_headers';

        proxy_wasm hostcalls 'on=response_body \
                              test=/t/set_response_headers \
                              value=From:response_body';

        proxy_wasm hostcalls 'on=log \
                              test=/t/set_response_headers \
                              value=From:log';
        return 200;
    }
--- response_headers
From: response_headers
--- ignore_response_body
--- grep_error_log eval: qr/(\[error\]|testing in) .*/
--- grep_error_log_out eval
qr/testing in "RequestHeaders".*
testing in "ResponseHeaders".*
testing in "ResponseBody".*
\[error\] .*? \[wasm\] cannot set response headers: headers already sent.*
testing in "Log".*
\[error\] .*? \[wasm\] cannot set response headers: headers already sent/
--- no_error_log
[warn]
[crit]
