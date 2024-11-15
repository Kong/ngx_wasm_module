# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasmX::Lua;

skip_hup();
skip_no_debug();
skip_no_openresty();

plan_tests(4);
run_tests();

__DATA__

=== TEST 1: proxy_wasm contexts - call_resolve_lua on_vm_start
--- main_config
    wasm {
        module context_checks $TEST_NGINX_CRATES_DIR/context_checks.wasm 'call_resolve_lua';
    }
--- config
    location /t {
        proxy_wasm context_checks;
        return 200;
    }
--- must_die: 0
--- error_log
[error]
can only call resolve_lua during "on_request_headers", "on_request_body", "on_tick", "on_dispatch_response", "on_foreign_function"
--- no_error_log
[crit]



=== TEST 2: proxy_wasm contexts - call_resolve_lua on_configure
--- wasm_modules: context_checks
--- config
    location /t {
        proxy_wasm context_checks 'on_configure=call_resolve_lua';
        return 200;
    }
--- must_die: 0
--- error_log
[error]
can only call resolve_lua during "on_request_headers", "on_request_body", "on_tick", "on_dispatch_response", "on_foreign_function"
--- no_error_log
[crit]



=== TEST 3: proxy_wasm contexts - call_resolve_lua on_tick
--- wasm_modules: context_checks
--- config
    location /t {
        proxy_wasm context_checks 'on_tick=call_resolve_lua';
        return 200;
    }
--- error_log
[error]
cannot resolve, missing name
--- no_error_log
[crit]



=== TEST 4: proxy_wasm contexts - call_resolve_lua on_http_dispatch_response
--- wasm_modules: context_checks
--- config
    location /t {
        proxy_wasm context_checks 'on_http_dispatch_response=call_resolve_lua \
                                   host=127.0.0.1:$TEST_NGINX_SERVER_PORT';
        return 200;
    }

    location /dispatch {
        return 200;
    }
--- error_log
[error]
cannot resolve, missing name
--- no_error_log
[crit]



=== TEST 5: proxy_wasm contexts - call_resolve_lua on_request_headers
--- wasm_modules: context_checks
--- config
    location /t {
        proxy_wasm context_checks 'on_request_headers=call_resolve_lua';
        return 200;
    }
--- error_code: 500
--- ignore_response_body
--- error_log
[error]
cannot resolve, missing name
--- no_error_log
[crit]



=== TEST 6: proxy_wasm contexts - call_resolve_lua on_request_body
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: context_checks
--- config
    location /t {
        proxy_wasm context_checks 'on_request_body=call_resolve_lua';
        echo ok;
    }
--- request
POST /t
payload
--- error_code: 500
--- ignore_response_body
--- error_log
[error]
cannot resolve, missing name
--- no_error_log
[crit]



=== TEST 7: proxy_wasm contexts - call_resolve_lua on_response_headers
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: context_checks
--- config
    location /t {
        proxy_wasm context_checks 'on_response_headers=call_resolve_lua';
        echo ok;
    }
--- ignore_response_body
--- error_log
[error]
can only call resolve_lua during "on_request_headers", "on_request_body", "on_tick", "on_dispatch_response", "on_foreign_function"
--- no_error_log
[crit]



=== TEST 8: proxy_wasm contexts - call_resolve_lua on_response_body
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: context_checks
--- config
    location /t {
        proxy_wasm context_checks 'on_response_body=call_resolve_lua';
        echo ok;
    }
--- ignore_response_body
--- error_log
[error]
can only call resolve_lua during "on_request_headers", "on_request_body", "on_tick", "on_dispatch_response", "on_foreign_function"
--- no_error_log
[crit]



=== TEST 9: proxy_wasm contexts - call_resolve_lua on_log
--- wasm_modules: context_checks
--- config
    location /t {
        proxy_wasm context_checks 'on_log=call_resolve_lua';
        return 200;
    }
--- error_log
[error]
can only call resolve_lua during "on_request_headers", "on_request_body", "on_tick", "on_dispatch_response", "on_foreign_function"
--- no_error_log
[crit]



=== TEST 10: proxy_wasm contexts - call_resolve_lua on_foreign_function
--- wasm_modules: context_checks
--- config
    location /t {
        proxy_wasm context_checks 'on_foreign_function=call_resolve_lua';
        return 200;
    }
--- ignore_response_body
--- error_log
[error]
cannot resolve, missing name
--- no_error_log
[crit]
