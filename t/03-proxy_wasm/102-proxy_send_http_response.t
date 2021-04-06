# vim:set ft= ts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

skip_valgrind();

plan tests => repeat_each() * (blocks() * 6);

run_tests();

__DATA__

=== TEST 1: send_http_response() set status code
should produce response with valid code
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls;
        echo fail;
    }
--- request
GET /t/send_http_response/status/204
--- error_code: 204
--- response_body
--- no_error_log
[warn]
[error]
[alert]
[emerg]



=== TEST 2: send_http_response() set status code (bad argument)
should produce error page content from a panic, not from echo
--- TODO: handle "already borrowed mut" panic in log phase
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls;
        echo fail;
    }
--- request
GET /t/send_http_response/status/1000
--- error_code: 500
--- response_body eval
qr/500 Internal Server Error/
--- error_log eval
[
    qr/\[crit\] .*? panicked at 'unexpected status: 2'/,
    qr/\[error\] .*? \[wasm\] (?:wasm trap\: )?unreachable/,
]
--- no_error_log
[alert]
[emerg]



=== TEST 3: send_http_response() set headers
should inject headers a produced response, not from echo
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls;
        echo fail;
    }
--- request
GET /t/send_http_response/headers
--- response_headers
Powered-By: proxy-wasm
--- response_body
--- no_error_log
[error]
[alert]
[emerg]



=== TEST 4: send_http_response() set body
should produce a response body, not from echo
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls;
        echo fail;
    }
--- request
GET /t/send_http_response/body
--- response_body chomp
Hello world
--- no_error_log
[warn]
[error]
[alert]
[emerg]



=== TEST 5: send_http_response() set content-length when body
should produce a response body, not from echo
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls;
        echo fail;
    }
--- request
GET /t/send_http_response/body
--- response_headers
Content-Length: 11
--- response_body chomp
Hello world
--- no_error_log
[error]
[alert]
[emerg]



=== TEST 6: send_http_response() set special headers (1/?)
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls;
        echo fail;
    }
--- request
GET /t/send_http_response/special_headers
--- response_headers
Server: proxy-wasm
Date: Wed, 22 Oct 2020 07:28:00 GMT
Location: /index.html
Refresh: 5; url=http://www.w3.org/index.html
Last-Modified: Tue, 15 Nov 1994 12:45:26 GMT
--- ignore_response_body



=== TEST 7: send_http_response() set special headers (2/?)
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls;
        echo fail;
    }
--- request
GET /t/send_http_response/special_headers
--- response_headers
Content-Range: bytes 21010-47021/47022
Accept-Ranges: bytes
WWW-Authenticate: Basic
Expires: Thu, 01 Dec 1994 16:00:00 GMT
E-Tag: 377060cd8c284d8af7ad3082f20958d2
--- ignore_response_body



=== TEST 8: send_http_response() set special headers (3/?)
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls;
        echo fail;
    }
--- request
GET /t/send_http_response/special_headers
--- response_headers
Content-Length: 0
Content-Encoding: gzip
Content-Type: text/plain
Cache-Control: no-cache
Link: </feed>; rel="alternate"
--- ignore_response_body
