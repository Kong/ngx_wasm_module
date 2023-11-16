# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

plan_tests(5);
run_tests();

__DATA__

=== TEST 1: proxy_wasm - on_request_headers in access sanity (number of headers)
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: on_phases
--- config
    location /t {
        proxy_wasm_request_headers_in_access on;
        proxy_wasm on_phases;
        echo ok;
    }
--- more_headers
Hello: wasm
--- response_body
ok
--- error_log eval
qr/\[info\] .*? on_request_headers, 3 headers/
--- no_error_log
[error]
[crit]



=== TEST 2: proxy_wasm - on_request_headers in access - dispatch_http_call()
--- skip_no_debug
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm_request_headers_in_access on;
        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=127.0.0.1:$TEST_NGINX_SERVER_PORT \
                              path=/dispatch \
                              on_http_call_response=echo_response_body';
        echo fail;
    }

    location /dispatch {
        echo ok;
    }
--- response_body
ok
--- error_log eval
qr/proxy_wasm pausing in \"access\" phase/
--- no_error_log eval
[
    qr/\[error\]/,
    qr/proxy_wasm pausing in \"rewrite\" phase/
]



=== TEST 3: proxy_wasm - on_request_headers in rewrite - dispatch_http_call()
--- skip_no_debug
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm_request_headers_in_access off;
        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=127.0.0.1:$TEST_NGINX_SERVER_PORT \
                              path=/dispatch \
                              on_http_call_response=echo_response_body';
        echo fail;
    }

    location /dispatch {
        echo ok;
    }
--- response_body
ok
--- error_log eval
qr/proxy_wasm pausing in \"rewrite\" phase/
--- no_error_log eval
[
    qr/\[error\]/,
    qr/proxy_wasm pausing in \"access\" phase/
]



=== TEST 4: proxy_wasm - on_request_headers in access - as a subrequest
warn users that subrequests do not run an access phase
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: on_phases
--- config
    location /dispatch {
        echo ok;
    }

    location /subrequest {
        proxy_wasm_request_headers_in_access on;
        proxy_wasm on_phases;
        echo ok;
    }

    location /t {
        echo_subrequest GET '/subrequest';
    }
--- response_body
ok
--- error_log eval
qr/\[warn\] .*? proxy_wasm_request_headers_in_access enabled in a subrequest \(no access phase\)/
--- no_error_log
[error]
on_request_headers
