# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

plan tests => repeat_each() * (blocks() * 3);

run_tests();

__DATA__

=== TEST 1: flush_local_response() set status + reason
--- wasm_modules: ngx_rust_tests
--- config
    location /t {
        wasm_call rewrite ngx_rust_tests local_reason;
    }
--- error_code: 201
--- raw_response_headers_like
HTTP/1.1 201 REASON\s
--- no_error_log
[error]
