# vim:set ft= ts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

skip_valgrind();

plan tests => repeat_each() * (blocks() * 8);

run_tests();

__DATA__

=== TEST 1: proxy_wasm - add_http_request_header() adds a request header
should add a 3rd request header visible in 2nd filter
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test_case=/t/add_http_request_header/sanity';
        proxy_wasm hostcalls 'test_case=/t/log/request_headers';
        return 200;
    }
--- response_body
--- error_log eval
[
    qr/\[wasm\] #\d+ on_request_headers, 2 headers/,
    qr/\[wasm\] #\d+ on_request_headers, 3 headers/,
    qr/\[wasm\] Host: localhost/,
    qr/\[wasm\] Connection: close/,
    qr/\[wasm\] Test-Case: from-add-request-header/
]
--- no_error_log
[error]
