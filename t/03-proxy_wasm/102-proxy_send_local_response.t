# vim:set ft= ts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

skip_valgrind();

no_long_string();

plan tests => repeat_each() * (blocks() * 6);

run_tests();

__DATA__

=== TEST 1: proxy_wasm - send_local_response() set status code
should produce response with valid code, not from echo
should not contain any extra header
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test_case=/t/send_local_response/status/204';
        echo fail;
    }
--- error_code: 204
--- response_body
--- raw_response_headers_like eval
"HTTP\/1\.1 204 No Content\r
Connection: close\r
Content-Length: 0\r
Server: nginx.*?\r
Date: .*? GMT\r"
--- no_error_log
[warn]
[error]
[crit]



=== TEST 2: proxy_wasm - send_local_response() set status code (special response, HTTP >= 300)
should produce response with valid code
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test_case=/t/send_local_response/status/300';
        echo fail;
    }
--- error_code: 300
--- response_body
--- no_error_log
[error]
[crit]
[alert]
stub



=== TEST 3: proxy_wasm - send_local_response() set status code (bad argument)
should produce error page content from a panic, not from echo
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test_case=/t/send_local_response/status/1000';
        echo fail;
    }
--- error_code: 500
--- response_body eval
qr/500 Internal Server Error/
--- error_log eval
[
    qr/\[crit\] .*? panicked at 'unexpected status: 2'/,
    qr/\[error\] .*? \[wasm\] (?:wasm trap\: )?unreachable/,
    qr/\[crit\] .*? \[wasm\] instance trapped: proxy_wasm failed to resume execution in "header_filter"/,
]
--- no_error_log
[alert]



=== TEST 4: proxy_wasm - send_local_response() default response headers
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test_case=/t/send_local_response/status/204';
    }
--- error_code: 204
--- ignore_response_body
--- raw_response_headers_like
HTTP\/1\.1 .*?\r
Connection: close\r
Content-Length: 0\r
Server: [\S\s]+\r
Date: [\S\s]+\r
--- error_log eval
qr/\[debug\] .*? \[wasm\] #\d+ on_response_headers, 4 headers/
--- no_error_log
[error]
[crit]
[alert]



=== TEST 5: proxy_wasm - send_local_response() set headers, no body
should inject headers a produced response, not from echo
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test_case=/t/send_local_response/headers';
        echo fail;
    }
--- raw_response_headers_like
HTTP\/.*? \d+ .*?
Connection: close\r
Powered-By: proxy-wasm\r
Content-Length: 0\r
Server: [\S\s]+\r
Date: .*? GMT\r
--- response_body
--- grep_error_log eval: qr/\[debug\] .*? \[wasm\] .*/
--- grep_error_log_out eval
qr/\[debug\] .*? \[wasm\] #\d+ on_request_headers, 2 headers
\[debug\] .*? \[wasm\] #\d+ on_response_headers, 5 headers
\[debug\] .*? \[wasm\] #\d+ on_response_body, 0 bytes, end_of_stream true
\[debug\] .*? \[wasm\] #\d+ on_log/
--- no_error_log
[error]
[crit]



=== TEST 6: proxy_wasm - send_local_response() response headers with extra headers
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test_case=/t/send_local_response/headers';
    }
--- response_body
--- raw_response_headers_like
HTTP\/1\.1 .*?\r
Connection: close\r
Powered-By: proxy-wasm\r
Content-Length: 0\r
Server: [\S\s]+\r
Date: [\S\s]+\r
--- error_log eval
qr/\[debug\] .*? \[wasm\] #\d+ on_response_headers, 5 headers/
--- no_error_log
[error]
[crit]



=== TEST 7: proxy_wasm - send_local_response() set body
should produce a response body, not from echo
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test_case=/t/send_local_response/body';
        echo fail;
    }
--- response_body
Hello world
--- no_error_log
[error]
[crit]
[alert]
stub



=== TEST 8: proxy_wasm - send_local_response() response headers with body
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test_case=/t/send_local_response/body';
    }
--- ignore_response_body
--- raw_response_headers_like
HTTP\/1\.1 .*?\r
Content-Type: text\/plain\r
Connection: close\r
Content-Length: 12\r
Server: [\S\s]+\r
Date: [\S\s]+\r
--- error_log eval
qr/\[debug\] .*? \[wasm\] #\d+ on_response_headers, 5 headers/
--- no_error_log
[error]
[crit]
[alert]



=== TEST 9: proxy_wasm - send_local_response() set special response headers
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test_case=/t/send_local_response/set_special_headers';
        echo fail;
    }
--- response_headers
--- raw_response_headers_like
HTTP\/.*? \d+ .*?
Content-Type: text\/plain; charset=UTF-8\r
Connection: close\r
Server: proxy-wasm\r
Date: Wed, 22 Oct 2020 07:28:00 GMT\r
Content-Length: 0\r
Content-Encoding: gzip\r
Location: \/index.html\r
Refresh: 5; url=http:\/\/www\.w3\.org\/index\.html\r
Last-Modified: Tue, 15 Nov 1994 12:45:26 GMT\r
Content-Range: bytes 21010-47021\/47022\r
Accept-Ranges: bytes\r
WWW-Authenticate: Basic\r
Expires: Thu, 01 Dec 1994 16:00:00 GMT\r
E-Tag: 377060cd8c284d8af7ad3082f20958d2\r
Cache-Control: no-cache\r
Link: <\/feed>; rel="alternate"\r
--- ignore_response_body
--- grep_error_log eval: qr/\[debug\] .*? \[wasm\] .*/
--- grep_error_log_out eval
qr/\[debug\] .*? \[wasm\] #\d+ on_request_headers, 2 headers
\[debug\] .*? \[wasm\] #\d+ on_response_headers, 16 headers
\[debug\] .*? \[wasm\] #\d+ on_response_body, 0 bytes, end_of_stream true
\[debug\] .*? \[wasm\] #\d+ on_log/
--- no_error_log
[error]
[crit]
[alert]



