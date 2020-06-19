# vim:set ft= ts=4 sw=4 et fdm=marker:
use lib '.';
use t::TestWasm;

plan tests => repeat_each() * (blocks() * 3);

add_block_preprocessor(sub {
    my $block = shift;

    if (!defined $block->no_error_log) {
        $block->set_value("no_error_log", "[error]");
    }
});

run_tests();

__DATA__

=== TEST 1: wasm_call_log directive: no wasm{} configuration block
--- main_config
--- config
    location /t {
        wasm_call_log hello get;
        return 200;
    }
--- error_log eval
qr/\[emerg\] .*? no "wasm" section in configuration/
--- must_die



=== TEST 2: wasm_call_log directive: sanity
--- main_config
    wasm {
        module hello $TEST_NGINX_HTML_DIR/hello.wat;
    }
--- config
    location /t {
        wasm_call_log hello get;
        return 200;
    }
--- user_files
>>> hello.wat
(module
  (func $get)
  (export "get" (func $get))
)
--- error_log
wasm: executing 1 call(s) in log phase



=== TEST 3: wasm_call_log directive: invalid module name
--- config
    location /t {
        wasm_call_log '' get;
        return 200;
    }
--- error_log eval
qr/\[emerg\] .*? invalid module name ""/
--- must_die



=== TEST 4: wasm_call_log directive: invalid function name
--- config
    location /t {
        wasm_call_log hello '';
        return 200;
    }
--- error_log eval
qr/\[emerg\] .*? invalid function name ""/
--- must_die



=== TEST 5: wasm_call_log directive: no such module
--- config
    location /t {
        wasm_call_log hello get;
        return 200;
    }
--- error_log eval
qr/\[emerg\] .*? \[wasm\] no "hello" module defined/
--- must_die



=== TEST 6: wasm_call_log directive: catch runtime error sanity
--- main_config
    wasm {
        module hello $TEST_NGINX_HTML_DIR/hello.wat;
    }
--- config
    location /t {
        wasm_call_log hello div0;
        return 200;
    }
--- user_files
>>> hello.wat
(module
    (func (export "div0")
        i32.const 0
        i32.const 0
        i32.div_u
        drop)
)
--- error_log eval
qr/\[error\] .*? \[wasm\] .*? wasm trap: integer divide by zero/
--- no_error_log
[emerg]



=== TEST 7: wasm_call_log directive: X
--- main_config
    wasm {
        module http_tests t/lib/rust-http-tests/target/wasm32-unknown-unknown/debug/rust_tests.wasm;
    }
--- config
    location /t {
        wasm_call_log http_tests my_func;
        return 200;
    }
--- no_error_log
[emerg]
[error]
