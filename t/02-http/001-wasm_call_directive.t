# vim:set ft= ts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

plan tests => repeat_each() * (blocks() * 4);

run_tests();

__DATA__

=== TEST 1: wasm_call directive - no wasm{} configuration block
--- main_config
--- config
    location /t {
        wasm_call log a nop;
        return 200;
    }
--- error_log eval
qr/\[emerg\] .*? "wasm_call" directive is specified but config has no "wasm" section/
--- no_error_log
[error]
[crit]
--- must_die



=== TEST 2: wasm_call directive - invalid number of arguments
--- config
    location /t {
        wasm_call log nop;
        return 200;
    }
--- error_log eval
qr/\[emerg\] .*? invalid number of arguments in "wasm_call" directive/
--- no_error_log
[error]
[crit]
--- must_die



=== TEST 3: wasm_call directive - empty phase
--- config
    location /t {
        wasm_call '' a nop;
        return 200;
    }
--- error_log eval
qr/\[emerg\] .*? invalid phase ""/
--- no_error_log
[error]
[crit]
--- must_die



=== TEST 4: wasm_call directive - empty module name
--- config
    location /t {
        wasm_call log '' nop;
        return 200;
    }
--- error_log eval
qr/\[emerg\] .*? invalid module name ""/
--- no_error_log
[error]
[crit]
--- must_die



=== TEST 5: wasm_call directive - empty function name
--- config
    location /t {
        wasm_call log a '';
        return 200;
    }
--- error_log eval
qr/\[emerg\] .*? invalid function name ""/
--- no_error_log
[error]
[crit]
--- must_die



=== TEST 6: wasm_call directive - unknown phase
--- main_config
    wasm {
        module a $TEST_NGINX_HTML_DIR/a.wat;
    }
--- config
    location /t {
        wasm_call foo a nop;
        return 200;
    }
--- error_log eval
qr/\[emerg\] .*? unknown phase "foo"/
--- no_error_log
[error]
[crit]
--- must_die



=== TEST 7: wasm_call directive - NYI phase
--- main_config
    wasm {
        module a $TEST_NGINX_HTML_DIR/a.wat;
    }
--- config
    location /t {
        wasm_call post_read a nop;
        return 200;
    }
--- error_log eval
qr/\[emerg\] .*? unsupported phase "post_read"/
--- no_error_log
[error]
[crit]
--- must_die



=== TEST 8: wasm_call directive - no such module
--- config
    location /t {
        wasm_call log a nop;
        return 200;
    }
--- error_log eval
qr/\[emerg\] .*? \[wasm\] no "a" module defined/
--- no_error_log
[error]
[crit]
--- must_die



=== TEST 9: wasm_call directive - no such function
--- main_config
    wasm {
        module a $TEST_NGINX_HTML_DIR/a.wat;
    }
--- config
    location /t {
        wasm_call log a nonexist;
        return 200;
    }
--- user_files
>>> a.wat
(module
  (func $nop)
  (export "nop" (func $nop))
)
--- error_log eval
qr/\[emerg\] .*? \[wasm\] no "nonexist" function in "a" module/
--- no_error_log
[error]
[crit]



=== TEST 10: wasm_call directive - catch runtime error sanity
--- main_config
    wasm {
        module a $TEST_NGINX_HTML_DIR/a.wat;
    }
--- config
    location /t {
        wasm_call log a div0;
        return 200;
    }
--- user_files
>>> a.wat
(module
    (func (export "div0")
        i32.const 0
        i32.const 0
        i32.div_u
        drop)
)
--- error_log eval
qr/\[error\] .*? \[wasm\] (?:wasm trap\: )?integer divide by zero/
--- no_error_log
[emerg]
[crit]



=== TEST 11: wasm_call directive - sanity 'rewrite' phase in location{} block
--- skip_no_debug: 4
--- main_config
    wasm {
        module a $TEST_NGINX_HTML_DIR/a.wat;
    }
--- config
    location /t {
        wasm_call rewrite a nop;
        return 200;
    }
--- user_files
>>> a.wat
(module
  (func $nop)
  (export "nop" (func $nop))
)
--- error_log
wasm ops calling "a.nop" in "rewrite" phase
--- no_error_log
[error]
[emerg]



