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

=== TEST 1: proxy_wasm contexts - get_http_request_body on_vm_start
'daemon off' must be set to check exit_code is 2
Valgrind mode already writes 'daemon off'
HUP mode does not catch the worker exit_code
--- skip_eval: 4: $ENV{TEST_NGINX_USE_HUP} == 1
--- main_config eval
qq{
    wasm {
        module context_checks $ENV{TEST_NGINX_CRATES_DIR}/context_checks.wasm 'get_request_body_buffer';
    }
}.($ENV{TEST_NGINX_USE_VALGRIND} ? '' : 'daemon off;')
--- config
    location /t {
        proxy_wasm context_checks;
        return 200;
    }
--- ignore_response_body
--- error_log
[error]
[emerg]
can only get request body during "on_request_body", "on_log"
--- no_error_log
--- must_die: 2



=== TEST 2: proxy_wasm contexts - get_http_request_body on_configure
'daemon off' must be set to check exit_code is 2
Valgrind mode already writes 'daemon off'
HUP mode does not catch the worker exit_code
--- skip_eval: 4: $ENV{TEST_NGINX_USE_HUP} == 1
--- main_config eval
qq{
    wasm {
        module context_checks $ENV{TEST_NGINX_CRATES_DIR}/context_checks.wasm;
    }
}.($ENV{TEST_NGINX_USE_VALGRIND} ? '' : 'daemon off;')
--- config
    location /t {
        proxy_wasm context_checks 'on_configure=get_request_body_buffer';
        return 200;
    }

--- ignore_response_body
--- error_log
[error]
[emerg]
can only get request body during "on_request_body", "on_log"
--- must_die: 2



=== TEST 3: proxy_wasm contexts - get_http_request_body on_tick
--- wasm_modules: context_checks
--- config
    location /t {
        proxy_wasm context_checks 'on_tick=get_request_body_buffer';
        return 200;
    }
--- ignore_response_body
--- error_log
[error]
can only get request body during "on_request_body", "on_log"
--- no_error_log
[crit]



=== TEST 4: proxy_wasm contexts - get_http_request_body on_http_dispatch_response
--- wasm_modules: context_checks
--- config
    location /t {
        proxy_wasm context_checks 'on_http_dispatch_response=get_request_body_buffer \
                                   host=127.0.0.1:$TEST_NGINX_SERVER_PORT';
        return 200;
    }

    location /dispatch {
        return 200;
    }
--- ignore_response_body
--- error_log
[error]
can only get request body during "on_request_body", "on_log"
--- no_error_log
[crit]
