# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

plan_tests(3);
run_tests();

__DATA__

=== TEST 1: proxy_wasm - proxy_get_header_map_value() with oob key
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/safety/proxy_get_header_map_value_oob_key';
    }
--- error_code: 500
--- error_log eval
qr/host trap \(bad usage\): invalid slice pointer passed to host function/
--- no_error_log
[alert]



=== TEST 2: proxy_wasm - proxy_get_header_map_value() with oob return data
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/safety/proxy_get_header_map_value_oob_return_data';
    }
--- error_code: 500
--- error_log eval
qr/host trap \(bad usage\): invalid data pointer passed to host function/
--- no_error_log
[alert]



=== TEST 3: proxy_wasm - proxy_get_header_map_value() with misaligned return data
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/safety/proxy_get_header_map_value_misaligned_return_data';
    }
--- error_code: 500
--- error_log eval
qr/host trap \(bad usage\): invalid data pointer passed to host function/
--- no_error_log
[alert]
