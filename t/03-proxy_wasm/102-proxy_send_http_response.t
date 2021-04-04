# vim:set ft= ts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

skip_valgrind();

plan tests => repeat_each() * (blocks() * 4);

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
[error]
[alert]



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
    qr/\[error\] .*? \[wasm\] (?:wasm trap\:)?unreachable/,
]



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
[error]
[alert]
