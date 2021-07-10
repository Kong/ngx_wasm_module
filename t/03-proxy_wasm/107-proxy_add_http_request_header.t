# vim:set ft= ts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

skip_valgrind();

plan tests => repeat_each() * (blocks() * 6);

add_block_preprocessor(sub {
    my $block = shift;

    if (!defined $block->no_error_log) {
        $block->set_value("no_error_log",
                          "[error]\n[crit]\n[alert]");
    }
});

run_tests();

__DATA__

=== TEST 1: proxy_wasm - add_http_request_header() adds a new non-builtin header
should add a request header visible in the second filter
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test_case=/t/add_http_request_header';
        proxy_wasm hostcalls 'test_case=/t/echo/headers';
    }
--- more_headers
pwm-add-req-header: Hello=world
--- response_body
Host: localhost
Connection: close
Hello: world
--- grep_error_log eval: qr/\[debug\] .*? \[wasm\] #\d+ on_request_headers.*/
--- grep_error_log_out eval
qr/\[debug\] .*? \[wasm\] #\d+ on_request_headers, 3 headers
\[debug\] .*? \[wasm\] #\d+ on_request_headers, 4 headers/



=== TEST 2: proxy_wasm - add_http_request_header() adds a new non-builtin header with empty value
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test_case=/t/add_http_request_header';
        proxy_wasm hostcalls 'test_case=/t/echo/headers';
    }
--- more_headers
pwm-add-req-header: Hello=
--- response_body_like
Host: localhost
Connection: close
Hello:\s
--- grep_error_log eval: qr/\[debug\] .*? \[wasm\] #\d+ on_request_headers.*/
--- grep_error_log_out eval
qr/\[debug\] .*? \[wasm\] #\d+ on_request_headers, 3 headers
\[debug\] .*? \[wasm\] #\d+ on_request_headers, 4 headers/



=== TEST 3: proxy_wasm - add_http_request_header() adds an existing non-builtin header
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test_case=/t/add_http_request_header';
        proxy_wasm hostcalls 'test_case=/t/echo/headers';
    }
--- more_headers
Hello: world
pwm-add-req-header: Hello=world
--- response_body
Host: localhost
Connection: close
Hello: world
Hello: world
--- grep_error_log eval: qr/\[debug\] .*? \[wasm\] #\d+ on_request_headers.*/
--- grep_error_log_out eval
qr/\[debug\] .*? \[wasm\] #\d+ on_request_headers, 4 headers
\[debug\] .*? \[wasm\] #\d+ on_request_headers, 5 headers/



=== TEST 4: proxy_wasm - add_http_request_header() adds an existing non-builtin header with empty value
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test_case=/t/add_http_request_header';
        proxy_wasm hostcalls 'test_case=/t/echo/headers';
    }
--- more_headers
Hello: world
pwm-add-req-header: Hello=
--- response_body_like
Host: localhost
Connection: close
Hello: world
Hello:\s
--- grep_error_log eval: qr/\[debug\] .*? \[wasm\] #\d+ on_request_headers.*/
--- grep_error_log_out eval
qr/\[debug\] .*? \[wasm\] #\d+ on_request_headers, 4 headers
\[debug\] .*? \[wasm\] #\d+ on_request_headers, 5 headers/



=== TEST 5: proxy_wasm - add_http_request_header() adds the same non-builtin header/value multiple times
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test_case=/t/add_http_request_header';
        proxy_wasm hostcalls 'test_case=/t/add_http_request_header';
        proxy_wasm hostcalls 'test_case=/t/echo/headers';
    }
--- more_headers
pwm-add-req-header: Hello=world
--- response_body
Host: localhost
Connection: close
Hello: world
Hello: world
--- grep_error_log eval: qr/\[debug\] .*? \[wasm\] #\d+ on_request_headers.*/
--- grep_error_log_out eval
qr/\[debug\] .*? \[wasm\] #\d+ on_request_headers, 3 headers
\[debug\] .*? \[wasm\] #\d+ on_request_headers, 4 headers/