=== TEST 10: proxy_wasm - send_local_response() set escaped header names
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test_case=/t/send_local_response/set_headers_escaping';
        echo fail;
    }
--- response_headers
Escape-Colon%3A: value
Escape-Parenthesis%28%29: value
Escape-Quote%22: value
Escape-Comps%3C%3E: value
Escape-Equal%3D: value
--- ignore_response_body



=== TEST 11: proxy_wasm - send_local_response() from on_http_request_headers
should produce a response
should invoke on_log
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'on_phase=http_request_headers \
                              test_case=/t/send_local_response/body';
        echo fail;
    }
--- response_headers
Content-Type: text/plain
--- response_body
Hello world
--- error_log eval
[
    qr/\[wasm\] #\d+ entering "HttpRequestHeaders"/,
    qr/\[debug\] .*? \[wasm\] #\d+ on_log/
]
--- no_error_log
[error]



=== TEST 12: proxy_wasm - send_local_response() from on_http_response_headers (with content)
should produce a trap
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        echo ok;
        proxy_wasm hostcalls 'on_phase=http_response_headers \
                              test_case=/t/send_local_response/body';
    }
--- error_code: 500
--- response_body_like: 500 Internal Server Error
--- error_log eval
qr/\[wasm\] #\d+ entering "HttpResponseHeaders"/
--- grep_error_log eval: qr/\[(error|crit)\] .*?(?=(\s+<|,|\n))/
--- grep_error_log_out eval
qr/\[error\] .*? \[wasm\] response already sent.*?
\[crit\] .*? \[wasm\] instance trapped: proxy_wasm failed to resume execution in "body_filter" phase
\[crit\] .*? \[wasm\] instance trapped: proxy_wasm failed to resume execution in "log" phase/
--- no_error_log
[alert]
stub



=== TEST 13: proxy_wasm - send_local_response() from on_log
should produce a trap in log phase
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        echo ok;
        proxy_wasm hostcalls 'on_phase=log \
                              test_case=/t/send_local_response/body';
    }
--- response_body
ok
--- error_log eval
qr/\[wasm\] #\d+ entering "Log"/
--- grep_error_log eval: qr/\[(error|crit)\] .*?(?=(\s+<|,|\n))/
--- grep_error_log_out eval
qr/\[error\] .*? \[wasm\] response already sent/
--- no_error_log
[crit]
[alert]



=== TEST 14: proxy_wasm - send_local_response() invoked as a subrequest before content
should produce content from the subrequest, then from echo
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t/send_local_response/body {
        proxy_wasm hostcalls;
    }

    location /t {
        echo_subrequest GET /t/send_local_response/body;
        echo ok;
    }
--- response_body
Hello world
ok
--- no_error_log
[error]
[crit]
[alert]
stub



=== TEST 15: proxy_wasm - send_local_response() invoked as a subrequest after content
should produce content from echo, then the subrequest
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t/send_local_response/body {
        proxy_wasm hostcalls;
    }

    location /t {
        echo ok;
        echo_subrequest GET /t/send_local_response/body;
    }
--- request
GET /t
--- response_body
ok
Hello world
--- no_error_log
[error]
[crit]
[alert]
stub



=== TEST 16: proxy_wasm - send_local_response() in chained filters
should interrupt the current phase, preventing "response already stashed"
should still run all response phases
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test_case=/t/send_local_response/body';
        proxy_wasm hostcalls 'test_case=/t/send_local_response/body';
    }
--- response_body
Hello world
--- grep_error_log eval: qr/\[debug\] .*? \[wasm\] .*/
--- grep_error_log_out eval
qr/\[debug\] .*? \[wasm\] #\d+ on_request_headers, \d+ headers
\[debug\] .*? \[wasm\] #\d+ on_response_headers, \d+ headers
\[debug\] .*? \[wasm\] #\d+ on_response_headers, \d+ headers
\[debug\] .*? \[wasm\] #\d+ on_response_body, \d+ bytes, end_of_stream true
\[debug\] .*? \[wasm\] #\d+ on_response_body, \d+ bytes, end_of_stream true
\[debug\] .*? \[wasm\] #\d+ on_log
\[debug\] .*? \[wasm\] #\d+ on_log/
--- no_error_log
[error]
[crit]
[alert]



=== TEST 17: proxy_wasm - send_local_response() in chained filters as a subrequest
should interrupt the current phase, preventing "response already stashed"
should still run all response phases
should not have a log phase (subrequest)
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t/send_local_response/body {
        proxy_wasm hostcalls;
        proxy_wasm hostcalls;
    }

    location /t {
        echo ok;
        echo_subrequest GET /t/send_local_response/body;
    }
--- response_body
ok
Hello world
--- grep_error_log eval: qr/\[debug\] .*? \[wasm\] .*/
--- grep_error_log_out eval
qr/\[debug\] .*? \[wasm\] #\d+ on_request_headers, \d+ headers
\[debug\] .*? \[wasm\] #\d+ on_response_headers, \d+ headers
\[debug\] .*? \[wasm\] #\d+ on_response_headers, \d+ headers
\[debug\] .*? \[wasm\] #\d+ on_response_body, \d+ bytes, end_of_stream true
\[debug\] .*? \[wasm\] #\d+ on_response_body, \d+ bytes, end_of_stream true/
--- no_error_log
on_log
[error]
[crit]
