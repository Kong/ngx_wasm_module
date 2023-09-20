# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

skip_valgrind();

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
        proxy_wasm hostcalls 'test=/t/send_local_response/status/204';
        echo fail;
    }
--- error_code: 204
--- response_body
--- raw_response_headers_like eval
"HTTP\/1\.1 204 No Content\r
Connection: close\r
Content-Length: 0\r
Server: (nginx|openresty).*?\r
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
        proxy_wasm hostcalls 'test=/t/send_local_response/status/300';
        echo fail;
    }
--- error_code: 300
--- response_body
--- no_error_log
[error]
[crit]
[alert]
[stub]



=== TEST 3: proxy_wasm - send_local_response() set status code (bad argument)
should produce error page content from a panic, not from echo
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/send_local_response/status/1000';
        echo fail;
    }
--- error_code: 500
--- response_body_like: 500 Internal Server Error
--- error_log eval
[
    qr/\[crit\] .*? panicked at/,
    qr/unexpected status: 2/,
]
--- no_error_log
[alert]
[emerg]



=== TEST 4: proxy_wasm - send_local_response() set status code (bad argument)
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls;
        echo failed;
    }
--- request eval
["GET /t/send_local_response/status/1000", "GET /t/send_local_response/status/204"]
--- error_code eval
[500, 204]
--- response_body eval
[qr/500 Internal Server Error/, qr//]
--- no_error_log
[alert]



=== TEST 5: proxy_wasm - send_local_response() default response headers
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/send_local_response/status/204';
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
qr/\[info\] .*? on_response_headers, 4 headers/
--- no_error_log
[error]
[crit]
[alert]



=== TEST 6: proxy_wasm - send_local_response() set headers, no body
should inject headers a produced response, not from echo
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/send_local_response/headers';
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
--- grep_error_log eval: qr/\[hostcalls\] .*/
--- grep_error_log_out eval
qr/.*? on_request_headers, 2 headers, .*
.*? testing in "RequestHeaders", .*
.*? on_response_headers, 5 headers, .*
.*? on_response_body, 0 bytes, eof: true, .*
.*? on_done.*
.*? on_log.*/
--- no_error_log
[error]
[crit]



=== TEST 7: proxy_wasm - send_local_response() response headers with extra headers
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/send_local_response/headers';
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
qr/\[info\] .*? on_response_headers, 5 headers/
--- no_error_log
[error]
[crit]



=== TEST 8: proxy_wasm - send_local_response() set body
should produce a response body, not from echo
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/send_local_response/body';
        echo fail;
    }
--- response_body
Hello world
--- no_error_log
[error]
[crit]
[alert]
[stub]



=== TEST 9: proxy_wasm - send_local_response() response headers with body
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/send_local_response/body';
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
qr/\[info\] .*? on_response_headers, 5 headers/
--- no_error_log
[error]
[crit]
[alert]



=== TEST 10: proxy_wasm - send_local_response() set special response headers
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/send_local_response/set_special_headers';
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
--- grep_error_log eval: qr/\[hostcalls\] .*/
--- grep_error_log_out eval
qr/.*? on_request_headers, 2 headers, .*
.*? testing in "RequestHeaders", .*
.*? on_response_headers, 16 headers, .*
.*? on_response_body, 0 bytes, eof: true, .*
.*? on_done.*
.*? on_log.*/
--- no_error_log
[error]
[crit]
[alert]



=== TEST 11: proxy_wasm - send_local_response() set escaped header names
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/send_local_response/set_headers_escaping';
        echo fail;
    }
--- response_headers
Escape-Colon%3A: value
Escape-Parenthesis%28%29: value
Escape-Quote%22: value
Escape-Comps%3C%3E: value
Escape-Equal%3D: value
--- ignore_response_body



=== TEST 12: proxy_wasm - send_local_response() from on_request_headers
should produce a response
should invoke on_log
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'on=request_headers \
                              test=/t/send_local_response/body';
        echo fail;
    }
--- response_headers
Content-Type: text/plain
--- response_body
Hello world
--- error_log eval
[
    qr/testing in "RequestHeaders"/,
    qr/\[info\] .*? on_log/
]
--- no_error_log
[error]



