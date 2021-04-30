# vim:set ft= ts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

skip_valgrind();

plan tests => repeat_each() * (blocks() * 8);

add_block_preprocessor(sub {
    my $block = shift;

    if (!defined $block->no_error_log) {
        $block->set_value("no_error_log",
                          "[error]\n[crit]\n[emerg]\n[alert]\nstub\nstub");
    }
});

run_tests();

__DATA__

=== TEST 1: proxy_wasm - add_http_request_header() adds a new request header
should add a request header visible in the second filter
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test_case=/t/add_http_request_header';
        proxy_wasm hostcalls 'test_case=/t/log/request_headers';
        return 200;
    }
--- more_headers
pwm-add-req-header: Hello=world
--- response_body
--- error_log eval
[
    qr/\[wasm\] #\d+ on_request_headers, 3 headers/,
    qr/\[wasm\] #\d+ on_request_headers, 4 headers/,
    qr/\[wasm\] Host: localhost/,
    qr/\[wasm\] Connection: close/,
    qr/\[wasm\] Hello: world/
]
--- no_error_log
[error]



=== TEST 2: proxy_wasm - add_http_request_header() adds an already existing header
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test_case=/t/add_http_request_header';
        proxy_wasm hostcalls 'test_case=/t/echo/headers';
    }
--- more_headers
Hello: world
pwm-add-req-header: Hello=world
--- response_body_like
Hello: world
Hello: world



=== TEST 3: proxy_wasm - add_http_request_header() adds multiple times the same name/value
should add multiple instances of the header
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test_case=/t/add_http_request_header';
        proxy_wasm hostcalls 'test_case=/t/add_http_request_header';
        proxy_wasm hostcalls 'test_case=/t/echo/headers';
    }
--- more_headers
pwm-add-req-header: Hello=world
--- response_body_like
Hello: world
Hello: world



=== TEST 4: proxy_wasm - add_http_request_header() does not add a new empty value
should not add a header with no value
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test_case=/t/add_http_request_header';
        proxy_wasm hostcalls 'test_case=/t/echo/headers';
    }
--- more_headers
pwm-add-req-header: Hello=
--- response_body
Host: localhost
Connection: close



=== TEST 5: proxy_wasm - add_http_request_header() does not clear an existing header
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test_case=/t/add_http_request_header';
        proxy_wasm hostcalls 'test_case=/t/echo/headers';
    }
--- more_headers
Hello: world
pwm-add-req-header: Hello=
--- response_body
Host: localhost
Connection: close
Hello: world



=== TEST 6: proxy_wasm - add_http_request_header() updates existing Host header
should update previously parsed builtin header
should update r->headers_in.server
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test_case=/t/add_http_request_header';
        proxy_wasm hostcalls 'test_case=/t/echo/headers';
    }
--- more_headers
pwm-add-req-header: Host=hello
--- response_body
Host: hello
Connection: close
--- error_log eval
[
    qr/on_request_headers, .*? host: "localhost"/,
    qr/on_request_headers, .*? host: "hello"/,
]
--- no_error_log
[error]
[crit]
[emerg]
[alert]



=== TEST 7: proxy_wasm - add_http_request_header() clears existing Host header
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test_case=/t/add_http_request_header';
        proxy_wasm hostcalls 'test_case=/t/echo/headers';
    }
--- more_headers
pwm-add-req-header: Host=
--- response_body
Connection: close
--- error_log eval
[
    qr/on_request_headers, .*? host: "localhost"/,
    qr/on_request_headers, .*? request: "GET \/t HTTP\/1\.1"$/,
]
--- no_error_log
[error]
[crit]
[emerg]
[alert]



=== TEST 8: proxy_wasm - add_http_request_header() updates existing Connection header
should update previously parsed builtin header
should update r->keepalive
--- timeout: 5s
--- abort
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test_case=/t/add_http_request_header';
        proxy_wasm hostcalls 'test_case=/t/echo/headers';
    }
