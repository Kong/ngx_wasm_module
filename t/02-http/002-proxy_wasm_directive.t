# vim:set ft= ts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

#plan skip_all => 'NYI: ngx_http_wasm_proxy_module';

plan tests => repeat_each() * (blocks() * 4);

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
[error]
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
[error]
[crit]
--- must_die



=== TEST 3: proxy_wasm directive - empty module name
--- config
    location /t {
        proxy_wasm '';
        return 200;
    }
--- error_log eval
qr/\[emerg\] .*? invalid module name ""/
--- no_error_log
[error]
[crit]
--- must_die



=== TEST 4: proxy_wasm directive - unknown ABI version
--- main_config
    wasm {
        module a $TEST_NGINX_HTML_DIR/a.wat;
    }
--- config
    location /t {
        proxy_wasm a;
        return 200;
    }
--- user_files
>>> a.wat
(module
  (func $nop)
  (export "nop" (func $nop))
)
--- error_log eval
qr/\[emerg\] .*? \[wasm\] unknown proxy_wasm ABI version/
--- no_error_log
[error]
[alert]
--- must_die



=== TEST 5: proxy_wasm directive - bad ABI version
--- main_config
    wasm {
        module a $TEST_NGINX_HTML_DIR/a.wat;
    }
--- config
    location /t {
        proxy_wasm a;
        return 200;
    }
--- user_files
>>> a.wat
(module
  (func $nop)
  (export "proxy_abi_version_0_2_0" (func $nop))
)
--- error_log eval
qr/\[emerg\] .*? \[wasm\] incompatible proxy_wasm ABI version/
--- no_error_log
[error]
[alert]
--- must_die



=== TEST 6: proxy_wasm directive - hello_world example
--- SKIP
--- main_config
    wasm {
        module a /home/chasum/code/proxy-wasm-rust-sdk/target/wasm32-unknown-unknown/debug/examples/hello_world.wasm;
    }
--- config
    location /t {
        proxy_wasm a;
        return 200;
    }
--- response_body
--- no_error_log
[error]
[emerg]
--- user_files
>>> a.wat
(module
  (func $nop)
  (export "nop" (func $nop))
)