=== TEST 6: proxy_wasm - add_http_request_header() cannot add Host header if exists
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test_case=/t/add_http_request_header';
        proxy_wasm hostcalls 'test_case=/t/echo/headers';
    }
--- more_headers
pwm-add-req-header: Host=invalid
--- response_body_like
Host: localhost
--- error_log eval
qr/\[error\] .*? \[wasm\] cannot add new "Host" builtin request header/
--- no_error_log
[crit]
[alert]
[stderr]



=== TEST 7: proxy_wasm - add_http_request_header() adds Connection header if exists
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test_case=/t/add_http_request_header';
        proxy_wasm hostcalls 'test_case=/t/echo/headers';
    }
--- more_headers
pwm-add-req-header: Connection=closed
--- response_body
Host: localhost
Connection: close
Connection: closed
--- grep_error_log eval: qr/\[debug\] .*? \[wasm\] #\d+ on_request_headers.*/
--- grep_error_log_out eval
qr/\[debug\] .*? \[wasm\] #\d+ on_request_headers, 3 headers
\[debug\] .*? \[wasm\] #\d+ on_request_headers, 4 headers/



=== TEST 8: proxy_wasm - add_http_request_header() adds Connection header with empty value
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test_case=/t/add_http_request_header';
        proxy_wasm hostcalls 'test_case=/t/echo/headers';
    }
--- more_headers
pwm-add-req-header: Connection=
--- response_body_like
Host: localhost
Connection: close
Connection:\s
--- grep_error_log eval: qr/\[debug\] .*? \[wasm\] #\d+ on_request_headers.*/
--- grep_error_log_out eval
qr/\[debug\] .*? \[wasm\] #\d+ on_request_headers, 3 headers
\[debug\] .*? \[wasm\] #\d+ on_request_headers, 4 headers/



=== TEST 9: proxy_wasm - add_http_request_header() adds a new Content-Length header
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test_case=/t/add_http_request_header';
        proxy_wasm hostcalls 'test_case=/t/echo/headers';
    }
--- more_headers
pwm-add-req-header: Content-Length=0
--- response_body
Host: localhost
Connection: close
Content-Length: 0
--- grep_error_log eval: qr/\[debug\] .*? \[wasm\] #\d+ on_request_headers.*/
--- grep_error_log_out eval
qr/\[debug\] .*? \[wasm\] #\d+ on_request_headers, 3 headers
\[debug\] .*? \[wasm\] #\d+ on_request_headers, 4 headers/



=== TEST 10: proxy_wasm - add_http_request_header() cannot add Content-Length if exists
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test_case=/t/add_http_request_header';
        proxy_wasm hostcalls 'test_case=/t/echo/headers';
    }
--- more_headers
Content-Length: 0
pwm-add-req-header: Content-Length=0
--- response_body_like
Content-Length: 0
--- error_log eval
qr/\[error\] .*? \[wasm\] cannot add new "Content-Length" builtin request header/
--- no_error_log
[crit]
[alert]
[stderr]



=== TEST 11: proxy_wasm - add_http_request_header() cannot add empty Content-Length
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test_case=/t/add_http_request_header';
        proxy_wasm hostcalls 'test_case=/t/echo/headers';
    }
--- more_headers
pwm-add-req-header: Content-Length=
--- response_body_unlike
Content-Length
--- error_log eval
qr/\[error\] .*? \[wasm\] attempt to set invalid Content-Length request header: ""/
--- no_error_log
[crit]
[alert]
[stderr]



=== TEST 12: proxy_wasm - add_http_request_header() cannot add invalid Content-Length header
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test_case=/t/add_http_request_header';
        proxy_wasm hostcalls 'test_case=/t/echo/headers';
    }
--- more_headers
pwm-add-req-header: Content-Length=FF
--- response_body_unlike
Content-Length
--- error_log eval
qr/\[error\] .*? \[wasm\] attempt to set invalid Content-Length request header: "FF"/
--- no_error_log
[crit]
[alert]
[stderr]



=== TEST 13: proxy_wasm - add_http_request_header() adds a new User-Agent header
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test_case=/t/add_http_request_header';
        proxy_wasm hostcalls 'test_case=/t/echo/headers';
    }
