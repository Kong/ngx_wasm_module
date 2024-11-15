# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasmX;

skip_hup();

plan_tests(4);
run_tests();

__DATA__

=== TEST 1: proxy_wasm contexts - define_metric on_vm_start
--- main_config
    env WASMTIME_BACKTRACE_DETAILS=1;
    wasm {
        module context_checks $TEST_NGINX_CRATES_DIR/context_checks.wasm 'define_metric';
    }
--- config
    location /t {
        proxy_wasm context_checks;
        return 200;
    }
--- ignore_response_body
--- error_log
define_metric status: 0
--- no_error_log
[error]
[crit]



=== TEST 2: proxy_wasm contexts - define_metric - on_configure
--- main_config
    wasm {
        module context_checks $TEST_NGINX_CRATES_DIR/context_checks.wasm;
    }
--- config
    location /t {
        proxy_wasm context_checks 'on_configure=define_metric';
        return 200;
    }
--- ignore_response_body
--- error_log
define_metric status: 0
--- no_error_log
[error]
[crit]



=== TEST 3: proxy_wasm contexts - define_metric - on_tick
--- main_config
    wasm {
        module context_checks $TEST_NGINX_CRATES_DIR/context_checks.wasm;
    }
--- config
    location /t {
        proxy_wasm context_checks 'on_tick=define_metric';
        return 200;
    }
--- ignore_response_body
--- error_log
define_metric status: 0
--- no_error_log
[error]
[crit]



=== TEST 4: proxy_wasm contexts - define_metric on_http_dispatch_response
--- wasm_modules: context_checks
--- config
    location /t {
        proxy_wasm context_checks 'on_http_dispatch_response=define_metric \
                                   host=127.0.0.1:$TEST_NGINX_SERVER_PORT';
        return 200;
    }

    location /dispatch {
        return 200;
    }
--- ignore_response_body
--- error_log
define_metric status: 0
--- no_error_log
[error]
[crit]



=== TEST 5: proxy_wasm contexts - define_metric on_request_headers
--- wasm_modules: context_checks
--- metrics: 16k
--- config
    location /t {
        proxy_wasm context_checks 'on_request_headers=define_metric';
        return 200;
    }
--- ignore_response_body
--- error_log
define_metric status: 0
--- no_error_log
[error]
[crit]



=== TEST 6: proxy_wasm contexts - define_metric on_request_body
--- load_nginx_modules: ngx_http_echo_module
--- main_config
    wasm {
        module context_checks $TEST_NGINX_CRATES_DIR/context_checks.wasm;
    }
--- config
    location /t {
        proxy_wasm context_checks 'on_request_body=define_metric';
        echo ok;
    }
--- request
POST /t
payload
--- ignore_response_body
--- error_log
define_metric status: 0
--- no_error_log
[error]
[crit]



=== TEST 7: proxy_wasm contexts - define_metric on_response_headers
--- main_config
    wasm {
        module context_checks $TEST_NGINX_CRATES_DIR/context_checks.wasm;
    }
--- config
    location /t {
        proxy_wasm context_checks 'on_response_headers=define_metric';
        return 200;
    }
--- ignore_response_body
--- error_log
define_metric status: 0
--- no_error_log
[error]
[crit]



=== TEST 8: proxy_wasm contexts - define_metric on_response_body
--- load_nginx_modules: ngx_http_echo_module
--- main_config
    wasm {
        module context_checks $TEST_NGINX_CRATES_DIR/context_checks.wasm;
    }
--- config
    location /t {
        proxy_wasm context_checks 'on_response_body=define_metric';
        echo ok;
    }
--- ignore_response_body
--- error_log
define_metric status: 0
--- no_error_log
[error]
[crit]



=== TEST 9: proxy_wasm contexts - define_metric on_log
--- main_config
    wasm {
        module context_checks $TEST_NGINX_CRATES_DIR/context_checks.wasm;
    }
--- config
    location /t {
        proxy_wasm context_checks 'on_log=define_metric';
        return 200;
    }
--- ignore_response_body
--- error_log
define_metric status: 0
--- no_error_log
[error]
[crit]



=== TEST 10: proxy_wasm contexts - define_metric on_foreign_function
--- skip_eval: 4: $::nginxV !~ m/openresty/
--- main_config
    wasm {
        module context_checks $TEST_NGINX_CRATES_DIR/context_checks.wasm;
    }
--- config
    location /t {
        proxy_wasm context_checks 'on_foreign_function=define_metric';
        return 200;
    }
--- ignore_response_body
--- error_log
define_metric status: 0
--- no_error_log
[error]
[crit]
