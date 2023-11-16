# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

skip_no_go_sdk();

plan_tests(5);
run_tests();

__DATA__

=== TEST 1: proxy_wasm Go SDK - dispatch_call_on_tick example (resolver error)
Missing IP for "web_service" host requested by the filter.
--- wasm_modules: go_dispatch_call_on_tick
--- config
    location /private {
        internal;
        # the filter does not support HTTP contexts
        proxy_wasm go_dispatch_call_on_tick;
    }

    location /t {
        return 200;
    }
--- ignore_response_body
--- error_log eval
qr/\[error\] .*? dispatch failed: tcp socket - resolver error: Host not found/
--- no_error_log
[crit]
[emerg]
[alert]



=== TEST 2: proxy_wasm Go SDK - dispatch_call_on_tick example
Connection refused on 127.0.0.1:81 requested by filter.
--- wasm_modules: go_dispatch_call_on_tick
--- config
    resolver_add 127.0.0.1 web_service;

    location /private {
        internal;
        # the filter does not support HTTP contexts
        proxy_wasm go_dispatch_call_on_tick;
    }

    location /t {
        return 200;
    }
--- ignore_response_body
--- error_log eval
qr/\[error\] .*? dispatch failed: tcp socket - Connection refused/
--- no_error_log
[crit]
[emerg]
[alert]
