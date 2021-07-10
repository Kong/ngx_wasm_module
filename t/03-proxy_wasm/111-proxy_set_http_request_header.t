# vim:set ft= ts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

skip_valgrind();

plan tests => repeat_each() * (blocks() * 5);

run_tests();

__DATA__

=== TEST 1: proxy_wasm - set_http_request_header() sets a new non-builtin header
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test_case=/t/set_http_request_header';
        proxy_wasm hostcalls 'test_case=/t/echo/headers';
    }
--- more_headers
pwm-set-req-header: Hello=wasm
--- response_body
Host: localhost
Connection: close
Hello: wasm
--- grep_error_log eval: qr/\[wasm\].*? #\d+ on_request_headers.*/
--- grep_error_log_out eval
qr/\[wasm\] #\d+ on_request_headers, 3 headers
\[wasm\] #\d+ on_request_headers, 4 headers/
--- no_error_log
[error]
[crit]



=== TEST 2: proxy_wasm - set_http_request_header() sets a new non-builtin header (case-sensitive)
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test_case=/t/set_http_request_header';
        proxy_wasm hostcalls 'test_case=/t/echo/headers';
    }
--- more_headers
pwm-set-req-header: hello=wasm
--- response_body
Host: localhost
Connection: close
hello: wasm
--- grep_error_log eval: qr/\[wasm\].*? #\d+ on_request_headers.*/
--- grep_error_log_out eval
qr/\[wasm\] #\d+ on_request_headers, 3 headers
\[wasm\] #\d+ on_request_headers, 4 headers/
--- no_error_log
[error]
[crit]



=== TEST 3: proxy_wasm - set_http_request_header() sets a non-builtin headers when many headers exist
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test_case=/t/set_http_request_header';
        proxy_wasm hostcalls 'test_case=/t/echo/headers';
    }
--- more_headers eval
(CORE::join "\n", map { "Header$_: value-$_" } 1..20)
. "\npwm-set-req-header: Header20=updated"
--- response_body eval
qq{Host: localhost
Connection: close
}.(CORE::join "\n", map { "Header$_: value-$_" } 1..19)
. "\nHeader20: updated\n"
--- no_error_log
[error]
[crit]
[alert]



=== TEST 4: proxy_wasm - set_http_request_header() removes an existing non-builtin header when no value
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test_case=/t/set_http_request_header';
        proxy_wasm hostcalls 'test_case=/t/echo/headers';
    }
--- more_headers
Wasm: incoming
pwm-set-req-header: Wasm=
--- response_body
Host: localhost
Connection: close
--- no_error_log
[error]
[crit]
[alert]



=== TEST 5: proxy_wasm - set_http_request_header() sets the same non-builtin header multiple times
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test_case=/t/set_http_request_header';
        proxy_wasm hostcalls 'test_case=/t/set_http_request_header';
        proxy_wasm hostcalls 'test_case=/t/set_http_request_header';
        proxy_wasm hostcalls 'test_case=/t/echo/headers';
    }
--- more_headers
Hello: invalid
pwm-set-req-header: Hello=wasm
--- response_body
Host: localhost
Connection: close
Hello: wasm
--- grep_error_log eval: qr/\[wasm\].*? #\d+ on_request_headers.*/
--- grep_error_log_out eval
qr/\[wasm\] #\d+ on_request_headers, 4 headers
\[wasm\] #\d+ on_request_headers, 4 headers
\[wasm\] #\d+ on_request_headers, 4 headers
\[wasm\] #\d+ on_request_headers, 4 headers/
--- no_error_log
[error]
[crit]



=== TEST 6: proxy_wasm - set_http_request_header() sets Connection header (close) when many headers exist
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test_case=/t/set_http_request_headers';
        proxy_wasm hostcalls 'test_case=/t/set_http_request_header';
        proxy_wasm hostcalls 'test_case=/t/set_http_request_header';
        proxy_wasm hostcalls 'test_case=/t/echo/headers';
    }
--- more_headers eval
(CORE::join "\n", map { "Header$_: value-$_" } 1..20)
. "\nConnection: keep-alive\n"
. "\npwm-set-req-header: Connection=close\n"
--- response_body
Connection: close
Hello: world
Welcome: wasm
--- grep_error_log eval: qr/\[wasm\].*? #\d+ on_request_headers.*/
--- grep_error_log_out eval
qr/\[wasm\] #\d+ on_request_headers, 23 headers
\[wasm\] #\d+ on_request_headers, 3 headers
\[wasm\] #\d+ on_request_headers, 3 headers
\[wasm\] #\d+ on_request_headers, 3 headers/
--- no_error_log
[error]
[crit]



