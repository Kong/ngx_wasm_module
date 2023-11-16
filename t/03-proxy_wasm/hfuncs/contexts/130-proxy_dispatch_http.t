# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

skip_hup();
skip_no_debug();
skip_valgrind('always');

plan tests => repeat_each() * (blocks() * 4);

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



=== TEST 4: proxy_wasm contexts - dispatch_http_call on_log
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
