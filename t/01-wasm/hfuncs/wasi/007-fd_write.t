# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

plan tests => repeat_each() * (blocks() * 3);

run_tests();

__DATA__

=== TEST 1: fd_write stdout
--- wasm_modules: wasi_host_tests
--- config
    location /t {
        wasm_call rewrite wasi_host_tests test_wasi_fd_write_stdout;
    }
--- response_body
test passed, wrote 15 bytes
--- error_log eval
qr/\[info\] .*? hello, fd_write/



=== TEST 2: fd_write stderr
--- wasm_modules: wasi_host_tests
--- config
    location /t {
        wasm_call rewrite wasi_host_tests test_wasi_fd_write_stderr;
    }
--- response_body
test passed, wrote 15 bytes
--- error_log eval
qr/\[error\] .*? hello, fd_write/



=== TEST 3: fd_write via println
--- wasm_modules: wasi_host_tests
--- config
    location /t {
        wasm_call rewrite wasi_host_tests test_wasi_fd_write_via_println;
    }
--- response_body
test passed
--- error_log eval
qr/\[info\] .*? Hello, println/



=== TEST 4: fd_write to an unsupported fd
--- wasm_modules: wasi_host_tests
--- config
    location /t {
        wasm_call rewrite wasi_host_tests test_wasi_fd_write_unsupported_fd;
    }
--- response_body
test passed
--- no_error_log
hello, fd_write



=== TEST 5: fd_write an empty string
--- wasm_modules: wasi_host_tests
--- config
    location /t {
        wasm_call rewrite wasi_host_tests test_wasi_fd_write_empty_string;
    }
--- response_body
test passed
--- no_error_log
hello, fd_write
