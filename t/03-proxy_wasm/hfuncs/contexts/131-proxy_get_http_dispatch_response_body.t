# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasmX;

skip_hup();
skip_no_debug();

plan_tests(4);
run_tests();

__DATA__

=== TEST 1: proxy_wasm contexts - get_http_dispatch_response_body on_tick
--- wasm_modules: context_checks
--- config
    location /t {
        proxy_wasm context_checks 'on_tick=get_dispatch_response_body_buffer';
        return 200;
    }
--- ignore_response_body
--- error_log
[error]
can only get dispatch response body during "on_http_dispatch_response"
--- no_error_log
[crit]
