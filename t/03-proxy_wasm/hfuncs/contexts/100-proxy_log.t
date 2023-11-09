# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

skip_hup();
skip_no_debug();
skip_valgrind();

plan tests => repeat_each() * (blocks() * 4);

run_tests();

__DATA__

=== TEST 1: proxy_wasm contexts - proxy_log on_vm_start
--- main_config
    wasm {
        module context_checks $TEST_NGINX_CRATES_DIR/context_checks.wasm 'proxy_log';
    }
--- config
    location /t {
        proxy_wasm context_checks;
        return 200;
    }
--- ignore_response_body
--- error_log
proxy_log msg
--- no_error_log
[error]
[crit]



=== TEST 2: proxy_wasm contexts - proxy_log on_configure
--- main_config
    wasm {
        module context_checks $TEST_NGINX_CRATES_DIR/context_checks.wasm;
    }
--- config
    location /t {
        proxy_wasm context_checks 'on_configure=proxy_log';
        return 200;
    }
--- ignore_response_body
--- error_log
proxy_log msg
--- no_error_log
[error]
[crit]



=== TEST 3: proxy_wasm contexts - proxy_log on_tick
--- main_config
    wasm {
        module context_checks $TEST_NGINX_CRATES_DIR/context_checks.wasm;
    }
--- config
    location /t {
        proxy_wasm context_checks 'on_tick=proxy_log';
        return 200;
    }
--- ignore_response_body
--- error_log
proxy_log msg
--- no_error_log
[error]
[crit]



=== TEST 4: proxy_wasm contexts - proxy_log on_http_dispatch_response
--- wasm_modules: context_checks
--- config
    location /t {
        proxy_wasm context_checks 'on_http_dispatch_response=proxy_log \
                                   host=127.0.0.1:$TEST_NGINX_SERVER_PORT';
        return 200;
    }

    location /dispatch {
        return 200;
    }
--- ignore_response_body
--- error_log
proxy_log msg
--- no_error_log
[error]
[crit]



=== TEST 5: proxy_wasm contexts - proxy_log on_request_headers
--- main_config
    wasm {
        module context_checks $TEST_NGINX_CRATES_DIR/context_checks.wasm;
    }
--- config
    location /t {
        proxy_wasm context_checks 'on_request_headers=proxy_log';
        return 200;
    }
--- ignore_response_body
--- error_log
proxy_log msg
--- no_error_log
[error]
[crit]



=== TEST 6: proxy_wasm contexts - proxy_log on_request_body
--- load_nginx_modules: ngx_http_echo_module
--- main_config
    wasm {
        module context_checks $TEST_NGINX_CRATES_DIR/context_checks.wasm;
    }
--- config
    location /t {
        proxy_wasm context_checks 'on_request_body=proxy_log';
        echo ok;
    }
--- request
POST /t
payload
--- ignore_response_body
--- error_log
proxy_log msg
--- no_error_log
[error]
[crit]



=== TEST 7: proxy_wasm contexts - proxy_log on_response_headers
--- main_config
    wasm {
        module context_checks $TEST_NGINX_CRATES_DIR/context_checks.wasm;
    }
--- config
    location /t {
        proxy_wasm context_checks 'on_response_headers=proxy_log';
        return 200;
    }
--- ignore_response_body
--- error_log
proxy_log msg
--- no_error_log
[error]
[crit]



=== TEST 8: proxy_wasm contexts - proxy_log on_response_body
--- load_nginx_modules: ngx_http_echo_module
--- main_config
    wasm {
        module context_checks $TEST_NGINX_CRATES_DIR/context_checks.wasm;
    }
--- config
    location /t {
        proxy_wasm context_checks 'on_response_body=proxy_log';
        echo ok;
    }
--- ignore_response_body
--- error_log
proxy_log msg
--- no_error_log
[error]
[crit]



=== TEST 9: proxy_wasm contexts - proxy_log on_log
--- main_config
    wasm {
        module context_checks $TEST_NGINX_CRATES_DIR/context_checks.wasm;
    }
--- config
    location /t {
        proxy_wasm context_checks 'on_log=proxy_log';
        return 200;
    }
--- ignore_response_body
--- error_log
proxy_log msg
--- no_error_log
[error]
[crit]
