# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

skip_valgrind();

plan tests => repeat_each() * (blocks() * 6);

run_tests();

__DATA__

=== TEST 1: proxy_wasm directive - no wasm{} configuration block
--- main_config
--- config
    location /t {
        proxy_wasm a;
        return 200;
    }
--- error_log eval
qr/\[emerg\] .*? "proxy_wasm" directive is specified but config has no "wasm" section/
--- no_error_log
[warn]
[error]
[alert]
[crit]
--- must_die



=== TEST 2: proxy_wasm directive - invalid number of arguments
--- config
    location /t {
        proxy_wasm a foo bar;
        return 200;
    }
--- error_log eval
qr/\[emerg\] .*? invalid number of arguments in "proxy_wasm" directive/
--- no_error_log
[warn]
[error]
[alert]
[crit]
--- must_die



=== TEST 3: proxy_wasm directive - empty module name
--- main_config
    wasm {}
--- config
    location /t {
        proxy_wasm '';
        return 200;
    }
--- error_log eval
qr/\[emerg\] .*? invalid module name ""/
--- no_error_log
[warn]
[error]
[alert]
[crit]
--- must_die



=== TEST 4: proxy_wasm directive - missing ABI version
--- load_nginx_modules: ngx_http_echo_module
--- main_config
    wasm {
        module a $TEST_NGINX_HTML_DIR/a.wat;
    }
--- config
    location /t {
        proxy_wasm a;
        echo ok;
    }
--- user_files
>>> a.wat
(module
  (func $nop)
  (export "nop" (func $nop))
)
--- error_code: 500
--- error_log eval
[
    qr/\[emerg\] .*? proxy_wasm failed initializing "a" filter \(unknown ABI version\)/,
    qr/\[warn\] .*? proxy_wasm "a" filter \(1\/1\) failed resuming \(unknown ABI version\)/
]
--- no_error_log
[error]
[crit]
[stderr]



=== TEST 5: proxy_wasm directive - missing malloc + proxy_on_memory_allocate
--- load_nginx_modules: ngx_http_echo_module
--- main_config
    wasm {
        module a $TEST_NGINX_HTML_DIR/a.wat;
    }
--- config
    location /t {
        proxy_wasm a;
        echo ok;
    }
--- user_files
>>> a.wat
(module
  (func $nop)
  (export "proxy_abi_version_0_2_1" (func $nop))
)
--- error_code: 500
--- error_log eval
[
    qr/\[emerg\] .*? proxy_wasm "a" filter missing malloc \(incompatible SDK interface\)/,
    qr/\[warn\] .*? proxy_wasm "a" filter \(1\/1\) failed resuming \(incompatible SDK interface\)/
]
--- no_error_log
[error]
[crit]
[stderr]



=== TEST 6: proxy_wasm directive - missing malloc, fallback proxy_on_memory_allocate
should error since missing on_context_create
--- load_nginx_modules: ngx_http_echo_module
--- main_config
    wasm {
        module a $TEST_NGINX_HTML_DIR/a.wat;
    }
--- config
    location /t {
        proxy_wasm a;
        echo ok;
    }
--- user_files
>>> a.wat
(module
  (func $nop)
  (export "proxy_abi_version_0_2_1" (func $nop))
  (export "proxy_on_memory_allocate" (func $nop))
)
--- error_code: 500
--- error_log eval
[
    qr/\[emerg\] .*? proxy_wasm "a" filter missing one of: .*? \(incompatible SDK interface\)/,
    qr/\[warn\] .*? proxy_wasm "a" filter \(1\/1\) failed resuming \(incompatible SDK interface\)/
]
--- no_error_log
[error]
[crit]
[stderr]



=== TEST 7: proxy_wasm directive - unknown ABI version
--- load_nginx_modules: ngx_http_echo_module
--- main_config
    wasm {
        module a $TEST_NGINX_HTML_DIR/a.wat;
    }
--- config
    location /t {
        proxy_wasm a;
        echo ok;
    }
--- user_files
>>> a.wat
(module
  (func $nop)
  (export "proxy_abi_version_0_0_0" (func $nop))
)
--- error_code: 500
--- error_log eval
[
    qr/\[emerg\] .*? proxy_wasm failed initializing "a" filter \(unknown ABI version\)/,
    qr/\[warn\] .*? proxy_wasm "a" filter \(1\/1\) failed resuming \(unknown ABI version\)/
]
--- no_error_log
[error]
[crit]
[stderr]



=== TEST 8: proxy_wasm directive - incompatible ABI version
--- skip_no_debug: 6
--- load_nginx_modules: ngx_http_echo_module
--- main_config
    wasm {
        module a $TEST_NGINX_HTML_DIR/a.wat;
    }
--- config
    location /t {
        proxy_wasm a;
        echo fail;
    }
--- user_files
>>> a.wat
(module
  (func $nop)
  (export "proxy_abi_version_vnext" (func $nop))
)
--- error_code: 500
--- error_log eval
[
    qr/\[emerg\] .*? proxy_wasm failed initializing "a" filter \(incompatible ABI version\)/,
    qr/\[warn\] .*? proxy_wasm "a" filter \(1\/1\) failed resuming \(incompatible ABI version\)/
]
--- no_error_log
[error]
[crit]
[stderr]



=== TEST 9: proxy_wasm directive - duplicate entries in location{} block
should be accepted
should create two instances of the same module
--- skip_no_debug: 6
--- skip_valgrind: 6
--- wasm_modules: on_tick
--- config
    location /t {
        proxy_wasm on_tick;
        proxy_wasm on_tick;
        return 200;
    }
--- error_log eval
[
    qr/\[debug\] .*? wasm creating "on_tick" instance in "main" vm/,
    qr/\[debug\] .*? wasm creating "on_tick" instance in "main" vm/,
    qr/\[debug\] .*? proxy_wasm initializing "on_tick" filter/,
    qr/\[debug\] .*? proxy_wasm initializing "on_tick" filter/,
]
--- no_error_log
[error]



=== TEST 10: proxy_wasm_isolation directive - no wasm{} configuration block
--- main_config
--- config
    proxy_wasm_isolation none;
--- error_log eval
qr/\[emerg\] .*? "proxy_wasm_isolation" directive is specified but config has no "wasm" section/
--- no_error_log
[warn]
[error]
[alert]
[crit]
--- must_die



=== TEST 11: proxy_wasm_isolation directive - invalid number of arguments
--- config
    location /t {
        proxy_wasm_isolation none none;
    }
--- error_log eval
qr/\[emerg\] .*? invalid number of arguments in "proxy_wasm_isolation" directive/
--- no_error_log
[warn]
[error]
[alert]
[crit]
--- must_die
