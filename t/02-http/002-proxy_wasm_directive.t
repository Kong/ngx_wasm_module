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
        proxy_wasm hello;
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
        proxy_wasm hello foo;
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



=== TEST 4: proxy_wasm directive - sanity
--- main_config
    wasm {
        module hello $TEST_NGINX_HTML_DIR/hello.wat;
    }
--- config
    location /t {
        proxy_wasm hello;
        return 200;
    }
--- response_body
--- no_error_log
[error]
[emerg]
--- user_files
>>> hello.wat
(module
  (func $nop)
  (export "nop" (func $nop))
)
