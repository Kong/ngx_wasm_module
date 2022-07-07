# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

skip_valgrind('wasmtime');

plan tests => repeat_each() * (blocks() * 4);

run_tests();

__DATA__

=== TEST 1: proxy_wasm Rust SDK - 0.1 sanity
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: rust_sdk_ver_zero_one
--- config
    location /t {
        proxy_wasm rust_sdk_ver_zero_one;
        echo ok;
    }
--- response_body
ok
--- no_error_log
[error]
[crit]



=== TEST 2: proxy_wasm Rust SDK - 0.1 HTTP dispatch
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: rust_sdk_ver_zero_one
--- config
    location /dispatched {
        return 200 "Hello world";
    }

    location /t {
        proxy_wasm rust_sdk_ver_zero_one 'test=dispatch \
                                          host=127.0.0.1:$TEST_NGINX_SERVER_PORT \
                                          path=/dispatched
                                          on_http_call_response=echo_response_body';
        echo fail;
    }
--- response_body
Hello world
--- no_error_log
[error]
[crit]



=== TEST 3: proxy_wasm Rust SDK - 0.1 proxy_get_header_map_value()
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: rust_sdk_ver_zero_one
--- config
    location /t {
        proxy_wasm rust_sdk_ver_zero_one 'test=proxy_get_header_map_value';
        echo ok;
    }
--- response_body
close
--- no_error_log
[error]
[crit]
