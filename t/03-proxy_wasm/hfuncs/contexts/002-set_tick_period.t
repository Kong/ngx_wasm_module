# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasmX;

skip_hup();
skip_no_debug();

plan_tests(4);
run_tests();

__DATA__

=== TEST 1: proxy_wasm contexts - set_tick_period on_vm_start
--- main_config
    wasm {
        module context_checks $TEST_NGINX_CRATES_DIR/context_checks.wasm 'set_tick_period';
    }
--- config
    location /t {
        proxy_wasm context_checks;
        return 200;
    }
--- ignore_response_body
--- error_log
set_tick_period status: 0
--- no_error_log
[error]
[crit]



=== TEST 2: proxy_wasm contexts - set_tick_period on_configure
--- wasm_modules: context_checks
--- config
    location /t {
        proxy_wasm context_checks 'on_configure=set_tick_period';
        return 200;
    }
--- ignore_response_body
--- error_log
set_tick_period status: 0
--- no_error_log
[error]
[crit]



=== TEST 3: proxy_wasm contexts - set_tick_period on_tick
--- wasm_modules: context_checks
--- config
    location /t {
        proxy_wasm context_checks 'on_tick=set_tick_period';
        return 200;
    }
--- ignore_response_body
--- error_log
[error]
tick_period already set
--- no_error_log
[crit]



=== TEST 4: proxy_wasm contexts - set_tick_period on_http_dispatch_response
--- wasm_modules: context_checks
--- config
    location /t {
        proxy_wasm context_checks 'on_http_dispatch_response=set_tick_period \
                                   host=127.0.0.1:$TEST_NGINX_SERVER_PORT';
        return 200;
    }

    location /dispatch {
        return 200;
    }
--- ignore_response_body
--- error_log
can only set tick_period in root context
--- no_error_log
[crit]
[alert]



=== TEST 5: proxy_wasm contexts - set_tick_period on_request_headers
--- wasm_modules: context_checks
--- config
    location /t {
        proxy_wasm context_checks 'on_request_headers=set_tick_period';
        return 200;
    }
--- error_code: 500
--- ignore_response_body
--- error_log
can only set tick_period in root context
--- no_error_log
[crit]
[alert]



=== TEST 6: proxy_wasm contexts - set_tick_period on_request_body
--- load_nginx_modules: ngx_http_echo_module
--- main_config
    wasm {
        module context_checks $TEST_NGINX_CRATES_DIR/context_checks.wasm;
    }
--- config
    location /t {
        proxy_wasm context_checks 'on_request_body=set_tick_period';
        echo ok;
    }
--- request
POST /t
payload
--- error_code: 500
--- ignore_response_body
--- error_log
can only set tick_period in root context
--- no_error_log
[crit]
[alert]



=== TEST 7: proxy_wasm contexts - set_tick_period on_response_headers
--- main_config
    wasm {
        module context_checks $TEST_NGINX_CRATES_DIR/context_checks.wasm;
    }
--- config
    location /t {
        proxy_wasm context_checks 'on_response_headers=set_tick_period';
        return 200;
    }
--- ignore_response_body
--- error_log
can only set tick_period in root context
--- no_error_log
[crit]
[alert]



=== TEST 8: proxy_wasm contexts - set_tick_period on_response_body
--- load_nginx_modules: ngx_http_echo_module
--- main_config
    wasm {
        module context_checks $TEST_NGINX_CRATES_DIR/context_checks.wasm;
    }
--- config
    location /t {
        proxy_wasm context_checks 'on_response_body=set_tick_period';
        echo ok;
    }
--- ignore_response_body
--- error_log
can only set tick_period in root context
--- no_error_log
[crit]
[alert]



=== TEST 9: proxy_wasm contexts - set_tick_period on_log
--- main_config
    wasm {
        module context_checks $TEST_NGINX_CRATES_DIR/context_checks.wasm;
    }
--- config
    location /t {
        proxy_wasm context_checks 'on_log=set_tick_period';
        return 200;
    }
--- ignore_response_body
--- error_log
can only set tick_period in root context
--- no_error_log
[crit]
[alert]



=== TEST 10: proxy_wasm contexts - set_tick_period on_foreign_function
--- skip_eval: 4: $::nginxV !~ m/openresty/
--- wasm_modules: context_checks
--- config
    location /t {
        proxy_wasm context_checks 'on_foreign_function=set_tick_period';
        return 200;
    }
--- ignore_response_body
--- error_log
can only set tick_period in root context
--- no_error_log
[crit]
[alert]
