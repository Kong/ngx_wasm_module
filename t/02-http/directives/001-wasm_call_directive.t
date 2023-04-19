# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:;

use strict;
use lib '.';
use t::TestWasm;

plan tests => repeat_each() * (blocks() * 4);

run_tests();

__DATA__

=== TEST 1: wasm_call directive - no wasm{} configuration block
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
--- main_config
    wasm {}
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
--- main_config
    wasm {}
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
--- main_config
    wasm {}
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
--- main_config
    wasm {}
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
qr/\[alert\] .*? unsupported phase "post_read"/
--- no_error_log
[error]
[crit]
--- must_die



=== TEST 8: wasm_call directive - no such module
--- main_config
    wasm {}
--- config
    location /t {
        wasm_call log a nop;
        return 200;
    }
--- error_log eval
qr/\[emerg\] .*? no "a" module defined/
--- no_error_log
[error]
[crit]
--- must_die



=== TEST 9: wasm_call directive - no such function in log phase
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
  (export "nop" (func $nop)))
--- error_log eval
qr/\[error\] .*? no "nonexist" function in "a" module while logging request/
--- no_error_log
[crit]
[emerg]



=== TEST 10: wasm_call directive - no such function in rewrite phase
--- main_config
    wasm {
        module a $TEST_NGINX_HTML_DIR/a.wat;
    }
--- config
    location /t {
        wasm_call rewrite a nonexist;
        return 200;
    }
--- user_files
>>> a.wat
(module
  (func $nop)
  (export "nop" (func $nop)))
--- error_code: 500
--- error_log eval
qr/\[error\] .*? no "nonexist" function in "a" module/
--- no_error_log
[crit]
[emerg]



=== TEST 11: wasm_call directive - catch runtime error sanity
--- main_config
    wasm {
        module a $TEST_NGINX_HTML_DIR/a.wat;
    }
--- config
    location /t {
        wasm_call rewrite a div0;
        return 200;
    }
--- user_files
>>> a.wat
(module
    (func (export "div0")
        i32.const 0
        i32.const 0
        i32.div_u
        drop))
--- error_code: 500
--- error_log eval
qr/(\[error\] .*? integer divide by zero|wasm trap: integer divide by zero|Uncaught RuntimeError: divide by zero)/
--- no_error_log
[crit]
[alert]



=== TEST 12: wasm_call directive - sanity 'rewrite' phase in location{} block
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
  (export "nop" (func $nop)))
--- error_log
wasm ops calling "a.nop" in "rewrite" phase
--- no_error_log
[error]
[crit]



=== TEST 13: wasm_call directive - sanity 'content' phase in location{} block
--- skip_no_debug: 4
--- load_nginx_modules: ngx_http_echo_module
--- main_config
    wasm {
        module a $TEST_NGINX_HTML_DIR/a.wat;
    }
--- config
    location /t {
        wasm_call content a nop;
        echo ok;
    }
--- user_files
>>> a.wat
(module
  (func $nop)
  (export "nop" (func $nop)))
--- error_log
wasm ops calling "a.nop" in "content" phase
--- no_error_log
[error]
[crit]



=== TEST 14: wasm_call directive - sanity 'log' phase in location{} block
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
  (export "nop" (func $nop)))
--- error_log
wasm ops calling "a.nop" in "log" phase
--- no_error_log
[error]
[crit]



=== TEST 15: wasm_call directive - multiple calls in location{} block
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
  (export "nop" (func $nop)))
--- error_log
wasm ops calling "a.nop" in "log" phase
wasm ops calling "a.nop" in "log" phase
--- no_error_log
[error]



=== TEST 16: wasm_call directive - multiple calls in server{} block
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
  (export "nop" (func $nop)))
--- error_log
wasm ops calling "a.nop" in "log" phase
wasm ops calling "a.nop" in "log" phase
--- no_error_log
[error]



=== TEST 17: wasm_call directive - multiple calls in http{} block
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
  (export "nop" (func $nop)))
--- error_log
wasm ops calling "a.nop" in "log" phase
wasm ops calling "a.nop" in "log" phase
--- no_error_log
[error]



=== TEST 18: wasm_call directive - mixed server{} and location{} blocks
--- skip_no_debug: 4
--- main_config
    wasm {
        module a $TEST_NGINX_HTML_DIR/a.wat;
    }
--- config
    wasm_call log a nocall;

    location /t {
        wasm_call log a nop;
        return 200;
    }
--- user_files
>>> a.wat
(module
  (func $nop)
  (export "nop" (func $nop))
  (export "nocall" (func $nop)))
--- error_log
wasm ops calling "a.nop" in "log" phase
--- no_error_log
wasm ops calling "a.nocall" in "log" phase
[error]



=== TEST 19: wasm_call directive - multiple modules/calls/phases/blocks
--- skip_no_debug: 4
--- main_config
    wasm {
        module moduleA $TEST_NGINX_HTML_DIR/a.wat;
        module moduleB $TEST_NGINX_HTML_DIR/a.wat;
    }
--- http_config
    wasm_call rewrite moduleA nocall;
    wasm_call rewrite moduleB nop;
--- config
    wasm_call rewrite moduleA nop;
    wasm_call log moduleA nocall;
    wasm_call log moduleB nocall;

    location /t {
        wasm_call log moduleB othernop;
        return 200;
    }
--- user_files
>>> a.wat
(module
  (func $nop)
  (export "nop" (func $nop))
  (export "nocall" (func $nop))
  (export "othernop" (func $nop)))
--- error_log
wasm ops calling "moduleB.othernop" in "log" phase
--- no_error_log eval
[
    qr/calling "module[A|B]\.nocall" in "log" phase/,
    qr/calling "moduleA\.nop" in "rewrite" phase/,
]
