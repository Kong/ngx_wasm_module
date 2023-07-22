# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

skip_no_debug();

plan tests => repeat_each() * (blocks() * 4);

run_tests();

__DATA__

=== TEST 1: coverage - rc >= NGX_OK in headers_filter
--- wasm_modules: ngx_rust_tests
--- config
    location /t {
        wasm_call rewrite ngx_rust_tests local_reason;
        wasm_debug_header_filter_return 500;
    }
--- error_code: 500
--- response_body_like: 500 Internal Server Error
--- error_log eval
qr/\[alert\] .*? header already sent/
--- no_error_log
[error]



=== TEST 2: coverage - rc == NGX_ERROR in header_filters
--- wasm_modules: ngx_rust_tests
--- config
    location /t {
        wasm_call rewrite ngx_rust_tests local_reason;
        wasm_debug_header_filter_return -1;
    }
--- error_code: 500
--- response_body_like: 500 Internal Server Error
--- no_error_log
[error]
[alert]



=== TEST 3: coverage - rc >= NGX_OK in body_filter
--- wasm_modules: ngx_rust_tests
--- config
    location /t {
        wasm_call rewrite ngx_rust_tests local_reason;
        wasm_debug_body_filter_return 500;
    }
--- error_code: 201
--- response_body
--- no_error_log
[error]
[alert]



=== TEST 4: coverage - rc == NGX_ERROR in body_filter
--- wasm_modules: ngx_rust_tests
--- config
    location /t {
        wasm_call rewrite ngx_rust_tests local_reason;
        wasm_debug_body_filter_return -1;
    }
--- error_code: 201
--- response_body
--- no_error_log
[error]
[alert]



=== TEST 5: coverage - rc >= NGX_OK in pre-ops content flush with error_page
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: ngx_rust_tests
--- config
    error_page 500 /error;

    location = /error {
        internal;
        echo 'error handler content';
    }

    location /t {
        wasm_call rewrite ngx_rust_tests local_reason;
        wasm_debug_header_filter_return 500;
    }
--- error_code: 500
--- response_body
error handler content
--- error_log eval
qr/\[alert\] .*? header already sent/
--- no_error_log
[error]



=== TEST 6: coverage - rc == NGX_ERROR in pre-ops content flush
--- wasm_modules: ngx_rust_tests
--- config
    location /t {
        wasm_call rewrite ngx_rust_tests local_reason;
        wasm_debug_header_filter_return -1;
    }
--- error_code: 500
--- response_body_like: 500 Internal Server Error
--- no_error_log
[error]
[alert]



=== TEST 7: coverage - rc >= NGX_OK in post-ops content flush with error_page
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: ngx_rust_tests
--- config
    error_page 500 /error;

    location = /error {
        internal;
        echo 'error handler content';
    }

    location /t {
        wasm_call content ngx_rust_tests local_reason;
        wasm_debug_header_filter_return 500;
    }
--- error_code: 500
--- response_body
error handler content
--- error_log eval
qr/\[alert\] .*? header already sent/
--- no_error_log
[error]



=== TEST 8: coverage - rc == NGX_ERROR in post-ops content flush
--- wasm_modules: ngx_rust_tests
--- config
    location /t {
        wasm_call content ngx_rust_tests local_reason;
        wasm_debug_header_filter_return -1;
    }
--- error_code: 500
--- response_body_like: 500 Internal Server Error
--- no_error_log
[error]
[alert]
