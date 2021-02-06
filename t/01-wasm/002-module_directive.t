# vim:set ft= ts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

plan tests => repeat_each() * (blocks() * 4);

run_tests();

__DATA__

=== TEST 1: module directive - sanity empty .wat module
--- main_config
    wasm {
        module a $TEST_NGINX_HTML_DIR/a.wat;
    }
--- user_files
>>> a.wat
(module)
--- error_log eval
qr/\[info\] .*? \[wasm\] loading "a" module from ".*?a\.wat" <vm: "main", runtime: ".*?">/
--- no_error_log
[error]
[emerg]



=== TEST 2: module directive - sanity empty .wasm module
--- main_config eval
qq{
    wasm {
        module http_tests $t::TestWasm::crates/rust_http_tests.wasm;
    }
}
--- error_log eval
qr/\[info\] .*? \[wasm\] loading "http_tests" module from ".*?rust_http_tests\.wasm" <vm: "main", runtime: ".*?">/
--- no_error_log
[error]
[emerg]



=== TEST 3: module directive - sanity module imports (functions only)
--- main_config
    wasm {
        module a $TEST_NGINX_HTML_DIR/a.wat;
    }
--- user_files
>>> a.wat
(module
   (import "env" "ngx_log" (func))
   (import "env" "ngx_log" (func))
)
--- error_log eval
qr/\[info\] .*? \[wasm\] loading "a" module from ".*?a\.wat" <vm: "main", runtime: ".*?">/
--- no_error_log
[error]
[emerg]



=== TEST 4: module directive - multiple modules
--- main_config
    wasm {
        module a $TEST_NGINX_HTML_DIR/a.wat;
        module b $TEST_NGINX_HTML_DIR/a.wat;
    }
--- user_files
>>> a.wat
(module)
--- error_log eval
[qr/\[info\] .*? \[wasm\] loading "a" module from ".*?a\.wat"/,
qr/\[info\] .*? \[wasm\] loading "b" module from ".*?a\.wat"/]
--- no_error_log
[emerg]



=== TEST 5: module directive - invalid arguments
--- main_config
    wasm {
        module $TEST_NGINX_HTML_DIR/a.wat;
    }
--- error_log eval
qr/\[emerg\] .*? invalid number of arguments in "module" directive/
--- no_error_log
[error]
[crit]
--- must_die



=== TEST 6: module directive - bad name
--- main_config
    wasm {
        module '' $TEST_NGINX_HTML_DIR/a.wat;
    }
--- error_log eval
qr/\[emerg\] .*? invalid module name ""/
--- no_error_log
[error]
[crit]
--- must_die



=== TEST 7: module directive - bad path
--- main_config
    wasm {
        module a '';
    }
--- error_log eval
qr/\[emerg\] .*? invalid module path ""/
--- no_error_log
[error]
[crit]
--- must_die



=== TEST 8: module directive - already defined
--- main_config
    wasm {
        module a $TEST_NGINX_HTML_DIR/a.wat;
        module a $TEST_NGINX_HTML_DIR/a.wat;
    }
--- user_files
>>> a.wat
(module)
--- error_log eval
qr/\[emerg\] .*? module "a" already defined/
--- no_error_log
[error]
[crit]
--- must_die



=== TEST 9: module directive - no such path
--- main_config
    wasm {
        module a $TEST_NGINX_HTML_DIR/none.wat;
    }
--- error_log eval
qr/\[emerg\] .*? \[wasm\] open\(\) ".*?none\.wat" failed \(2: No such file or directory\)/
--- no_error_log
[error]
[crit]
--- must_die



=== TEST 10: module directive - no .wat bytes
--- main_config
    wasm {
        module a $TEST_NGINX_HTML_DIR/a.wat;
    }
--- user_files
>>> a.wat
--- error_log eval
[
    qr/\[emerg\] .*? \[wasm\] failed loading "a" module:/,
    qr/expected at least one module field/
]
--- no_error_log
[error]
--- must_die



=== TEST 11: module directive - no .wasm bytes
--- main_config
    wasm {
        module a $TEST_NGINX_HTML_DIR/a.wasm;
    }
--- user_files
>>> a.wasm
--- error_log eval
[
    qr/\[emerg\] .*? \[wasm\] failed loading "a" module:/,
    qr/Unexpected EOF/
]
--- no_error_log
[error]
--- must_die



=== TEST 12: module directive - invalid module
--- main_config
    wasm {
        module a $TEST_NGINX_HTML_DIR/a.wat;
    }
--- user_files
>>> a.wat
(module
   (import "env" "ngx_log" (func))
   (import "env" "ngx_log" (func)))
)
--- error_log eval
[
    qr/\[emerg\] .*? \[wasm\] failed loading "a" module:/,
    qr/extra tokens remaining after parse/
]
--- no_error_log
[error]
--- must_die



=== TEST 13: module directive - invalid module (NYI import types)
--- main_config
    wasm {
        module a $TEST_NGINX_HTML_DIR/a.wat;
    }
--- user_files
>>> a.wat
(module
    (import "env" "ngx_log" (func))
    (import "env" "global" (global f32))
)
--- error_log eval
[
    qr/\[info\] .*? \[wasm\] loading "a" module from ".*?a\.wat" <vm: "main", runtime: ".*?">/,
    qr/\[alert\] .*? \[wasm\] NYI: module import type not supported <vm: "main", runtime: ".*?">/
]
--- no_error_log
[error]
--- must_die



=== TEST 14: module directive - invalid module (missing host function)
--- SKIP
--- main_config
    wasm {
        module a $TEST_NGINX_HTML_DIR/a.wat;
    }
--- user_files
>>> a.wat
(module
    (import "env" "ngx_unknown" (func)))
--- error_log eval
[
    qr/\[info\] .*? \[wasm\] loading "a" module from ".*?a\.wat" <vm: "main", runtime: ".*?">/,
    qr/\[error\] .*? \[wasm\] failed importing "env.ngx_unknown": missing host function <vm: "main", runtime: ".*?">/,
    qr/\[emerg\] .*? \[wasm\] failed loading "a" module: incompatible host interface/
]
--- must_die