--- more_headers
pwm-add-req-header: User-Agent=Mozilla/5.0
--- response_body
Host: localhost
Connection: close
User-Agent: Mozilla/5.0
--- grep_error_log eval: qr/\[debug\] .*? \[wasm\] #\d+ on_request_headers.*/
--- grep_error_log_out eval
qr/\[debug\] .*? \[wasm\] #\d+ on_request_headers, 3 headers
\[debug\] .*? \[wasm\] #\d+ on_request_headers, 4 headers/



=== TEST 14: proxy_wasm - add_http_request_header() cannot add User-Agent header if exists
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test_case=/t/add_http_request_header';
        proxy_wasm hostcalls 'test_case=/t/echo/headers';
    }
--- more_headers
User-Agent: Mozilla/4.0
pwm-add-req-header: User-Agent=Mozilla/5.0
--- response_body_like
User-Agent: Mozilla\/4\.0
--- error_log eval
qr/\[error\] .*? \[wasm\] cannot add new "User-Agent" builtin request header/
--- no_error_log
[crit]
[alert]
[stderr]



=== TEST 15: proxy_wasm - add_http_request_header() adds a new User-Agent header with empty value
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test_case=/t/add_http_request_header';
        proxy_wasm hostcalls 'test_case=/t/echo/headers';
    }
--- more_headers
pwm-add-req-header: User-Agent=
--- response_body_like
Host: localhost
Connection: close
User-Agent:\s
--- grep_error_log eval: qr/\[debug\] .*? \[wasm\] #\d+ on_request_headers.*/
--- grep_error_log_out eval
qr/\[debug\] .*? \[wasm\] #\d+ on_request_headers, 3 headers
\[debug\] .*? \[wasm\] #\d+ on_request_headers, 4 headers/



=== TEST 16: proxy_wasm - add_http_request_header() adds a new builtin header (If-Modified-Since)
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test_case=/t/add_http_request_header';
        proxy_wasm hostcalls 'test_case=/t/echo/headers';
    }
--- more_headers
pwm-add-req-header: If-Modified-Since=Wed, 21 Oct 2015 07:28:00 GMT
--- response_body
Host: localhost
Connection: close
If-Modified-Since: Wed, 21 Oct 2015 07:28:00 GMT
--- grep_error_log eval: qr/\[debug\] .*? \[wasm\] #\d+ on_request_headers.*/
--- grep_error_log_out eval
qr/\[debug\] .*? \[wasm\] #\d+ on_request_headers, 3 headers
\[debug\] .*? \[wasm\] #\d+ on_request_headers, 4 headers/



=== TEST 17: proxy_wasm - add_http_request_header() cannot add a new builtin header (If-Modified-Since) if exists
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test_case=/t/add_http_request_header';
        proxy_wasm hostcalls 'test_case=/t/echo/headers';
    }
--- more_headers
If-Modified-Since: Mon, 8 Sep 2015 07:28:00 GMT
pwm-add-req-header: If-Modified-Since=Wed, 21 Oct 2015 07:28:00 GMT
--- response_body_like
If-Modified-Since: Mon, 8 Sep 2015 07:28:00 GMT
--- error_log eval
qr/\[error\] .*? \[wasm\] cannot add new "If-Modified-Since" builtin request header/
--- no_error_log
[crit]
[alert]
[stderr]



=== TEST 18: proxy_wasm - add_http_request_header() adds a new builtin header (If-Modified-Since) with empty value
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test_case=/t/add_http_request_header';
        proxy_wasm hostcalls 'test_case=/t/echo/headers';
    }
--- more_headers
pwm-add-req-header: If-Modified-Since=
--- response_body_like
Host: localhost
Connection: close
If-Modified-Since:\s
--- grep_error_log eval: qr/\[debug\] .*? \[wasm\] #\d+ on_request_headers.*/
--- grep_error_log_out eval
qr/\[debug\] .*? \[wasm\] #\d+ on_request_headers, 3 headers
\[debug\] .*? \[wasm\] #\d+ on_request_headers, 4 headers/