=== TEST 7: proxy_wasm - set_http_request_header() sets Connection header (keep-alive)
--- abort
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test_case=/t/set_http_request_header';
        proxy_wasm hostcalls 'test_case=/t/set_http_request_header';
        proxy_wasm hostcalls 'test_case=/t/echo/headers';
    }
--- more_headers
Connection: close
pwm-set-req-header: Connection=keep-alive
--- response_body
Host: localhost
Connection: keep-alive
--- grep_error_log eval: qr/\[wasm\].*? #\d+ on_request_headers.*/
--- grep_error_log_out eval
qr/\[wasm\] #\d+ on_request_headers, 3 headers
\[wasm\] #\d+ on_request_headers, 3 headers
\[wasm\] #\d+ on_request_headers, 3 headers/
--- no_error_log
[error]
[crit]



=== TEST 8: proxy_wasm - set_http_request_header() sets Connection header (closed)
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test_case=/t/set_http_request_header';
        proxy_wasm hostcalls 'test_case=/t/set_http_request_header';
        proxy_wasm hostcalls 'test_case=/t/echo/headers';
    }
--- more_headers
pwm-set-req-header: Connection=closed
--- response_body
Host: localhost
Connection: closed
--- grep_error_log eval: qr/\[wasm\].*? #\d+ on_request_headers.*/
--- grep_error_log_out eval
qr/\[wasm\] #\d+ on_request_headers, 3 headers
\[wasm\] #\d+ on_request_headers, 3 headers
\[wasm\] #\d+ on_request_headers, 3 headers/
--- no_error_log
[error]
[crit]



=== TEST 9: proxy_wasm - set_http_request_header() sets Content-Length header
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test_case=/t/set_http_request_header';
        proxy_wasm hostcalls 'test_case=/t/set_http_request_header';
        proxy_wasm hostcalls 'test_case=/t/echo/headers';
    }
--- more_headers
Content-Length: 10
pwm-set-req-header: Content-Length=0
--- response_body
Host: localhost
Connection: close
Content-Length: 0
--- grep_error_log eval: qr/\[wasm\].*? #\d+ on_request_headers.*/
--- grep_error_log_out eval
qr/\[wasm\] #\d+ on_request_headers, 4 headers
\[wasm\] #\d+ on_request_headers, 4 headers
\[wasm\] #\d+ on_request_headers, 4 headers/
--- no_error_log
[error]
[crit]



=== TEST 10: proxy_wasm - set_http_request_header() removes Content-Length header when no value
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test_case=/t/set_http_request_header';
        proxy_wasm hostcalls 'test_case=/t/set_http_request_header';
        proxy_wasm hostcalls 'test_case=/t/echo/headers';
    }
--- more_headers
Content-Length: 10
pwm-set-req-header: Content-Length=
--- response_body
Host: localhost
Connection: close
--- grep_error_log eval: qr/\[wasm\].*? #\d+ on_request_headers.*/
--- grep_error_log_out eval
qr/\[wasm\] #\d+ on_request_headers, 4 headers
\[wasm\] #\d+ on_request_headers, 3 headers
\[wasm\] #\d+ on_request_headers, 3 headers/
--- no_error_log
[error]
[crit]



=== TEST 11: proxy_wasm - set_http_request_header() x on_phases
should log an error (but no trap) when response is produced
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'on_phase=http_request_headers \
                              test_case=/t/set_http_request_header';
        proxy_wasm hostcalls 'on_phase=http_request_headers \
                              test_case=/t/echo/headers';
        proxy_wasm hostcalls 'on_phase=http_response_headers \
                              test_case=/t/set_http_request_header';
        proxy_wasm hostcalls 'on_phase=log \
                              test_case=/t/set_http_request_header';
    }
--- more_headers
Hello: world
pwm-set-req-header: Hello=world
--- response_body_like
Hello: world
--- grep_error_log eval: qr/\[(info|error|crit)\] .*?(?=(\s+<|,|\n))/
--- grep_error_log_out eval
qr/.*?
\[info\] .*? \[wasm\] #\d+ entering "HttpRequestHeaders"
\[info\] .*? \[wasm\] #\d+ entering "HttpRequestHeaders"
\[info\] .*? \[wasm\] #\d+ entering "HttpResponseHeaders"
\[error\] .*? \[wasm\] cannot set request header: response produced
\[info\] .*? \[wasm\] #\d+ entering "Log"
\[error\] .*? \[wasm\] cannot set request header: response produced/
--- no_error_log
[warn]
[crit]
