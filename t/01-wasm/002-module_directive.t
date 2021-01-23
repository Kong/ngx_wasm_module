# vim:set ft= ts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

plan tests => repeat_each() * (blocks() * 4);

run_tests();

__DATA__

=== TEST 1: module directive - sanity with .wat module
--- main_config
    wasm {
        module hello $TEST_NGINX_HTML_DIR/hello.wat;
    }
--- user_files
>>> hello.wat
(module)
--- error_log eval
qr/\[info\] .*? \[wasm\] loading "hello" module from ".*?hello\.wat" <vm: "main", runtime: ".*?">/
--- no_error_log
[error]
[emerg]



=== TEST 2: module directive - sanity with .wasm module
--- main_config eval
qq{
    wasm {
        module hello $t::TestWasm::crates/rust_http_tests.wasm;
    }
}
--- error_log eval
qr/\[info\] .*? \[wasm\] loading "hello" module from ".*?rust_http_tests\.wasm" <vm: "main", runtime: ".*?">/
--- no_error_log
[error]
[emerg]



=== TEST 3: module directive - multiple modules
--- main_config
    wasm {
        module hello $TEST_NGINX_HTML_DIR/hello.wat;
        module world $TEST_NGINX_HTML_DIR/hello.wat;
    }
--- user_files
>>> hello.wat
(module)
--- error_log eval
[qr/\[info\] .*? \[wasm\] loading "hello" module from ".*?hello\.wat"/,
qr/\[info\] .*? \[wasm\] loading "world" module from ".*?hello\.wat"/]
--- no_error_log
[emerg]



=== TEST 4: module directive - invalid arguments
--- main_config
    wasm {
        module $TEST_NGINX_HTML_DIR/hello.wat;
    }
--- error_log eval
qr/\[emerg\] .*? invalid number of arguments in "module" directive/
--- no_error_log
[error]
[crit]
--- must_die



=== TEST 5: module directive - bad name
--- main_config
    wasm {
        module '' $TEST_NGINX_HTML_DIR/hello.wat;
    }
--- error_log eval
qr/\[emerg\] .*? invalid module name ""/
--- no_error_log
[error]
[crit]
--- must_die



=== TEST 6: module directive - bad path
--- main_config
    wasm {
        module hello '';
    }
--- error_log eval
qr/\[emerg\] .*? invalid module path ""/
--- no_error_log
[error]
[crit]
--- must_die



=== TEST 7: module directive - already defined
--- main_config
    wasm {
        module hello $TEST_NGINX_HTML_DIR/hello.wat;
        module hello $TEST_NGINX_HTML_DIR/hello.wat;
    }
--- user_files
>>> hello.wat
(module)
--- error_log eval
qr/\[emerg\] .*? module "hello" already defined/
--- no_error_log
[error]
[crit]
--- must_die



=== TEST 8: module directive - no such path
--- main_config
    wasm {
        module hello $TEST_NGINX_HTML_DIR/none.wat;
    }
--- error_log eval
qr/\[emerg\] .*? \[wasm\] open\(\) ".*?none\.wat" failed \(2: No such file or directory\)/
--- no_error_log
[error]
[crit]
--- must_die



=== TEST 9: module directive - invalid .wat
--- main_config
    wasm {
        module hello $TEST_NGINX_HTML_DIR/hello.wat;
    }
--- user_files
>>> hello.wat
--- error_log eval
qr/\[emerg\] .*? \[wasm\] failed to load "hello" module:/
--- no_error_log
[error]
[crit]
--- must_die



=== TEST 10: module directive - invalid .wasm
--- main_config
    wasm {
        module hello $TEST_NGINX_HTML_DIR/hello.wasm;
    }
--- user_files
>>> hello.wasm
--- error_log eval
qr/\[emerg\] .*? \[wasm\] failed to load "hello" module:/
--- no_error_log
[error]
[crit]
--- must_die
