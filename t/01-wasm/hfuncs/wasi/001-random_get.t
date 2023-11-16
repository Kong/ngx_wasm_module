# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

plan_tests(3);
run_tests();

__DATA__

=== TEST 1: random_get
--- wasm_modules: wasi_host_tests
--- config
    location /t {
        wasm_call rewrite wasi_host_tests test_wasi_random_get;
    }
--- response_body
test passed
--- no_error_log
[error]