=== TEST 19: proxy_wasm - add_http_request_header() adds a new builtin multi header (X-Forwarded-For)
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test_case=/t/add_http_request_header';
        proxy_wasm hostcalls 'test_case=/t/echo/headers';
    }
--- more_headers
pwm-add-req-header: X-Forwarded-For=10.0.0.2
--- response_body
Host: localhost
Connection: close
X-Forwarded-For: 10.0.0.2
--- grep_error_log eval: qr/\[debug\] .*? \[wasm\] #\d+ on_request_headers.*/
--- grep_error_log_out eval
qr/\[debug\] .*? \[wasm\] #\d+ on_request_headers, 3 headers
\[debug\] .*? \[wasm\] #\d+ on_request_headers, 4 headers/



=== TEST 20: proxy_wasm - add_http_request_header() adds builtin multi header (X-Forwarded-For) if exists
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test_case=/t/add_http_request_header';
        proxy_wasm hostcalls 'test_case=/t/echo/headers';
    }
--- more_headers
X-Forwarded-For: 10.0.0.1
pwm-add-req-header: X-Forwarded-For=10.0.0.2
--- response_body
Host: localhost
Connection: close
X-Forwarded-For: 10.0.0.1
X-Forwarded-For: 10.0.0.2
--- grep_error_log eval: qr/\[debug\] .*? \[wasm\] #\d+ on_request_headers.*/
--- grep_error_log_out eval
qr/\[debug\] .*? \[wasm\] #\d+ on_request_headers, 4 headers
\[debug\] .*? \[wasm\] #\d+ on_request_headers, 5 headers/



=== TEST 21: proxy_wasm - add_http_request_header() new User-Agent sets flags (MSIE)
should set r->headers_in.msie6 = 1 (coverage only)
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test_case=/t/add_http_request_header';
        proxy_wasm hostcalls 'test_case=/t/echo/headers';
    }
--- more_headers
pwm-add-req-header: User-Agent=MSIE 4.0
--- response_body
Host: localhost
Connection: close
User-Agent: MSIE 4.0
--- grep_error_log eval: qr/\[debug\] .*? \[wasm\] #\d+ on_request_headers.*/
--- grep_error_log_out eval
qr/\[debug\] .*? \[wasm\] #\d+ on_request_headers, 3 headers
\[debug\] .*? \[wasm\] #\d+ on_request_headers, 4 headers/



=== TEST 22: proxy_wasm - add_http_request_header() new User-Agent sets flags (MSIE)
should set r->headers_in.msie6 = 1 (coverage only)
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test_case=/t/add_http_request_header';
        proxy_wasm hostcalls 'test_case=/t/echo/headers';
    }
--- more_headers
pwm-add-req-header: User-Agent=MSIE 6.0
--- response_body
Host: localhost
Connection: close
User-Agent: MSIE 6.0
--- grep_error_log eval: qr/\[debug\] .*? \[wasm\] #\d+ on_request_headers.*/
--- grep_error_log_out eval
qr/\[debug\] .*? \[wasm\] #\d+ on_request_headers, 3 headers
\[debug\] .*? \[wasm\] #\d+ on_request_headers, 4 headers/



=== TEST 23: proxy_wasm - add_http_request_header() new User-Agent sets flags (Opera)
should set r->headers_in.opera = 1 (coverage only)
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test_case=/t/add_http_request_header';
        proxy_wasm hostcalls 'test_case=/t/echo/headers';
    }
--- more_headers
pwm-add-req-header: User-Agent=Opera
--- response_body
Host: localhost
Connection: close
User-Agent: Opera
--- grep_error_log eval: qr/\[debug\] .*? \[wasm\] #\d+ on_request_headers.*/
--- grep_error_log_out eval
qr/\[debug\] .*? \[wasm\] #\d+ on_request_headers, 3 headers
\[debug\] .*? \[wasm\] #\d+ on_request_headers, 4 headers/



=== TEST 24: proxy_wasm - add_http_request_header() new User-Agent sets flags (Gecko)
should set r->headers_in.gecko = 1 (coverage only)
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test_case=/t/add_http_request_header';
        proxy_wasm hostcalls 'test_case=/t/echo/headers';
    }
