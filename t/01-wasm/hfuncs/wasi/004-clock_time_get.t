# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

plan tests => repeat_each() * (blocks() * 2);

run_tests();

__DATA__

=== TEST 1: clock_time_get via std::time::SystemTime
--- wasm_modules: ngx_rust_tests
--- config
    location /t {
        wasm_call rewrite ngx_rust_tests test_wasi_clock_time_get_via_systemtime;
    }
--- error_code: 200
--- response_body eval
qr/seconds since Unix epoch: [1-9][0-9]{9}\n/



=== TEST 2: clock_time_get via std::time::Instant
--- wasm_modules: ngx_rust_tests
--- config
    location /t {
        wasm_call rewrite ngx_rust_tests test_wasi_clock_time_get_via_instant;
    }
--- error_code: 204
--- no_error_log
[error]



=== TEST 3: clock_time_get
--- wasm_modules: ngx_rust_tests
--- config
    location /t {
        wasm_call rewrite ngx_rust_tests test_wasi_clock_time_get;
    }
--- error_code: 200
--- response_body eval
qr/test passed with timestamp: [0-9]+\n/



=== TEST 4: clock_time_get using an unsupported clock
--- wasm_modules: ngx_rust_tests
--- config
    location /t {
        wasm_call rewrite ngx_rust_tests test_wasi_clock_time_get_unsupported;
    }
--- error_code: 204
--- no_error_log
[error]
