# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasmX;

plan_tests(5);
run_tests();

__DATA__

=== TEST 1: proxy_wasm - set_http_request_headers() sets request headers
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/set_request_headers';
        proxy_wasm hostcalls 'test=/t/echo/headers';
    }
--- more_headers
Goodbye: this header
If-Modified-Since: Wed, 21 Oct 2015 07:28:00 GMT
User-Agent: MSIE 4.0
Content-Length: 0
Content-Type: application/text
X-Forwarded-For: 10.0.0.1, 10.0.0.2
--- response_body
Hello: world
Welcome: wasm
--- no_error_log
[error]
[crit]
[alert]



=== TEST 2: proxy_wasm - set_http_request_headers() sets special request headers
should set request headers, even if they receive special treatment
should set special keys headers (e.g. :path)
--- valgrind
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/set_request_headers/special';
        proxy_wasm hostcalls 'test=/t/echo/headers';
        proxy_wasm hostcalls 'on=log test=/t/log/request_header name=:path';
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
--- error_log
request header ":path: /updated"
--- no_error_log
[error]
[crit]



=== TEST 3: proxy_wasm - set_http_request_headers() cannot set invalid special request headers
--- valgrind
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/set_request_headers/invalid';
        return 200;
    }
--- response_body
--- error_log eval
qr/\[error\] .*? \[wasm\] cannot set read-only ":scheme" header/
--- no_error_log
[crit]
[alert]



=== TEST 4: proxy_wasm - set_http_request_headers() sets headers when many headers exist
--- valgrind
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/set_request_headers';
        proxy_wasm hostcalls 'test=/t/echo/headers';
    }
--- more_headers eval
CORE::join "\n", map { "Header$_: value-$_" } 1..20
--- response_body
Hello: world
Welcome: wasm
--- no_error_log
[error]
[crit]
[alert]



=== TEST 5: proxy_wasm - set_http_request_headers() x request_headers phase
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'on=request_headers \
                              test=/t/set_request_headers';
        proxy_wasm hostcalls 'on=request_headers \
                              test=/t/echo/headers';
    }
--- more_headers eval
CORE::join "\n", map { "Header$_: value-$_" } 1..20
--- response_body
Hello: world
Welcome: wasm
--- grep_error_log eval: qr/\[(error|hostcalls)\] [^,]*/
--- grep_error_log_out
[hostcalls] on_request_headers
[hostcalls] testing in "RequestHeaders"
[hostcalls] on_request_headers
[hostcalls] testing in "RequestHeaders"
[hostcalls] on_response_headers
[hostcalls] on_response_headers
[hostcalls] on_response_body
[hostcalls] on_response_body
[hostcalls] on_done while logging request
[hostcalls] on_log while logging request
[hostcalls] on_done while logging request
[hostcalls] on_log while logging request
--- no_error_log
[warn]
[crit]



=== TEST 6: proxy_wasm - set_http_request_headers() x on_response_headers
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'on=response_headers \
                              test=/t/set_request_headers \
                              value=From:response_headers';

        return 200;
    }
--- grep_error_log eval: qr/.*?host trap.*/
--- grep_error_log_out eval
qr~(\[error\]|Uncaught RuntimeError|\s+).*?host trap \(bad usage\): can only set request headers before response is produced.*~
--- no_error_log
[crit]
[warn]
[alert]



=== TEST 7: proxy_wasm - set_http_request_headers() x on_response_body
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'on=response_body \
                              test=/t/set_request_headers \
                              value=From:response_body';

        return 200;
    }
--- grep_error_log eval: qr/.*?host trap.*/
--- grep_error_log_out eval
qr~(\[error\]|Uncaught RuntimeError|\s+).*?host trap \(bad usage\): can only set request headers before response is produced.*~
--- no_error_log
[crit]
[warn]
[alert]



=== TEST 8: proxy_wasm - set_http_request_headers() x on_log
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'on=log \
                              test=/t/set_request_headers \
                              value=From:log';

        return 200;
    }
--- grep_error_log eval: qr/.*?host trap.*/
--- grep_error_log_out eval
qr~(\[error\]|Uncaught RuntimeError|\s+).*?host trap \(bad usage\): can only set request headers before response is produced.*~
--- no_error_log
[crit]
[warn]
[alert]
