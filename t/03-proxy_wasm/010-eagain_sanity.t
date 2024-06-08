# vim:set ft= ts=4 sw=4 et fdm=marker:

BEGIN {
    $ENV{MOCKEAGAIN} = 'rw';
    $ENV{MOCKEAGAIN_VERBOSE} ||= 0;
    $ENV{TEST_NGINX_EVENT_TYPE} = 'poll';
}

use strict;
use lib '.';
use t::TestWasmX;

plan_tests(5);
run_tests();

__DATA__

=== TEST 1: EAGAIN - reading request body
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: on_phases
--- config
    location /t {
        proxy_wasm on_phases;
        echo_sleep 0.3;
        echo ok;
    }
--- request
POST /t

Hello world
--- response_body
ok
--- grep_error_log eval: qr/#\d+ on_(request|log).*?(?=(, client|\s+while))/
--- grep_error_log_out eval
qr/\A#\d+ on_request_headers, 3 headers, eof: false
#\d+ on_request_body, 11 bytes, eof: true
#\d+ on_log\Z/
--- no_error_log
[error]
[crit]



=== TEST 2: EAGAIN - reading request body in subrequest
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: on_phases
--- config
    location /subrequest {
        internal;
        proxy_wasm on_phases;
        echo_sleep 0.3;
        echo ok;
    }

    location /t {
        echo_subrequest POST '/subrequest' -b 'Hello world';
    }
--- response_body
ok
--- grep_error_log eval: qr/#\d+ on_(request|log).*?(?=(, client|\s+while))/
--- grep_error_log_out eval
qr/\A#\d+ on_request_headers, 3 headers, eof: false
#\d+ on_request_body, 11 bytes, eof: true\Z/
--- no_error_log
[error]
[crit]