--- more_headers
pwm-add-req-header: User-Agent=Gecko/
--- response_body
Host: localhost
Connection: close
User-Agent: Gecko/
--- grep_error_log eval: qr/\[debug\] .*? \[wasm\] #\d+ on_request_headers.*/
--- grep_error_log_out eval
qr/\[debug\] .*? \[wasm\] #\d+ on_request_headers, 3 headers
\[debug\] .*? \[wasm\] #\d+ on_request_headers, 4 headers/



=== TEST 25: proxy_wasm - add_http_request_header() new User-Agent sets flags (Chrome)
should set r->headers_in.chrome = 1 (coverage only)
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test_case=/t/add_http_request_header';
        proxy_wasm hostcalls 'test_case=/t/echo/headers';
    }
--- more_headers
pwm-add-req-header: User-Agent=Chrome/
--- response_body
Host: localhost
Connection: close
User-Agent: Chrome/
--- grep_error_log eval: qr/\[debug\] .*? \[wasm\] #\d+ on_request_headers.*/
--- grep_error_log_out eval
qr/\[debug\] .*? \[wasm\] #\d+ on_request_headers, 3 headers
\[debug\] .*? \[wasm\] #\d+ on_request_headers, 4 headers/



=== TEST 26: proxy_wasm - add_http_request_header() new User-Agent sets flags (Safari)
should set r->headers_in.safari = 1 (coverage only)
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test_case=/t/add_http_request_header';
        proxy_wasm hostcalls 'test_case=/t/echo/headers';
    }
--- more_headers
pwm-add-req-header: User-Agent=Safari/Mac OS X
--- response_body
Host: localhost
Connection: close
User-Agent: Safari/Mac OS X
--- grep_error_log eval: qr/\[debug\] .*? \[wasm\] #\d+ on_request_headers.*/
--- grep_error_log_out eval
qr/\[debug\] .*? \[wasm\] #\d+ on_request_headers, 3 headers
\[debug\] .*? \[wasm\] #\d+ on_request_headers, 4 headers/



=== TEST 27: proxy_wasm - add_http_request_header() updates existing User-Agent (Konqueror)
should set r->headers_in.konqueror = 1 (coverage only)
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test_case=/t/add_http_request_header';
        proxy_wasm hostcalls 'test_case=/t/echo/headers';
    }
--- more_headers
pwm-add-req-header: User-Agent=Konqueror
--- response_body
Host: localhost
Connection: close
User-Agent: Konqueror
--- grep_error_log eval: qr/\[debug\] .*? \[wasm\] #\d+ on_request_headers.*/
--- grep_error_log_out eval
qr/\[debug\] .*? \[wasm\] #\d+ on_request_headers, 3 headers
\[debug\] .*? \[wasm\] #\d+ on_request_headers, 4 headers/



=== TEST 28: proxy_wasm - add_http_request_header() x on_phases
should log an error (but no trap) when response is produced
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'on_phase=http_request_headers \
                              test_case=/t/add_http_request_header';
        proxy_wasm hostcalls 'on_phase=http_request_headers \
                              test_case=/t/echo/headers';
        proxy_wasm hostcalls 'on_phase=http_response_headers \
                              test_case=/t/add_http_request_header';
        proxy_wasm hostcalls 'on_phase=log \
                              test_case=/t/add_http_request_header';
    }
--- more_headers
Hello: world
pwm-add-req-header: Hello=world
--- response_body_like
Hello: world
Hello: world
--- grep_error_log eval: qr/\[(info|error|crit)\] .*?(?=(\s+<|,|\n))/
--- grep_error_log_out eval
qr/.*?
\[info\] .*? \[wasm\] #\d+ entering "HttpRequestHeaders"
\[info\] .*? \[wasm\] #\d+ entering "HttpRequestHeaders"
\[info\] .*? \[wasm\] #\d+ entering "HttpResponseHeaders"
\[error\] .*? \[wasm\] cannot add request header: response produced
\[info\] .*? \[wasm\] #\d+ entering "Log"
\[error\] .*? \[wasm\] cannot add request header: response produced/
--- no_error_log
[warn]
[crit]
[alert]
