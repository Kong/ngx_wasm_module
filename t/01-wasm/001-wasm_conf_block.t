# vim:set ft= ts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

plan tests => repeat_each() * (blocks() * 3);

run_tests();

__DATA__

=== TEST 1: wasm{} block
--- main_config
    wasm {}
--- no_error_log
[error]
[crit]



=== TEST 2: wasm{} - missing block
--- main_config
--- error_log
no "wasm" section in configuration
--- no_error_log
[error]
--- must_die



=== TEST 3: wasm{} - duplicated block
--- main_config
    wasm {}
    wasm {}
--- error_log
"wasm" directive is duplicate
--- no_error_log
[error]
--- must_die



=== TEST 4: wasm{} - default runtime (no 'use' directive)
--- main_config
    wasm {}
--- error_log eval
qr/\[notice\] .*? using the "wasmtime" wasm runtime/
--- no_error_log
[error]



=== TEST 5: 'use' directive - sanity
--- main_config
    wasm {
        use wasmtime;
    }
--- error_log eval
qr/\[notice\] .*? using the "wasmtime" wasm runtime/
--- no_error_log
[error]



=== TEST 6: 'use' directive - duplicated
--- main_config
    wasm {
        use wasmtime;
        use wasmtime;
    }
--- error_log
"use" directive is duplicate
--- no_error_log
[error]
--- must_die



=== TEST 7: 'use' directive - invalid value
--- main_config
    wasm {
        use foo;
    }
--- error_log
invalid wasm runtime "foo"
--- no_error_log
[error]
--- must_die



=== TEST 8: 'module' directive - sanity with .wat module
--- skip_no_debug: 3
--- main_config
    wasm {
        module hello $TEST_NGINX_HTML_DIR/hello.wat;
    }
--- user_files
>>> hello.wat
(module)
--- error_log eval
[qr/\[debug\] .*? wasm compiling "hello" \.wat module at ".*?hello\.wat"/,
qr/\[notice\] .*? \[wasm\] loading "hello" module from ".*?hello\.wat" <vm: core, runtime: .*?>/]
--- no_error_log



=== TEST 9: 'module' directive - sanity with .wasm module
--- skip_no_debug: 3
--- main_config eval
qq{
    wasm {
        module hello $t::TestWasm::crates/rust_http_tests.wasm;
    }
}
--- user_files
>>> hello.wat
(module)
--- error_log eval
qr/\[notice\] .*? \[wasm\] loading "hello" module from ".*?\.wasm" <vm: core, runtime: .*?>/
--- no_error_log
wasm compiling "hello" .wat module



=== TEST 10: 'module' directive - multiple modules
--- main_config
    wasm {
        module hello $TEST_NGINX_HTML_DIR/hello.wat;
        module world $TEST_NGINX_HTML_DIR/hello.wat;
    }
--- user_files
>>> hello.wat
(module)
--- error_log eval
[qr/\[notice\] .*? \[wasm\] loading "hello" module from ".*?hello\.wat"/,
qr/\[notice\] .*? \[wasm\] loading "world" module from ".*?hello\.wat"/]
--- no_error_log



=== TEST 11: 'module' directive - invalid (no name)
--- main_config
    wasm {
        module $TEST_NGINX_HTML_DIR/hello.wat;
    }
--- error_log eval
qr/\[emerg\] .*? invalid number of arguments in "module" directive/
--- no_error_log
[error]
--- must_die



=== TEST 12: 'module' directive - invalid (no path)
--- main_config
    wasm {
        module hello;
    }
--- error_log eval
qr/\[emerg\] .*? invalid number of arguments in "module" directive/
--- no_error_log
[error]
--- must_die



=== TEST 13: 'module' directive - invalid (invalid name)
--- main_config
    wasm {
        module '' $TEST_NGINX_HTML_DIR/hello.wat;
    }
--- error_log eval
qr/\[emerg\] .*? invalid module name ""/
--- no_error_log
[error]
--- must_die



=== TEST 14: 'module' directive - invalid (invalid path)
--- main_config
    wasm {
        module hello '';
    }
--- error_log eval
qr/\[emerg\] .*? invalid module path ""/
--- no_error_log
[error]
--- must_die



=== TEST 15: 'module' directive - invalid (already defined)
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
--- must_die



=== TEST 16: 'module' directive - invalid (no such path)
--- main_config
    wasm {
        module hello $TEST_NGINX_HTML_DIR/none.wat;
    }
--- error_log eval
qr/\[emerg\] .*? \[wasm\] open\(\) ".*?none\.wat" failed \(2: No such file or directory\)/
--- no_error_log
[error]
--- must_die



=== TEST 17: 'module' directive - invalid (invalid module)
--- main_config
    wasm {
        module hello $TEST_NGINX_HTML_DIR/hello.wat;
    }
--- user_files
>>> hello.wat
--- error_log eval
qr/\[emerg\] .*? \[wasm\] failed to compile ".*?hello\.wat" to wasm \(expected at least one module field/
--- no_error_log
[error]
--- must_die
