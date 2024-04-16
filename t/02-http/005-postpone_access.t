# vim:set ft= ts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasmX;

skip_no_debug();

plan_tests(3);
run_tests();

__DATA__

=== TEST 1: wasm_postpone_access - default (disabled)
--- load_nginx_modules: ngx_http_echo_module
--- config
    location /t {
        echo "ok";
    }
--- response_body
ok
--- grep_error_log eval: qr/wasm access handler/
--- grep_error_log_out
wasm access handler



=== TEST 2: wasm_postpone_access - disabled
--- load_nginx_modules: ngx_http_echo_module
--- config
    location /t {
        wasm_postpone_access off;
        echo "ok";
    }
--- response_body
ok
--- grep_error_log eval: qr/wasm access handler/
--- grep_error_log_out
wasm access handler



=== TEST 3: wasm_postpone_access - enabled
--- load_nginx_modules: ngx_http_echo_module
--- config
location /t {
    wasm_postpone_access on;
        echo "ok";
    }
--- response_body
ok
--- grep_error_log eval: qr/wasm access handler/
--- grep_error_log_out
wasm access handler
wasm access handler



=== TEST 4: wasm_postpone_access - enabled in server{} block
--- load_nginx_modules: ngx_http_echo_module
--- config
    wasm_postpone_access on;

    location /t {
        echo "ok";
    }
--- response_body
ok
--- grep_error_log eval: qr/wasm access handler/
--- grep_error_log_out
wasm access handler
wasm access handler



=== TEST 5: wasm_postpone_access - enabled in http{} block
--- load_nginx_modules: ngx_http_echo_module
--- http_config
    wasm_postpone_access on;
--- config
    location /t {
        echo "ok";
    }
--- response_body
ok
--- grep_error_log eval: qr/wasm access handler/
--- grep_error_log_out
wasm access handler
wasm access handler



=== TEST 6: wasm_postpone_access - enabled in http{} block, disabled in location{}
--- load_nginx_modules: ngx_http_echo_module
--- http_config
    wasm_postpone_access on;
--- config
    location /t {
        wasm_postpone_access off;
        echo "ok";
    }
--- response_body
ok
--- grep_error_log eval: qr/wasm access handler/
--- grep_error_log_out
wasm access handler
