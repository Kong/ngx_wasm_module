# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

plan tests => repeat_each() * (blocks() * 2);

run_tests();

__DATA__

=== TEST 1: fd_prestat_dir_name stub
--- wasm_modules: ngx_rust_tests
--- config
    location /t {
        wasm_call rewrite ngx_rust_tests test_wasi_fd_prestat_dir_name;
    }
--- error_code: 204
--- no_error_log
[error]