=== TEST 13: proxy_wasm - send_local_response() from on_http_response_headers (with content)
should produce a trap
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        echo ok;
        proxy_wasm hostcalls 'on=response_headers \
                              test=/t/send_local_response/body';
    }
--- error_code: 500
--- response_body_like: 500 Internal Server Error
--- error_log eval
qr/testing in "ResponseHeaders"/
--- grep_error_log eval: qr/(\[error\]|host trap|\[.*?failed resuming).*/
--- grep_error_log_out eval
qr/.*?host trap \(bad usage\): response already sent.*
\[info\] .*? filter chain failed resuming: previous error \(instance trapped\)/
--- no_error_log
[alert]
[stub]



=== TEST 14: proxy_wasm - send_local_response() from on_log
should produce a trap in log phase
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        echo ok;
        proxy_wasm hostcalls 'on=log \
                              test=/t/send_local_response/body';
    }
--- response_body
ok
--- error_log eval
qr/testing in "Log"/
--- grep_error_log eval: qr/(\[(error|crit)\]|host trap).*/
--- grep_error_log_out eval
qr/host trap \(bad usage\): response already sent.*/
--- no_error_log
[emerg]
failed resuming



=== TEST 15: proxy_wasm - send_local_response() invoked when a response is already stashed
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls ngx_rust_tests
--- config
    location /t {
        wasm_call rewrite ngx_rust_tests local_response;
        proxy_wasm hostcalls 'on=request_headers \
                              test=/t/send_local_response/body';
        echo fail;
    }
--- error_code: 500
--- response_body_like: 500 Internal Server Error
--- error_log
host trap (bad usage): local response already stashed
--- no_error_log
[crit]
[emerg]
[alert]



=== TEST 16: proxy_wasm - send_local_response() invoked as a subrequest before content
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
[stub]



=== TEST 17: proxy_wasm - send_local_response() invoked as a subrequest after content
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
[stub]



=== TEST 18: proxy_wasm - send_local_response() on_request_headers, filter chain execution sanity
should execute all filters request and response steps
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls;
        proxy_wasm hostcalls 'test=/t/send_local_response/body';
        proxy_wasm hostcalls;
    }
--- response_body
Hello world
--- grep_error_log eval: qr/\[info\] .*? on_(request|response|log).*/
--- grep_error_log_out eval
qr/\A.*? on_request_headers, \d+ headers.*
.*? on_request_headers, \d+ headers.*
.*? on_response_headers, \d+ headers.*
.*? on_response_headers, \d+ headers.*
.*? on_response_headers, \d+ headers.*
.*? on_response_body, \d+ bytes, eof: true.*
.*? on_response_body, \d+ bytes, eof: true.*
.*? on_response_body, \d+ bytes, eof: true.*
.*? on_log.*
.*? on_log.*
.*? on_log/
--- no_error_log
[error]
[crit]
[alert]



=== TEST 19: proxy_wasm - send_local_response() invoked twice in chained filters
should interrupt the current phase, preventing "response already stashed"
should still run all response phases
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/send_local_response/body';
        proxy_wasm hostcalls 'test=/t/send_local_response/body';
    }
--- response_body
Hello world
--- grep_error_log eval: qr/\[hostcalls\] .*/
--- grep_error_log_out eval
qr/.*? on_request_headers, \d+ headers.*
.*? testing in "RequestHeaders".*
.*? on_response_headers, \d+ headers.*
.*? on_response_headers, \d+ headers.*
.*? on_response_body, \d+ bytes, eof: true.*
.*? on_response_body, \d+ bytes, eof: true.*
.*? on_done.*
.*? on_log.*
.*? on_done.*
.*? on_log.*/
--- no_error_log
[error]
[crit]
[alert]



=== TEST 20: proxy_wasm - send_local_response() in chained filters as a subrequest
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
--- grep_error_log eval: qr/\[hostcalls\] .*/
--- grep_error_log_out eval
qr/.*? on_request_headers, \d+ headers.*
.*? testing in "RequestHeaders".*
.*? on_response_headers, \d+ headers.*
.*? on_response_headers, \d+ headers.*
.*? on_response_body, \d+ bytes, eof: true.*
.*? on_response_body, \d+ bytes, eof: true.*/
--- no_error_log
\s+on_log\s+
[error]
[crit]
