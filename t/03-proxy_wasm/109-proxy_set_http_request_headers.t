# vim:set ft= ts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

skip_valgrind();

plan tests => repeat_each() * (blocks() * 5);

run_tests();

__DATA__

=== TEST 1: proxy_wasm - set_http_request_headers() sets request headers
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test_case=/t/set_http_request_headers';
        proxy_wasm hostcalls 'test_case=/t/echo/headers';
    }
--- more_headers
Goodbye: this header
If-Modified-Since: Wed, 21 Oct 2015 07:28:00 GMT
User-Agent: MSIE 4.0
Content-Length: 0
Content-Type: application/text
X-Forwarded-For: 10.0.0.1, 10.0.0.2
--- response_body
Connection: close
Hello: world
Welcome: wasm
--- no_error_log
[error]
[crit]
[emerg]



=== TEST 2: proxy_wasm - set_http_request_headers() sets special request headers
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test_case=/t/set_http_request_headers/special';
        proxy_wasm hostcalls 'test_case=/t/echo/headers';
    }
--- more_headers
Goodbye: this header
If-Modified-Since: Wed, 21 Oct 2015 07:28:00 GMT
User-Agent: MSIE 4.0
Content-Length: 0
Content-Type: application/text
X-Forwarded-For: 10.0.0.1, 10.0.0.2
--- response_body
Host: somehost
Connection: closed
User-Agent: Gecko
Content-Type: text/none
X-Forwarded-For: 128.168.0.1
--- no_error_log
[error]
[crit]
[emerg]



=== TEST 3: proxy_wasm - set_http_request_headers() sets headers when many headers exist
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test_case=/t/set_http_request_headers';
        proxy_wasm hostcalls 'test_case=/t/echo/headers';
    }
--- more_headers eval
CORE::join "\n", map { "Header$_: value-$_" } 1..20
--- response_body
Connection: close
Hello: world
Welcome: wasm
--- no_error_log
[error]
[crit]
[emerg]