--- more_headers
Connection: close
pwm-add-req-header: Connection=keep-alive
--- response_headers
Connection: keep-alive
--- response_body
Host: localhost
Connection: keep-alive
--- no_error_log
stub
[error]
[crit]
[emerg]
[alert]



=== TEST 9: proxy_wasm - add_http_request_header() clears existing Connection header
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test_case=/t/add_http_request_header';
        proxy_wasm hostcalls 'test_case=/t/echo/headers';
    }
--- more_headers
Connection: close
pwm-add-req-header: Connection=
--- response_headers
Connection: close
--- response_body
Host: localhost
--- no_error_log
stub
[error]
[crit]
[emerg]
[alert]



=== TEST 10: proxy_wasm - add_http_request_header() adds Content-Length header
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



=== TEST 11: proxy_wasm - add_http_request_header() updates existing Content-Length header
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test_case=/t/add_http_request_header';
        proxy_wasm hostcalls 'test_case=/t/echo/headers';
    }
--- more_headers
Content-Length: 1
pwm-add-req-header: Content-Length=0
--- response_body
Host: localhost
Connection: close
Content-Length: 0



=== TEST 12: proxy_wasm - add_http_request_header() clears existing Content-Length header
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test_case=/t/add_http_request_header';
        proxy_wasm hostcalls 'test_case=/t/echo/headers';
    }
--- more_headers
Content-Length: 1
pwm-add-req-header: Content-Length=
--- response_body
Host: localhost
Connection: close



=== TEST 13: proxy_wasm - add_http_request_header() catches invalid Content-Length header
should produce an error
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test_case=/t/add_http_request_header';
        proxy_wasm hostcalls 'test_case=/t/echo/headers';
    }
--- more_headers
pwm-add-req-header: Content-Length=FF
--- response_body
Host: localhost
Connection: close
--- error_log eval
qr/\[error\] .*? \[wasm\] attempt to set invalid Content-Length request header: "FF"/
--- no_error_log
stub
[crit]
[emerg]
[alert]
[stderr]



=== TEST 14: proxy_wasm - add_http_request_header() clears existing User-Agent header
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test_case=/t/add_http_request_header';
        proxy_wasm hostcalls 'test_case=/t/echo/headers';
    }
--- more_headers
User-Agent: Mozilla/5.0
pwm-add-req-header: User-Agent=
--- response_body
Host: localhost
Connection: close



=== TEST 15: proxy_wasm - add_http_request_header() updates existing User-Agent header
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test_case=/t/add_http_request_header';
        proxy_wasm hostcalls 'test_case=/t/echo/headers';
    }
--- more_headers
User-Agent: Mozilla/5.0
pwm-add-req-header: User-Agent=Mozilla/4.0
--- response_body
Host: localhost
Connection: close
User-Agent: Mozilla/4.0



=== TEST 16: proxy_wasm - add_http_request_header() adds User-Agent header
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



=== TEST 17: proxy_wasm - add_http_request_header() updates a builtin header (If-Modified-Since)
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test_case=/t/add_http_request_header';
        proxy_wasm hostcalls 'test_case=/t/echo/headers';
    }
--- more_headers
If-Modified-Since: Mon, 8 Sep 2015 07:28:00 GMT
pwm-add-req-header: If-Modified-Since=Wed, 21 Oct 2015 07:28:00 GMT
--- response_body
Host: localhost
Connection: close
If-Modified-Since: Wed, 21 Oct 2015 07:28:00 GMT



=== TEST 18: proxy_wasm - add_http_request_header() adds a builtin multi header (X-Forwarded-For)
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



=== TEST 19: proxy_wasm - add_http_request_header() adds an already existing builtin multi header (X-Forwarded-For)
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



=== TEST 20: proxy_wasm - add_http_request_header() updates existing User-Agent
should update previously parsed builtin header
should set r->headers_in.msie6 = 1 (coverage only)
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test_case=/t/add_http_request_header';
        proxy_wasm hostcalls 'test_case=/t/echo/headers';
    }
--- more_headers
User-Agent: Mozilla/5.0
pwm-add-req-header: User-Agent=MSIE 4.0
--- response_body
Host: localhost
Connection: close
User-Agent: MSIE 4.0



