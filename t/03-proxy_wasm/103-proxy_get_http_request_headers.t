# vim:set ft= ts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

skip_valgrind();

plan tests => repeat_each() * (blocks() * 6);

run_tests();

__DATA__

=== TEST 1: proxy_wasm - get_http_request_headers() gets request headers
should produce a response with headers
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/echo/headers';
    }
--- more_headers
Hello: world
--- response_body
Host: localhost
Connection: close
Hello: world
--- no_error_log
[error]
[crit]
[alert]
[stderr]



=== TEST 2: proxy_wasm - get_http_request_headers() gets special request headers keys
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/echo/headers/all';
    }
--- more_headers
Hello: world
--- response_body_like
:path: /t
:method: GET
:scheme: http
:authority: localhost:\d+
Host: localhost
Connection: close
Hello: world
--- no_error_log
[error]
[crit]
[alert]
[stderr]



=== TEST 3: proxy_wasm - get_http_request_headers() many headers
should produce a response with 20+ headers
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/echo/headers';
        echo fail;
    }
--- more_headers eval
CORE::join "\n", map { "Header$_: value-$_" } 1..20
--- response_body eval
qq{Host: localhost
Connection: close
}.(CORE::join "\n", map { "Header$_: value-$_" } 1..20) . "\n"
--- no_error_log
[error]
[crit]
[alert]
[stderr]



=== TEST 4: proxy_wasm - get_http_request_headers() too many headers
should produce a response but truncate number of headers if > 100
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/echo/headers';
        echo fail;
    }
--- more_headers eval
CORE::join "\n", map { "Header$_: value-$_" } 1..105
--- response_body eval
qq{Host: localhost
Connection: close
}.(CORE::join "\n", map { "Header$_: value-$_" } 1..94) . "\n"
--- error_log eval
[
    qr/\[warn\] .*? marshalled map truncated to 100 elements/,
    qr/\[debug\] .*? on_request_headers, 107 headers/
]
--- no_error_log
[error]
[alert]



=== TEST 5: proxy_wasm - get_http_request_headers() pipelined requests with large headers
should produce a response with headers
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/echo/headers';
        echo fail;
    }
--- pipelined_requests eval
["GET /t", "GET /t"]
--- more_headers eval
CORE::join "\n", map { "Header$_: value-$_" } 1..105
--- response_body eval
[qq{Host: localhost
Connection: keep-alive
}.(CORE::join "\n", map { "Header$_: value-$_" } 1..94) . "\n",
qq{Host: localhost
Connection: close
}.(CORE::join "\n", map { "Header$_: value-$_" } 1..94) . "\n"]
--- no_error_log
[error]



=== TEST 6: proxy_wasm - get_http_request_headers() x on_phases (1/2)
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t/A {
        proxy_wasm hostcalls 'on=request_headers \
                              test=/t/log/request_headers';
        echo A;
    }

    location /t/B {
        proxy_wasm hostcalls 'on=response_headers \
                              test=/t/log/request_headers';
        echo B;
    }

    location /t {
        echo_location /t/A;
        echo_location /t/B;
    }
--- ignore_response_body
--- error_log eval
[
    qr/\[wasm\] #\d+ entering "RequestHeaders"/,
    qr/\[info\] .*? \[wasm\] Host: localhost/,
    qr/\[wasm\] #\d+ entering "ResponseHeaders"/,
    qr/\[info\] .*? \[wasm\] Host: localhost/,
]
--- no_error_log
[error]



=== TEST 7: proxy_wasm - get_http_request_headers() x on_phases (2/2)
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'on=log \
                              test=/t/log/request_headers';
        echo ok;
    }
--- ignore_response_body
--- error_log eval
[
    qr/\[wasm\] #\d+ entering "Log"/,
    qr/\[info\] .*? \[wasm\] Host: localhost/,
]
--- no_error_log
[error]
[crit]
[alert]
