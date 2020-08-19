# vim:set ft= ts=4 sw=4 et fdm=marker:
use lib '.';
use t::TestWasm;

plan tests => repeat_each() * (blocks() * 3 + 1);

run_tests();

__DATA__

=== TEST 1: wasm_call directive: no wasm{} configuration block
--- main_config
--- config
    location /t {
        wasm_call log hello nop;
        return 200;
    }
--- error_log eval
qr/\[emerg\] .*? no "wasm" section in configuration/
--- must_die



=== TEST 2: wasm_call directive: invalid number of arguments
--- config
    location /t {
        wasm_call log nop;
        return 200;
    }
--- error_log eval
qr/\[emerg\] .*? invalid number of arguments in "wasm_call" directive/
--- must_die



=== TEST 3: wasm_call directive: empty phase
--- config
    location /t {
        wasm_call '' hello nop;
        return 200;
    }
--- error_log eval
qr/\[emerg\] .*? invalid phase ""/
--- must_die



=== TEST 4: wasm_call directive: empty module name
--- config
    location /t {
        wasm_call log '' nop;
        return 200;
    }
--- error_log eval
qr/\[emerg\] .*? invalid module name ""/
--- must_die



=== TEST 5: wasm_call directive: empty function name
--- config
    location /t {
        wasm_call log hello '';
        return 200;
    }
--- error_log eval
qr/\[emerg\] .*? invalid function name ""/
--- must_die



=== TEST 6: wasm_call directive: unknown phase
--- config
    location /t {
        wasm_call foo hello nop;
        return 200;
    }
--- error_log eval
qr/\[emerg\] .*? unknown phase "foo"/
--- must_die



=== TEST 7: wasm_call directive: NYI phase
--- config
    location /t {
        wasm_call post_read hello nop;
        return 200;
    }
--- error_log eval
qr/\[alert\] .*? unsupported phase "post_read"/
--- must_die



=== TEST 8: wasm_call directive: no such module
--- config
    location /t {
        wasm_call log hello nop;
        return 200;
    }
--- error_log eval
qr/\[emerg\] .*? \[wasm\] no "hello" module defined/
--- must_die



=== TEST 9: wasm_call directive: no such function
--- main_config
    wasm {
        module hello $TEST_NGINX_HTML_DIR/hello.wat;
    }
--- config
    location /t {
        wasm_call log hello nonexist;
        return 200;
    }
--- user_files
>>> hello.wat
(module
  (func $nop)
  (export "nop" (func $nop))
)
--- error_log eval
qr/\[error\] .*? \[wasm\] no "nonexist" function defined in "hello" module <vm: .*?, runtime: .*?>/
--- no_error_log
[emerg]



=== TEST 10: wasm_call directive: catch runtime error sanity
--- main_config
    wasm {
        module hello $TEST_NGINX_HTML_DIR/hello.wat;
    }
--- config
    location /t {
        wasm_call log hello div0;
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



=== TEST 11: wasm_call directive: sanity in 'log' phase
--- skip_no_debug: 3
--- main_config
    wasm {
        module hello $TEST_NGINX_HTML_DIR/hello.wat;
    }
--- config
    location /t {
        wasm_call log hello nop;
        return 200;
    }
--- user_files
>>> hello.wat
(module
  (func $nop)
  (export "nop" (func $nop))
)
--- error_log
wasm: calling "hello.nop" in "log" phase



=== TEST 12: wasm_call directive: sanity in 'rewrite' phase in location block
--- skip_no_debug: 3
--- main_config
    wasm {
        module hello $TEST_NGINX_HTML_DIR/hello.wat;
    }
--- config
    location /t {
        wasm_call rewrite hello nop;
        return 200;
    }
--- user_files
>>> hello.wat
(module
  (func $nop)
  (export "nop" (func $nop))
)
--- error_log
wasm: calling "hello.nop" in "rewrite" phase



=== TEST 13: wasm_call directive: multiple calls in a location block
--- skip_no_debug: 3
--- main_config
    wasm {
        module hello $TEST_NGINX_HTML_DIR/hello.wat;
    }
--- config
    location /t {
        wasm_call log hello nop;
        wasm_call log hello nop;
        return 200;
    }
--- user_files
>>> hello.wat
(module
  (func $nop)
  (export "nop" (func $nop))
)
--- error_log
wasm: calling "hello.nop" in "log" phase
wasm: calling "hello.nop" in "log" phase
--- no_error_log



=== TEST 14: wasm_call directive: multiple calls in main block
--- skip_no_debug: 3
--- main_config
    wasm {
        module hello $TEST_NGINX_HTML_DIR/hello.wat;
    }
--- config
    wasm_call log hello nop;
    wasm_call log hello nop;

    location /t {
        return 200;
    }
--- user_files
>>> hello.wat
(module
  (func $nop)
  (export "nop" (func $nop))
)
--- error_log
wasm: calling "hello.nop" in "log" phase
wasm: calling "hello.nop" in "log" phase
--- no_error_log



=== TEST 15: wasm_call directive: mixed main and location blocks
--- skip_no_debug: 3
--- main_config
    wasm {
        module hello $TEST_NGINX_HTML_DIR/hello.wat;
    }
--- config
    wasm_call log hello nop;

    location /t {
        wasm_call log hello nop2;
        return 200;
    }
--- user_files
>>> hello.wat
(module
  (func $nop)
  (export "nop" (func $nop))
  (export "nop2" (func $nop))
)
--- error_log
wasm: calling "hello.nop2" in "log" phase
--- no_error_log
wasm: calling "hello.nop" in "log" phase



=== TEST 16: wasm_call directive: multiple modules, multiple calls, multiple phases, multiple blocks
--- skip_no_debug: 4
--- main_config
    wasm {
        module moduleA $TEST_NGINX_HTML_DIR/hello.wat;
        module moduleB $TEST_NGINX_HTML_DIR/hello.wat;
    }
--- config
    wasm_call rewrite moduleA nop;
    wasm_call log moduleA nop2;
    wasm_call log moduleB nop2;

    location /t {
        wasm_call log moduleB nop3;
        return 200;
    }
--- user_files
>>> hello.wat
(module
  (func $nop)
  (export "nop" (func $nop))
  (export "nop2" (func $nop))
  (export "nop3" (func $nop))
)
--- error_log
wasm: calling "moduleA.nop" in "rewrite" phase
wasm: calling "moduleB.nop3" in "log" phase
--- no_error_log eval
qr/wasm: calling "module[A|B]\.nop" in "log" phase/
