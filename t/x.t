# vim:set ft= ts=4 sw=4 et fdm=marker:
use lib '.';
use t::TestWasm;

plan tests => repeat_each() * (blocks() * 1);

run_tests();

__DATA__

=== TEST 1: wasm_call_log directive: sanity
--- main_config
    wasm {
        module http_tests t/lib/rust-http-tests/target/wasm32-unknown-unknown/debug/rust_tests.wasm;
    }
--- config
    location /t {
        #wasm_call_log http_tests _start;
        return 200;
    }
--- no_error_log
[error]
