# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

skip_valgrind();

plan tests => repeat_each() * (blocks() * 3);

run_tests();

__DATA__

=== TEST 1: safety - proxy_get_header_map_value() with oob key
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/safety/proxy_get_header_map_value_oob_key';
    }
--- error_code: 500
--- error_log eval
[
    qr/bad host usage: invalid slice pointer passed to host function/
]
--- no_error_log
[alert]



=== TEST 2: safety - proxy_get_header_map_value() with oob return data
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/safety/proxy_get_header_map_value_oob_return_data';
    }
--- error_code: 500
--- error_log eval
[
    qr/bad host usage: invalid data pointer passed to host function/
]
--- no_error_log
[alert]



=== TEST 3: safety - proxy_get_header_map_value() with misaligned return data
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/safety/proxy_get_header_map_value_misaligned_return_data';
    }
--- error_code: 500
--- error_log eval
[
    qr/bad host usage: invalid data pointer passed to host function/
]
--- no_error_log
[alert]
