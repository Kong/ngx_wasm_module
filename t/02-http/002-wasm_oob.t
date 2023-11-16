# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

plan_tests(4);
run_tests();

__DATA__

=== TEST 1: wasm - oob memory read
--- main_config
    wasm {
        module a $TEST_NGINX_HTML_DIR/a.wat;
    }
--- config
    location /t {
        wasm_call rewrite a oob_read;
        return 200;
    }
--- user_files
>>> a.wat
(module
    (memory (;0;) 25)
    (func (export "oob_read")
        i32.const 0x20000000
        i32.load
        drop)
)
--- error_code: 500
--- error_log eval
qr/(\[error\] .*? out of bounds|wasm trap: out of bounds memory access|Uncaught RuntimeError: memory access out of bounds)/
--- no_error_log
[crit]
[alert]



=== TEST 2: wasm - oob memory write
--- main_config
    wasm {
        module a $TEST_NGINX_HTML_DIR/a.wat;
    }
--- config
    location /t {
        wasm_call rewrite a oob_write;
        return 200;
    }
--- user_files
>>> a.wat
(module
    (memory (;0;) 25)
    (func (export "oob_write")
        i32.const 0x20000000
        i32.const 42
        i32.store)
)
--- error_code: 500
--- error_log eval
qr/(\[error\] .*? out of bounds|wasm trap: out of bounds|Uncaught RuntimeError: memory access out of bounds)/
--- no_error_log
[crit]
[alert]
