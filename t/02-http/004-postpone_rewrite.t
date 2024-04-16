# vim:set ft= ts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasmX;

skip_hup(); # HUP mode /ver requests trigger many rewrite handler calls
skip_no_debug();

plan_tests(3);
run_tests();

__DATA__

=== TEST 1: wasm_postpone_rewrite - default (disabled)
--- load_nginx_modules: ngx_http_echo_module
--- config
    location /t {
        echo "ok";
    }
--- response_body
ok
--- grep_error_log eval: qr/wasm rewrite handler/
--- grep_error_log_out
wasm rewrite handler



=== TEST 2: wasm_postpone_rewrite - disabled
--- load_nginx_modules: ngx_http_echo_module
--- config
    location /t {
        wasm_postpone_rewrite off;
        echo "ok";
    }
--- response_body
ok
--- grep_error_log eval: qr/wasm rewrite handler/
--- grep_error_log_out
wasm rewrite handler



=== TEST 3: wasm_postpone_rewrite - enabled
--- load_nginx_modules: ngx_http_echo_module
--- config
    location /t {
        wasm_postpone_rewrite on;
        echo "ok";
    }
--- response_body
ok
--- grep_error_log eval: qr/wasm rewrite handler/
--- grep_error_log_out
wasm rewrite handler
wasm rewrite handler



=== TEST 4: wasm_postpone_rewrite - enabled in server{} block
--- load_nginx_modules: ngx_http_echo_module
--- config
    wasm_postpone_rewrite on;

    location /t {
        echo "ok";
    }
--- response_body
ok
--- grep_error_log eval: qr/wasm rewrite handler/
--- grep_error_log_out
wasm rewrite handler
wasm rewrite handler



=== TEST 5: wasm_postpone_rewrite - enabled in http{} block
--- load_nginx_modules: ngx_http_echo_module
--- http_config
    wasm_postpone_rewrite on;
--- config
    location /t {
        echo "ok";
    }
--- response_body
ok
--- grep_error_log eval: qr/wasm rewrite handler/
--- grep_error_log_out
wasm rewrite handler
wasm rewrite handler



=== TEST 6: wasm_postpone_rewrite - enabled in http{} block, disabled in location{}
--- load_nginx_modules: ngx_http_echo_module
--- http_config
    wasm_postpone_rewrite on;
--- config
    location /t {
        wasm_postpone_rewrite off;
        echo "ok";
    }
--- response_body
ok
--- grep_error_log eval: qr/wasm rewrite handler/
--- grep_error_log_out
wasm rewrite handler