=== TEST 12: wasm_call directive - sanity 'content' phase in location{} block
--- SKIP
--- skip_no_debug: 4
--- main_config
    wasm {
        module a $TEST_NGINX_HTML_DIR/a.wat;
    }
--- config
    location /t {
        wasm_call content a nop;
    }
--- user_files
>>> a.wat
(module
  (func $nop)
  (export "nop" (func $nop))
)
--- error_code: 404
--- error_log
wasm ops calling "a.nop" in "content" phase
--- no_error_log
[emerg]



=== TEST 13: wasm_call directive - sanity 'log' phase in location{} block
--- skip_no_debug: 4
--- main_config
    wasm {
        module a $TEST_NGINX_HTML_DIR/a.wat;
    }
--- config
    location /t {
        wasm_call log a nop;
        return 200;
    }
--- user_files
>>> a.wat
(module
  (func $nop)
  (export "nop" (func $nop))
)
--- error_log
wasm ops calling "a.nop" in "log" phase
--- no_error_log
[error]
[emerg]



=== TEST 14: wasm_call directive - multiple calls in location{} block
--- skip_no_debug: 4
--- main_config
    wasm {
        module a $TEST_NGINX_HTML_DIR/a.wat;
    }
--- config
    location /t {
        wasm_call log a nop;
        wasm_call log a nop;
        return 200;
    }
--- user_files
>>> a.wat
(module
  (func $nop)
  (export "nop" (func $nop))
)
--- error_log
wasm ops calling "a.nop" in "log" phase
wasm ops calling "a.nop" in "log" phase
--- no_error_log
[error]



=== TEST 15: wasm_call directive - multiple calls in server{} block
--- skip_no_debug: 4
--- main_config
    wasm {
        module a $TEST_NGINX_HTML_DIR/a.wat;
    }
--- config
    wasm_call log a nop;
    wasm_call log a nop;

    location /t {
        return 200;
    }
--- user_files
>>> a.wat
(module
  (func $nop)
  (export "nop" (func $nop))
)
--- error_log
wasm ops calling "a.nop" in "log" phase
wasm ops calling "a.nop" in "log" phase
--- no_error_log
[error]



=== TEST 16: wasm_call directive - multiple calls in http{} block
--- skip_no_debug: 4
--- main_config
    wasm {
        module a $TEST_NGINX_HTML_DIR/a.wat;
    }
--- http_config
    wasm_call log a nop;
    wasm_call log a nop;
--- config
    location /t {
        return 200;
    }
--- user_files
>>> a.wat
(module
  (func $nop)
  (export "nop" (func $nop))
)
--- error_log
wasm ops calling "a.nop" in "log" phase
wasm ops calling "a.nop" in "log" phase
--- no_error_log
[error]



=== TEST 17: wasm_call directive - mixed server{} and location{} blocks
--- skip_no_debug: 4
--- main_config
    wasm {
        module a $TEST_NGINX_HTML_DIR/a.wat;
    }
--- config
    wasm_call log a nop;

    location /t {
        wasm_call log a nop2;
        return 200;
    }
--- user_files
>>> a.wat
(module
  (func $nop)
  (export "nop" (func $nop))
  (export "nop2" (func $nop))
)
--- error_log
wasm ops calling "a.nop2" in "log" phase
--- no_error_log
wasm ops calling "a.nop" in "log" phase
[error]



=== TEST 18: wasm_call directive - multiple modules/calls/phases/blocks
--- skip_no_debug: 4
--- main_config
    wasm {
        module moduleA $TEST_NGINX_HTML_DIR/a.wat;
        module moduleB $TEST_NGINX_HTML_DIR/a.wat;
    }
--- http_config
    wasm_call rewrite moduleA nop2;
    wasm_call rewrite moduleB nop;
--- config
    wasm_call rewrite moduleA nop;
    wasm_call log moduleA nop2;
    wasm_call log moduleB nop2;

    location /t {
        wasm_call log moduleB nop3;
        return 200;
    }
--- user_files
>>> a.wat
(module
  (func $nop)
  (export "nop" (func $nop))
  (export "nop2" (func $nop))
  (export "nop3" (func $nop))
)
--- error_log
wasm ops calling "moduleA.nop" in "rewrite" phase
wasm ops calling "moduleB.nop3" in "log" phase
--- no_error_log eval
qr/\[wasm\] calling "module[A|B]\.nop2" in "log" phase/
