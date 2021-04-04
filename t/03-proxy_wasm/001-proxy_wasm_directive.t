# vim:set ft= ts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

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
        proxy_wasm a foo;
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
qr/\[emerg\] .*? \[wasm\] unknown ABI version/
--- no_error_log
[warn]
[error]
[alert]
[crit]



=== TEST 5: proxy_wasm directive - unknown ABI version
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
qr/\[emerg\] .*? \[wasm\] unknown ABI version/
--- no_error_log
[warn]
[error]
[alert]
[crit]



=== TEST 6: proxy_wasm directive - duplicated
--- main_config
    wasm {
        module on_tick $TEST_NGINX_CRATES_DIR/on_tick.wasm;
    }
--- config
    location /t {
        proxy_wasm on_tick;
        proxy_wasm on_tick;
        return 200;
    }
--- error_log eval
qr/\[emerg\] .*? "proxy_wasm" directive is duplicate/
--- no_error_log
[warn]
[error]
[alert]
[crit]
--- must_die
