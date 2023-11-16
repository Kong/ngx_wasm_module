# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

BEGIN {
    $ENV{MOCKEAGAIN} = 'w';
    $ENV{MOCKEAGAIN_VERBOSE} ||= 1;
    $ENV{TEST_NGINX_EVENT_TYPE} = 'poll';
}

use strict;
use lib '.';
use t::TestWasm;

plan_tests(4);
run_tests();

__DATA__

=== TEST 1: local_response - pre-ops flush with status + reason
--- wasm_modules: ngx_rust_tests
--- config
    location /t {
        wasm_call rewrite ngx_rust_tests local_reason;
    }
--- error_code: 201
--- raw_response_headers_like
HTTP/1.1 201 REASON\s
--- no_error_log
[error]
[crit]



=== TEST 2: local_response - pre-ops flush with body
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: ngx_rust_tests
--- config
    location /t {
        wasm_call rewrite ngx_rust_tests local_response;
        echo fail;
    }
--- response_body
hello world
--- no_error_log
[error]
[crit]



=== TEST 3: local_response - post-ops flush with body
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: ngx_rust_tests
--- config
    location /t {
        wasm_call content ngx_rust_tests local_response;
        echo fail;
    }
--- response_body
hello world
--- no_error_log
[error]
[crit]



=== TEST 4: local_response - can be discarded
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: ngx_rust_tests
--- config
    location /t {
        wasm_call rewrite ngx_rust_tests local_response;
        wasm_call rewrite ngx_rust_tests discard_local_response;
        echo ok;
    }
--- response_body
ok
--- no_error_log
[error]
[crit]