=== TEST 21: proxy_wasm - add_http_request_header() updates existing User-Agent (MSIE)
should update previously parsed builtin header
should set r->headers_in.msie6 = 1 (coverage only)
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test_case=/t/add_http_request_header';
        proxy_wasm hostcalls 'test_case=/t/echo/headers';
    }
--- more_headers
User-Agent: Mozilla/5.0
pwm-add-req-header: User-Agent=MSIE 6.0
--- response_body
Host: localhost
Connection: close
User-Agent: MSIE 6.0



=== TEST 22: proxy_wasm - add_http_request_header() updates existing User-Agent (Opera)
should update previously parsed builtin header
should set r->headers_in.opera = 1 (coverage only)
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test_case=/t/add_http_request_header';
        proxy_wasm hostcalls 'test_case=/t/echo/headers';
    }
--- more_headers
User-Agent: Mozilla/5.0
pwm-add-req-header: User-Agent=Opera
--- response_body
Host: localhost
Connection: close
User-Agent: Opera



=== TEST 23: proxy_wasm - add_http_request_header() updates existing User-Agent (Gecko)
should update previously parsed builtin header
should set r->headers_in.gecko = 1 (coverage only)
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test_case=/t/add_http_request_header';
        proxy_wasm hostcalls 'test_case=/t/echo/headers';
    }
--- more_headers
User-Agent: Mozilla/5.0
pwm-add-req-header: User-Agent=Gecko
--- response_body
Host: localhost
Connection: close
User-Agent: Gecko



=== TEST 24: proxy_wasm - add_http_request_header() updates existing User-Agent (Chrome)
should update previously parsed builtin header
should set r->headers_in.chrome = 1 (coverage only)
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test_case=/t/add_http_request_header';
        proxy_wasm hostcalls 'test_case=/t/echo/headers';
    }
--- more_headers
User-Agent: Mozilla/5.0
pwm-add-req-header: User-Agent=Chrome/
--- response_body
Host: localhost
Connection: close
User-Agent: Chrome/



=== TEST 25: proxy_wasm - add_http_request_header() updates existing User-Agent (Safari)
should update previously parsed builtin header
should set r->headers_in.safari = 1 (coverage only)
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test_case=/t/add_http_request_header';
        proxy_wasm hostcalls 'test_case=/t/echo/headers';
    }
--- more_headers
User-Agent: Mozilla/5.0
pwm-add-req-header: User-Agent=Safari/Mac OS X
--- response_body
Host: localhost
Connection: close
User-Agent: Safari/Mac OS X



=== TEST 26: proxy_wasm - add_http_request_header() updates existing User-Agent (Konqueror)
should update previously parsed builtin header
should set r->headers_in.konqueror = 1 (coverage only)
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test_case=/t/add_http_request_header';
        proxy_wasm hostcalls 'test_case=/t/echo/headers';
    }
--- more_headers
User-Agent: Mozilla/5.0
pwm-add-req-header: User-Agent=Konqueror
--- response_body
Host: localhost
Connection: close
User-Agent: Konqueror



=== TEST 27: proxy_wasm - add_http_request_header() x on_phases
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'on_phase=http_request_headers test_case=/t/add_http_request_header';
        proxy_wasm hostcalls 'test_case=/t/echo/headers';
        proxy_wasm hostcalls 'on_phase=http_response_headers test_case=/t/add_http_request_header';
        proxy_wasm hostcalls 'on_phase=log test_case=/t/add_http_request_header';
    }
--- more_headers
Hello: world
pwm-add-req-header: Hello=world
--- response_body_like
Hello: world
Hello: world
--- error_log eval
[
    qr/\[wasm\] \[tests\] #\d+ entering "HttpRequestHeaders"/,
    qr/\[wasm\] \[tests\] #\d+ entering "HttpResponseHeaders"/,
    qr/\[wasm\] \[tests\] #\d+ entering "Log"/,
]
--- no_error_log
[error]
[crit]
[emerg]
