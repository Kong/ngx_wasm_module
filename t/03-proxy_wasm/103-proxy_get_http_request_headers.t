# vim:set ft= ts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

skip_valgrind();

plan tests => repeat_each() * (blocks() * 6);

run_tests();

__DATA__

=== TEST 1: get_http_request_headers() gets request headers
should produce logs with headers, yield content production
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls;
        echo ok;
    }
--- request
GET /t/log_http_request_headers
--- more_headers
Hello: world
--- response_body
ok
--- error_log eval
[
    qr/\[wasm\] #\d+ -> Host: localhost/,
    qr/\[wasm\] #\d+ -> Connection: close/,
    qr/\[wasm\] #\d+ -> Hello: world/
]
--- no_error_log
[alert]



=== TEST 2: get_http_request_headers() many headers
should produce a response with 20+ headers
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls;
        echo fail;
    }
--- request
GET /t/echo/headers
--- more_headers eval
CORE::join "\n", map { "Header$_: value-$_" } 1..20
--- response_body eval
qq{Host: localhost\r
Connection: close\r
}.(CORE::join "\r\n", map { "Header$_: value-$_" } 1..20)
--- no_error_log
[warn]
[error]
[emerg]
[crit]



=== TEST 3: get_http_request_headers() too many headers
should produce a response but truncate number of headers if > 100
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls;
        echo fail;
    }
--- request
GET /t/echo/headers
--- more_headers eval
CORE::join "\n", map { "Header$_: value-$_" } 1..105
--- response_body eval
qq{Host: localhost\r
Connection: close\r
}.(CORE::join "\r\n", map { "Header$_: value-$_" } 1..98)
--- error_log eval
[
    qr/\[warn\] .*? truncated request headers map to 100 elements/,
    qr/\[info\] .*? number of request headers: 107/
]
--- no_error_log
[error]
[emerg]



=== TEST 4: get_http_request_headers() pipelined requests with large headers
should produce a response with headers
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls;
        echo fail;
    }
--- pipelined_requests eval
["GET /t/echo/headers", "GET /t/echo/headers"]
--- more_headers eval
CORE::join "\n", map { "Header$_: value-$_" } 1..105
--- response_body eval
[qq{Host: localhost\r
Connection: keep-alive\r
}.(CORE::join "\r\n", map { "Header$_: value-$_" } 1..98),
qq{Host: localhost\r
Connection: close\r
}.(CORE::join "\r\n", map { "Header$_: value-$_" } 1..98)]
--- no_error_log
[error]
