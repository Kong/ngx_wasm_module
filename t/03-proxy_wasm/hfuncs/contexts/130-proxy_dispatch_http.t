# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasmX;

skip_hup();
skip_no_debug();

plan_tests(4);
run_tests();

__DATA__

=== TEST 1: proxy_wasm contexts - dispatch_http_call on_vm_start
--- main_config
    wasm {
        module context_checks $TEST_NGINX_CRATES_DIR/context_checks.wasm 'dispatch_http_call';
    }
--- config
    location /t {
        proxy_wasm context_checks;
        return 200;
    }
--- must_die: 0
--- error_log
[error]
can only send HTTP dispatch during "on_request_headers", "on_request_body", "on_dispatch_response", "on_tick"
--- no_error_log
[crit]



=== TEST 2: proxy_wasm contexts - dispatch_http_call on_configure
--- wasm_modules: context_checks
--- config
    location /t {
        proxy_wasm context_checks 'on_configure=dispatch_http_call';
        return 200;
    }
--- must_die: 0
--- error_log
[error]
can only send HTTP dispatch during "on_request_headers", "on_request_body", "on_dispatch_response", "on_tick"
--- no_error_log
[crit]



=== TEST 3: proxy_wasm contexts - dispatch_http_call on_tick
--- wasm_modules: context_checks
--- config
    location /t {
        proxy_wasm context_checks 'on_tick=dispatch_http_call';
        return 200;
    }
--- error_log
dispatch failed: no :method
--- no_error_log
[crit]
[emerg]



=== TEST 4: proxy_wasm contexts - dispatch_http_call on_http_dispatch_response
--- wasm_modules: context_checks
--- config
    location /t {
        proxy_wasm context_checks 'on_http_dispatch_response=dispatch_http_call \
                                   host=127.0.0.1:$TEST_NGINX_SERVER_PORT';
        return 200;
    }

    location /dispatch {
        return 200;
    }
--- ignore_response_body
--- error_log
dispatch failed: no :method
--- no_error_log
[crit]
[alert]



=== TEST 5: proxy_wasm contexts - dispatch_http_call on_request_headers
--- wasm_modules: context_checks
--- config
    location /t {
        proxy_wasm context_checks 'on_request_headers=dispatch_http_call';
        return 200;
    }
--- error_code: 500
--- ignore_response_body
--- error_log
dispatch failed: no :method
--- no_error_log
[crit]
[alert]



=== TEST 6: proxy_wasm contexts - dispatch_http_call on_request_body
--- load_nginx_modules: ngx_http_echo_module
--- main_config
    wasm {
        module context_checks $TEST_NGINX_CRATES_DIR/context_checks.wasm;
    }
--- config
    location /t {
        proxy_wasm context_checks 'on_request_body=dispatch_http_call';
        echo ok;
    }
--- request
POST /t
payload
--- error_code: 500
--- ignore_response_body
--- error_log
dispatch failed: no :method
--- no_error_log
[crit]
[alert]



=== TEST 7: proxy_wasm contexts - dispatch_http_call on_response_headers
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: context_checks
--- config
    location /t {
        proxy_wasm context_checks 'on_response_headers=dispatch_http_call';
        echo ok;
    }
--- error_code: 500
--- ignore_response_body
--- error_log
[error]
can only send HTTP dispatch during "on_request_headers", "on_request_body", "on_dispatch_response", "on_tick"
--- no_error_log
[crit]



=== TEST 8: proxy_wasm contexts - dispatch_http_call on_response_body
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: context_checks
--- config
    location /t {
        proxy_wasm context_checks 'on_response_body=dispatch_http_call';
        echo ok;
    }
--- ignore_response_body
--- error_log
[error]
can only send HTTP dispatch during "on_request_headers", "on_request_body", "on_dispatch_response", "on_tick"
--- no_error_log
[crit]



=== TEST 9: proxy_wasm contexts - dispatch_http_call on_log
--- wasm_modules: context_checks
--- config
    location /t {
        proxy_wasm context_checks 'on_log=dispatch_http_call';
        return 200;
    }
--- error_log
[error]
can only send HTTP dispatch during "on_request_headers", "on_request_body", "on_dispatch_response", "on_tick"
--- no_error_log
[crit]
