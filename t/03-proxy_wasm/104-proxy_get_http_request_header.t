# vim:set ft= ts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

skip_valgrind();

plan tests => repeat_each() * (blocks() * 8);

run_tests();

__DATA__

=== TEST 1: proxy_wasm - get_http_request_header() retrieves case insensitive header
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls;
    }
--- request eval
[
    "GET /t/echo/header/Hello",
    "GET /t/echo/header/hello"
]
--- more_headers
Hello: world
--- response_body eval
[
    "Hello: world",
    "hello: world"
]
--- no_error_log
[error]
[alert]



=== TEST 2: proxy_wasm - get_http_request_header() retrieves a missing header
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls;
    }
--- request
GET /t/echo/header/Some-Header
--- response_body chomp
Some-Header:
--- no_error_log
[warn]
[error]
[crit]
[emerg]
[alert]
[stderr]



=== TEST 3: proxy_wasm - get_http_request_header() retrieves ':method'
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls;
    }
--- request eval
[
    "GET /t/echo/header/:method",
    "POST /t/echo/header/:method"
]
--- response_body eval
[
    ":method: GET",
    ":method: POST"
]
--- no_error_log
[error]
[alert]



=== TEST 4: proxy_wasm - get_http_request_header() retrieves ':path'
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls;
    }
--- request
GET /t/echo/header/:path
--- response_body chomp
:path: /t/echo/header/:path
--- no_error_log
[warn]
[error]
[emerg]
[alert]
[crit]
[stderr]



=== TEST 5: proxy_wasm - get_http_request_header() retrieves ':path' from r->uri (parsed)
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls;
    }
--- request
GET //t/echo//header/:path
--- response_body chomp
:path: /t/echo/header/:path
--- no_error_log
[warn]
[error]
[emerg]
[alert]
[crit]
[stderr]



=== TEST 6: proxy_wasm - get_http_request_header() retrieves ':path' as a subrequest
should respond with the subrequest's path (/t/echo/header/...) and not the main
request's path (/t)
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t/echo/header/ {
        proxy_wasm hostcalls;
    }

    location /t {
        echo_subrequest GET /t/echo/header/:path;
    }
--- request
GET /t
--- response_body chomp
:path: /t/echo/header/:path
--- no_error_log
[warn]
[error]
[crit]
[emerg]
[alert]
[stderr]
