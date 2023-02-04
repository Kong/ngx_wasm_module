# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

plan tests => repeat_each() * (blocks() * 3);

run_tests();

__DATA__

=== TEST 1: clock_time_get via std::time::SystemTime
--- wasm_modules: wasi_host_tests
--- config
    location /t {
        wasm_call rewrite wasi_host_tests test_wasi_clock_time_get_via_systemtime;
    }
--- error_code: 200
--- response_body eval
qr/seconds since Unix epoch: [1-9][0-9]{9}\n/
--- no_error_log
[error]



=== TEST 2: clock_time_get via std::time::Instant
--- wasm_modules: wasi_host_tests
--- config
    location /t {
        wasm_call rewrite wasi_host_tests test_wasi_clock_time_get_via_instant;
    }
--- response_body
test passed
--- no_error_log
[error]



=== TEST 3: clock_time_get
--- wasm_modules: wasi_host_tests
--- config
    location /t {
        wasm_call rewrite wasi_host_tests test_wasi_clock_time_get;
    }
--- response_body eval
qr/test passed with timestamp: [0-9]+\n/
--- no_error_log
[error]



=== TEST 4: clock_time_get using an unsupported clock
--- wasm_modules: wasi_host_tests
--- config
    location /t {
        wasm_call rewrite wasi_host_tests test_wasi_clock_time_get_unsupported;
    }
--- response_body
test passed
--- no_error_log
[error]
