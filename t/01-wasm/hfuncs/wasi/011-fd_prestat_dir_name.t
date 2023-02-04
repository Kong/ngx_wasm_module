# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

plan tests => repeat_each() * (blocks() * 3);

run_tests();

__DATA__

=== TEST 1: fd_prestat_dir_name stub
--- wasm_modules: wasi_host_tests
--- config
    location /t {
        wasm_call rewrite wasi_host_tests test_wasi_fd_prestat_dir_name;
    }
--- response_body
test passed
--- no_error_log
[error]
