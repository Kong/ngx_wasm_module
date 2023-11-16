# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

plan_tests(5);
run_tests();

__DATA__

=== TEST 1: proxy_wasm - set_buffer() bad buffer type
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/bad_set_buffer_type';
        echo fail;
    }
--- error_code: 500
--- response_body_like: 500 Internal Server Error
--- error_log eval
[
    qr/\[alert\] .*? NYI - set_buffer bad buf_type: 100/,
    qr/\[crit\] .*? panicked at/,
    qr/unexpected status: 2/,
]



=== TEST 2: proxy_wasm - get_buffer() bad buffer type
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/bad_get_buffer_type';
        echo fail;
    }
--- error_code: 500
--- response_body_like: 500 Internal Server Error
--- error_log eval
[
    qr/\[alert\] .*? NYI - get_buffer bad buf_type: 100/,
    qr/\[crit\] .*? panicked at/,
    qr/unexpected status: 2/,
]



=== TEST 3: proxy_wasm - set_map() bad map type
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/bad_set_map_type';
        echo fail;
    }
--- error_code: 500
--- response_body_like: 500 Internal Server Error
--- error_log eval
[
    qr/\[alert\] .*? NYI - set_map bad map_type: 100/,
    qr/\[crit\] .*? panicked at/,
    qr/unexpected status: 2/,
]



=== TEST 4: proxy_wasm - get_map() bad map type
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/bad_get_map_type';
        echo fail;
    }
--- error_code: 500
--- response_body_like: 500 Internal Server Error
--- error_log eval
[
    qr/\[alert\] .*? NYI - get_map bad map_type: 100/,
    qr/\[crit\] .*? panicked at/,
    qr/unexpected status: 2/,
]
