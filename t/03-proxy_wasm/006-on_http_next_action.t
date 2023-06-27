# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

skip_valgrind();

plan tests => repeat_each() * (blocks() * 5);

run_tests();

__DATA__

=== TEST 1: proxy_wasm - on_request_headers -> Pause
--- timeout_expected: 1
--- abort
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: on_phases
--- config
    location /t {
        proxy_wasm on_phases 'pause_on=request_headers';
        echo;
    }
--- error_code:
--- response_body
--- error_log
pausing after "RequestHeaders"
--- no_error_log
[error]
[crit]



=== TEST 2: proxy_wasm - on_request_body -> Pause
--- timeout_expected: 1
--- abort
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: on_phases
--- config
    location /t {
        proxy_wasm on_phases 'pause_on=request_body';
        echo;
    }
--- request
POST /t
Hello world
--- error_code:
--- response_body
--- error_log
pausing after "RequestBody"
--- no_error_log
[error]
[crit]



=== TEST 3: proxy_wasm - on_response_headers -> Pause
NYI
--- wasm_modules: on_phases
--- config
    location /t {
        proxy_wasm on_phases 'pause_on=response_headers';
        return 200;
    }
--- error_code: 500
--- response_body_like: 500 Internal Server Error
--- error_log eval
[
    qr/pausing after "ResponseHeaders"/,
    qr#\[error\] .*? filter 1/1 cannot pause in "header_filter" phase#,
    qr#\[warn\] .*? filter 1/1 failed resuming "on_response_body" step in "body_filter" phase \(not yieldable\)#
]



=== TEST 4: proxy_wasm - on_response_body -> Pause
NYI
--- wasm_modules: on_phases
--- config
    location /t {
        proxy_wasm on_phases 'pause_on=response_body';
        return 200;
    }
--- response_body
--- error_log eval
[
    qr/pausing after "ResponseBody"/,
    qr#\[error\] .*? filter 1/1 cannot pause in "body_filter" phase#,
    qr#\[warn\] .*? filter 1/1 failed resuming "on_log" step in "log" phase \(not yieldable\)#
]



=== TEST 5: proxy_wasm - subrequest on_request_headers -> Pause
--- timeout_expected: 1
--- abort
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: on_phases
--- config
    location /pause {
        internal;
        proxy_wasm on_phases 'pause_on=request_headers';
        echo ok;
    }

    location /nop {
        internal;
        proxy_wasm on_phases;
        echo ok;
    }

    location /t {
        echo_subrequest_async GET /pause;
        echo_subrequest_async GET /nop;
    }
--- error_code:
--- response_body
--- error_log
pausing after "RequestHeaders"
--- no_error_log
[error]
[emerg]



=== TEST 6: proxy_wasm - subrequest on_request_body -> Pause
--- timeout_expected: 1
--- abort
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: on_phases
--- config
    location /pause {
        internal;
        proxy_wasm on_phases 'pause_on=request_body';
        echo pause;
    }

    location /nop {
        internal;
        proxy_wasm on_phases;
        echo nop;
    }

    location /t {
        echo_subrequest_async GET /nop;
        echo_subrequest_async GET /pause;
    }
--- request
POST /t
Hello world
--- error_code: 200
--- response_body
nop
--- error_log
pausing after "RequestBody"
--- no_error_log
[error]
[crit]



=== TEST 7: proxy_wasm - subrequest on_response_headers -> Pause
NYI
--- timeout_expected: 1
--- abort
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: on_phases
--- config
    location /pause {
        internal;
        proxy_wasm on_phases 'pause_on=response_headers';
        echo ok;
    }

    location /nop {
        internal;
        proxy_wasm on_phases;
        echo ok;
    }

    location /t {
        echo_subrequest_async GET /pause;
        echo_subrequest_async GET /nop;
    }
--- error_code: 200
--- response_body_like: 500 Internal Server Error
--- error_log eval
[
    qr/pausing after "ResponseHeaders"/,
    qr#\[error\] .*? filter 1/1 cannot pause in "header_filter" phase#,
    qr#\[warn\] .*? filter 1/1 failed resuming "on_response_body" step in "body_filter" phase \(not yieldable\)#
]



=== TEST 8: proxy_wasm - subrequest on_response_body -> Pause
NYI
--- timeout_expected: 1
--- abort
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: on_phases
--- config
    location /pause {
        internal;
        proxy_wasm on_phases 'pause_on=response_body';
        echo ok;
    }

    location /nop {
        internal;
        proxy_wasm on_phases;
        echo ok;
    }

    location /t {
        echo_subrequest GET /pause;
        echo_subrequest GET /nop;
    }
--- error_code: 200
--- response_body
ok
ok
--- error_log eval
[
    qr/pausing after "ResponseBody"/,
    qr#\[error\] .*? filter 1/1 cannot pause in "body_filter" phase#,
    qr#\[warn\] .*? filter 1/1 failed resuming "on_response_body" step in "body_filter" phase \(not yieldable\)#
]
