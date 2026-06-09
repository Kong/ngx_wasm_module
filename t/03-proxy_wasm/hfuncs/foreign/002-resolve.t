# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasmX;

our $ExtTimeout = $t::TestWasmX::exttimeout;

plan tests => 43;
run_tests();

__DATA__

=== TEST 1: proxy_wasm - resolve, NXDOMAIN
--- timeout eval: $::ExtTimeout
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/resolve \
                              name=foo';
        return 200;
    }
--- error_log eval
qr/\[info\] .*? could not resolve foo/
--- no_error_log
[crit]
[emerg]



=== TEST 2: proxy_wasm - resolve, bad name
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/resolve \
                              name=';
        return 200;
    }
--- error_code: 500
--- error_log eval
qr/host trap \(bad usage\): cannot resolve, missing name/,
--- no_error_log
[crit]
[emerg]
[alert]



=== TEST 3: proxy_wasm - resolve (yielding)
--- timeout eval: $::ExtTimeout
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/resolve \
                              name=httpbin.org';
        return 200;
    }
--- error_log eval
qr/resolved \(yielding\) httpbin\.org to \[\d+(?:, \d+)+\]/,
--- no_error_log
[error]
[crit]
[emerg]



=== TEST 4: proxy_wasm - resolve (no yielding)
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/resolve \
                              name=httpbin.org';
        proxy_wasm hostcalls 'test=/t/resolve \
                              name=httpbin.org';
        return 200;
    }
--- error_log eval
qr/resolved \(yielding\) httpbin\.org to \[\d+(?:, \d+)+\]/,
qr/resolved \(no yielding\) httpbin\.org to \[\d+(?:, \d+)+\]/
--- no_error_log
[error]
[crit]
[emerg]



=== TEST 5: proxy_wasm - resolve, IPv6 record
--- timeout eval: $::ExtTimeout
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/resolve \
                              name=ipv6.google.com';
        return 200;
    }
--- error_log eval
qr/resolved \(yielding\) ipv6\.google\.com to \[\d+, \d+, \d+, \d+, \d+, \d+, \d+, \d+, \d+, \d+, \d+, \d+, \d+, \d+, \d+, \d+\]/
--- no_error_log
[error]
[crit]
[emerg]



=== TEST 6: proxy_wasm - resolve, multiple calls (yielding)
--- timeout eval: $::ExtTimeout
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/resolve \
                              name=httpbin.org,example.com';
        return 200;
    }
--- error_log eval
[
    qr/resolved \(yielding\) httpbin\.org to \[\d+(?:, \d+)+\]/,
    qr/resolved \(yielding\) example\.com to \[\d+(?:, \d+)+\]/
]
--- no_error_log
[error]
[crit]



=== TEST 7: proxy_wasm - resolve, on_tick
--- timeout eval: $::ExtTimeout
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'tick_period=500 \
                              on_tick=resolve \
                              name=httpbin.org';
        return 200;
    }
--- error_log eval
[
    qr/resolved \(yielding\) httpbin\.org to \[\d+(?:, \d+)+\]/
]
--- no_error_log
[error]
[crit]



=== TEST 8: proxy_wasm - resolve, on_http_call_response
--- timeout eval: $::ExtTimeout
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=127.0.0.1:$TEST_NGINX_SERVER_PORT \
                              path=/dispatch \
                              on_http_call_response=resolve \
                              name=httpbin.org,example.com';
        return 200;
    }

    location /dispatch {
        return 200;
    }
--- error_log eval
[
    qr/resolved \(yielding\) httpbin\.org to \[\d+(?:, \d+)+\]/,
    qr/resolved \(yielding\) example\.com to \[\d+(?:, \d+)+\]/
]
--- no_error_log
[error]
[crit]



=== TEST 9: proxy_wasm - resolve, on_foreign_function
--- timeout eval: $::ExtTimeout
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/resolve \
                              on_foreign_function=resolve \
                              name=httpbin.org';
        return 200;
    }
--- error_log eval
[
    qr/resolved \(yielding\) httpbin\.org to \[\d+(?:, \d+)+\]/,
    qr/resolved \(yielding\) httpbin\.org to \[\d+(?:, \d+)+\]/
]
--- no_error_log
[error]
[crit]
