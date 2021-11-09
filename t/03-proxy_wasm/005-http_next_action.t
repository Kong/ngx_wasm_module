# vim:set ft= ts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

skip_valgrind();

plan tests => repeat_each() * (blocks() * 5);

run_tests();

__DATA__

=== TEST 1: proxy_wasm - on_request_headers -> Pause
should delay pause until access phase (NGX_AGAIN)
--- timeout_no_valgrind: 1s
--- abort
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: on_phases
--- config
    location /t {
        proxy_wasm on_phases 'pause_on=request_headers';
        echo;

        # does not work with return which runs in rewrite
        #return 200;
    }
--- error_code:
--- response_body
--- error_log
pausing after "RequestHeaders"
--- no_error_log
[error]
[crit]



=== TEST 2: proxy_wasm - on_request_body -> Pause
should pause on content phase (NGX_AGAIN)
--- timeout_no_valgrind: 1s
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



=== TEST 3: proxy_wasm - async subrequests
--- timeout_no_valgrind: 1s
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
[wasm] NYI - proxy_wasm cannot pause after "rewrite" phase in subrequests
--- no_error_log
[error]



=== TEST 4: proxy_wasm - on_response_headers -> Pause
NYI
--- wasm_modules: on_phases
--- config
    location /t {
        proxy_wasm on_phases 'pause_on=response_headers';
        return 200;
    }
--- response_body
--- error_log
pausing after "ResponseHeaders"
[wasm] NYI - proxy_wasm cannot pause after "header_filter" phase
--- no_error_log
[error]



=== TEST 5: proxy_wasm - on_response_body -> Pause
NYI
--- wasm_modules: on_phases
--- config
    location /t {
        proxy_wasm on_phases 'pause_on=response_body';
        return 200;
    }
--- response_body
--- error_log
pausing after "ResponseBody"
[wasm] NYI - proxy_wasm cannot pause after "body_filter" phase
--- no_error_log
[error]
