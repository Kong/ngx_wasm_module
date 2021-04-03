# vim:set ft= ts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

skip_valgrind();

plan tests => repeat_each() * (blocks() * 5);

run_tests();

__DATA__

=== TEST 1: proxy_wasm - get_http_request_headers() sanity
--- wasm_modules: hostcalls
--- load_nginx_modules: ngx_http_echo_module
--- config
    location /t {
        proxy_wasm hostcalls;
        echo ok;
    }
--- request
GET /t/log_http_request_headers
--- more_headers
Hello: world
--- ignore_response_body
--- error_log eval
[
    qr/\[wasm\] #\d+ -> Host: localhost/,
    qr/\[wasm\] #\d+ -> Connection: close/,
    qr/\[wasm\] #\d+ -> Hello: world/
]
--- no_error_log
[alert]
