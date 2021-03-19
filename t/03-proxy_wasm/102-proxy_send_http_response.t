# vim:set ft= ts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

skip_valgrind();

plan tests => repeat_each() * (blocks() * 4);

run_tests();

__DATA__

=== TEST 1: proxy_wasm - send_http_response() status code
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls;
    }
--- request
GET /t/send_http_response/status
--- error_code: 204
--- response_body eval: ""
--- no_error_log
[error]
[alert]
