# vim:set ft= ts=4 sw=4 et fdm=marker:
use lib '.';
use t::TestWasm;

plan tests => repeat_each() * (blocks() * 2);

run_tests();

__DATA__

=== TEST 1: wasm_module - empty path
--- config
    location /t {
        wasm_module '';
        return 200;
    }
--- error_log
no file specified
--- must_die



=== TEST 2: wasm_module - duplicated
--- config
    location /t {
        wasm_module 'a';
        wasm_module 'b';
        return 200;
    }
--- error_log
"wasm_module" directive is duplicate
--- must_die



=== TEST 3: wasm_module - invalid path
--- config
    location /t {
        wasm_module $TEST_NGINX_HTML_DIR/none.wat;
        return 200;
    }
--- error_log eval
qr/\[error\] .*? open\(\) ".*?none\.wat" .*? No such file or directory/
--- must_die



=== TEST 4: wasm_module - can load a .wat file
--- config
    location /t {
        wasm_module $TEST_NGINX_HTML_DIR/hello.wat;
        return 200;
    }
--- user_files
>>> hello.wat
(module)
--- error_log eval
qr/\[debug\] .*? loading wasm module at ".*?hello\.wat"/



=== TEST 5: wasm_module - invalid .wat file
--- config
    location /t {
        wasm_module $TEST_NGINX_HTML_DIR/hello.wat;
        return 200;
    }
--- user_files
>>> hello.wat
--- error_log eval
qr/\[error\] .*? \[wasmtime\] failed to compile \.wat module at ".*?hello\.wat" \(expected at least one module field/
--- must_die
