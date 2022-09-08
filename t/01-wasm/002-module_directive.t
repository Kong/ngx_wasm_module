# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

our $nginxV = $t::TestWasm::nginxV;

plan tests => repeat_each() * (blocks() * 4);

run_tests();

__DATA__

=== TEST 1: module directive - valid .wasm module
--- main_config eval
qq{
    wasm {
        module ngx-rust-tests $t::TestWasm::crates/ngx_rust_tests.wasm;
    }
}
--- error_log eval
qr/\[info\] .*? \[wasm\] loading "ngx-rust-tests" module <vm: "main", runtime: ".*?">/
--- no_error_log
[error]
[crit]



=== TEST 2: module directive - valid .wat module
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
qr/\[info\] .*? \[wasm\] loading "a" module <vm: "main", runtime: ".*?">/
--- no_error_log
[error]
[crit]



=== TEST 3: module directive - multiple valid modules
--- skip_no_debug: 4
--- main_config
    wasm {
        module a $TEST_NGINX_HTML_DIR/a.wat;
        module b $TEST_NGINX_HTML_DIR/a.wat;
    }
--- user_files
>>> a.wat
(module)
--- error_log eval
[qr/\[debug\] .*? wasm loading "a" module bytes from ".*?a\.wat"/,
qr/\[debug\] .*? wasm loading "b" module bytes from ".*?a\.wat"/]
--- no_error_log
[error]



=== TEST 4: module directive - invalid arguments
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



=== TEST 5: module directive - bad name
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



=== TEST 6: module directive - bad path
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



=== TEST 7: module directive - already defined
--- main_config
    wasm {
        module a $TEST_NGINX_HTML_DIR/a.wat;
        module a $TEST_NGINX_HTML_DIR/a.wat;
    }
--- user_files
>>> a.wat
(module)
--- error_log eval
qr/\[emerg\] .*? "a" module already defined/
--- no_error_log
[error]
[crit]
--- must_die



=== TEST 8: module directive - no such path
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



=== TEST 9: module directive - no .wat bytes - wasmtime, wasmer
--- skip_eval: 4: !( $::nginxV =~ m/wasmtime/ || $::nginxV =~ m/wasmer/ )
--- main_config
    wasm {
        module a $TEST_NGINX_HTML_DIR/a.wat;
    }
--- user_files
>>> a.wat
--- error_log eval
[
    qr/\[info\] .*? \[wasm\] initializing "main" wasm VM/,
    qr/\[emerg\] .*? \[wasm\] failed loading "a" module bytes: expected at least one module field/
]
--- no_error_log
[error]
--- must_die



=== TEST 10: module directive - no .wat bytes - v8
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        module a $TEST_NGINX_HTML_DIR/a.wat;
    }
--- user_files
>>> a.wat
--- error_log eval
[
    qr/\[info\] .*? \[wasm\] initializing "main" wasm VM/,
    qr/\[emerg\] .*? \[wasm\] failed loading "a" module bytes: unexpected token "EOF", expected a module field or a module/
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
    qr/\[info\] .*? \[wasm\] initializing "main" wasm VM/,
    qr/\[emerg\] .*? \[wasm\] failed loading "a" module bytes: .*?unexpected EOF/
]
--- no_error_log
[error]
--- must_die



=== TEST 12: module directive - invalid .wat module - wasmtime, wasmer
--- skip_eval: 4: !( $::nginxV =~ m/wasmtime/ || $::nginxV =~ m/wasmer/ )
--- main_config
    wasm {
        module a $TEST_NGINX_HTML_DIR/a.wat;
    }
--- user_files
>>> a.wat
(module
  (import "env" "ngx_log" (func))
  (import "env" "ngx_log" (func))))
--- error_log eval
[
    qr/\[info\] .*? \[wasm\] initializing "main" wasm VM/,
    qr/\[emerg\] .*? \[wasm\] failed loading "a" module bytes: extra tokens remaining after parse/
]
--- no_error_log
[error]
--- must_die



=== TEST 13: module directive - invalid .wat module - v8
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        module a $TEST_NGINX_HTML_DIR/a.wat;
    }
--- user_files
>>> a.wat
(module
  (import "env" "ngx_log" (func))
  (import "env" "ngx_log" (func))))
--- error_log eval
[
    qr/\[info\] .*? \[wasm\] initializing "main" wasm VM/,
    qr/\[emerg\] .*? \[wasm\] failed loading "a" module bytes: unexpected token \), expected EOF/
]
--- no_error_log
[error]
--- must_die



=== TEST 14: module directive - invalid module (NYI import types)
--- SKIP
--- main_config
    wasm {
        module a $TEST_NGINX_HTML_DIR/a.wat;
    }
--- user_files
>>> a.wat
(module
  (import "env" "ngx_log" (func))
  (import "env" "global" (global f32)))
--- error_log eval
qr/\[alert\] .*? \[wasm\] NYI: module import type not supported <vm: "main", runtime: ".*?">/
--- no_error_log
[error]
[crit]



=== TEST 15: module directive - invalid module (missing host function in env)
'daemon off' must be set to check exit_code is 2
Valgrind mode already writes 'daemon off'
HUP mode does not catch the worker exit_code
--- skip_eval: 4: defined $ENV{TEST_NGINX_USE_HUP}
--- main_config eval
qq{
    wasm {
        module a $ENV{TEST_NGINX_HTML_DIR}/a.wat;
    }
}.(defined $ENV{TEST_NGINX_USE_VALGRIND} ? '' : 'daemon off;')
--- config
    proxy_wasm a;
--- user_files
>>> a.wat
(module
  (import "env" "ngx_unknown" (func)))
--- error_log eval
[
    qr/\[info\] .*? \[wasm\] loading "a" module <vm: "main", runtime: ".*?">/,
    qr/\[error\] .*? \[wasm\] failed importing "env.ngx_unknown": missing host function <vm: "main", runtime: ".*?">/,
    qr/\[emerg\] .*? \[wasm\] failed linking "a" module with "ngx_proxy_wasm" host interface: incompatible host interface/
]
--- must_die: 2



=== TEST 16: module directive - invalid module (missing WASI function)
'daemon off' must be set to check exit_code is 2
Valgrind mode already writes 'daemon off'
HUP mode does not catch the worker exit_code
--- skip_eval: 4: $::nginxV =~ m/wasmtime/ || defined $ENV{TEST_NGINX_USE_HUP}
--- main_config eval
qq{
    wasm {
        module x $ENV{TEST_NGINX_HTML_DIR}/x.wat;
    }
}.(defined $ENV{TEST_NGINX_USE_VALGRIND} ? '' : 'daemon off;')
--- config
    proxy_wasm x;
--- user_files
>>> x.wat
(module
  (import "wasi_snapshot_preview1" "unknown_function" (func)))
--- error_log eval
[
    qr/\[info\] .*? \[wasm\] loading "x" module <vm: "main", runtime: ".*?">/,
    qr/\[error\] .*? \[wasm\].*? unhandled WASI function "unknown_function"/,
    qr/\[emerg\] .*? \[wasm\] failed linking "x" module with "ngx_proxy_wasm" host interface/
]
--- must_die: 2
