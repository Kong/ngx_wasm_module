# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

plan tests => repeat_each() * (blocks() * 2);

run_tests();

__DATA__

=== TEST 1: fd_write stdout
--- wasm_modules: ngx_rust_tests
--- config
    location /t {
        wasm_call rewrite ngx_rust_tests test_wasi_fd_write_stdout;
    }
--- error_code: 204
--- error_log eval
[
    qr/\[info\] .*? hello, fd_write/,
]



=== TEST 2: fd_write stderr
--- wasm_modules: ngx_rust_tests
--- config
    location /t {
        wasm_call rewrite ngx_rust_tests test_wasi_fd_write_stderr;
    }
--- error_code: 204
--- error_log eval
[
    qr/\[error\] .*? hello, fd_write/,
]



=== TEST 3: fd_write via println
--- wasm_modules: ngx_rust_tests
--- config
    location /t {
        wasm_call rewrite ngx_rust_tests test_wasi_fd_write_via_println;
    }
--- error_code: 204
--- error_log eval
[
    qr/\[info\] .*? Hello, println/,
]



=== TEST 4: fd_write to an unsupported fd
--- wasm_modules: ngx_rust_tests
--- config
    location /t {
        wasm_call rewrite ngx_rust_tests test_wasi_fd_write_unsupported_fd;
    }
--- error_code: 204
--- no_error_log
hello, fd_write



=== TEST 5: fd_write an empty string
--- wasm_modules: ngx_rust_tests
--- config
    location /t {
        wasm_call rewrite ngx_rust_tests test_wasi_fd_write_empty_string;
    }
--- error_code: 204
--- no_error_log
hello, fd_write
