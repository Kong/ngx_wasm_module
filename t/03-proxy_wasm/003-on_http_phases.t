# vim:set ft= ts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

skip_valgrind();

plan tests => repeat_each() * (blocks() * 5);

run_tests();

__DATA__

=== TEST 1: proxy_wasm - on_request_headers
--- load_nginx_modules: ngx_http_echo_module
--- main_config
    wasm {
        module on_http_phases $TEST_NGINX_CRATES_DIR/on_http_phases.wasm;
    }
--- config
    location /t {
        proxy_wasm on_http_phases;
        echo ok;
    }
--- ignore_response_body
--- error_log eval
qr/\[info\] .*? \[wasm\] #\d+ on_request_headers, 2 headers/
--- no_error_log
[error]
[emerg]
[alert]



=== TEST 2: proxy_wasm - on_response_headers
--- load_nginx_modules: ngx_http_echo_module
--- main_config
    wasm {
        module on_http_phases $TEST_NGINX_CRATES_DIR/on_http_phases.wasm;
    }
--- config
    location /t {
        proxy_wasm on_http_phases;
        echo ok;
    }
--- ignore_response_body
--- error_log eval
qr/\[info\] .*? \[wasm\] #\d+ on_response_headers, 0 headers/
--- no_error_log
[error]
[emerg]
[alert]



=== TEST 3: proxy_wasm - on_done
--- load_nginx_modules: ngx_http_echo_module
--- main_config
    wasm {
        module on_http_phases $TEST_NGINX_CRATES_DIR/on_http_phases.wasm;
    }
--- config
    location /t {
        proxy_wasm on_http_phases;
        echo ok;
    }
--- ignore_response_body
--- error_log eval
[
    qr/\[info\] .*? \[wasm\] #\d+ on_done/,
    qr/\[info\] .*? \[wasm\] #\d+ on_done/
]
--- no_error_log
[error]
[alert]
