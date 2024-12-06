# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasmX::Lua;

skip_no_openresty();

plan_tests(5);
run_tests();

__DATA__

=== TEST 1: proxy_wasm - resolve_lua, NXDOMAIN
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm_lua_resolver on;
        proxy_wasm hostcalls 'test=/t/resolve_lua \
                              name=foo';
        return 200;
    }
--- error_log eval
qr/could not resolve foo/
--- no_error_log
[crit]
[emerg]
[stub]



=== TEST 2: proxy_wasm - resolve_lua (no yielding)
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm_lua_resolver on;
        proxy_wasm hostcalls 'test=/t/resolve_lua \
                              name=localhost';
        return 200;
    }
--- error_log eval
qr/resolved \(no yielding\) localhost to \[127, 0, 0, 1\]/
--- no_error_log
[error]
[crit]
[emerg]



=== TEST 3: proxy_wasm - resolve_lua (yielding)
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm_lua_resolver on;
        proxy_wasm hostcalls 'test=/t/resolve_lua \
                              name=httpbin.org';
        return 200;
    }
--- error_log eval
qr/resolved \(yielding\) httpbin\.org to \[\d+, \d+, \d+, \d+\]/
--- no_error_log
[error]
[crit]
[emerg]



=== TEST 4: proxy_wasm - resolve_lua, IPv6 record
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm_lua_resolver on;
        proxy_wasm hostcalls 'test=/t/resolve_lua \
                              name=ipv6.google.com';
        return 200;
    }
--- error_log eval
qr/resolved \(yielding\) ipv6\.google\.com to \[\d+, \d+, \d+, \d+, \d+, \d+, \d+, \d+, \d+, \d+, \d+, \d+, \d+, \d+, \d+, \d+\]/
--- no_error_log
[error]
[crit]
[emerg]



=== TEST 5: proxy_wasm - resolve_lua, multiple calls (yielding)
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm_lua_resolver on;
        proxy_wasm hostcalls 'test=/t/resolve_lua \
                              name=httpbin.org,wikipedia.org';
        return 200;
    }
--- error_log eval
[
    qr/resolved \(yielding\) httpbin\.org to \[\d+, \d+, \d+, \d+\]/,
    qr/resolved \(yielding\) wikipedia\.org to \[\d+, \d+, \d+, \d+\]/
]
--- no_error_log
[error]
[crit]



=== TEST 6: proxy_wasm - resolve_lua, multiple calls (mixed)
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm_lua_resolver on;
        proxy_wasm hostcalls 'test=/t/resolve_lua \
                              name=localhost,httpbin.org';
        return 200;
    }
--- error_log eval
[
    qr/resolved \(no yielding\) localhost to \[127, 0, 0, 1\]/,
    qr/resolved \(yielding\) httpbin\.org to \[\d+, \d+, \d+, \d+\]/
]
--- no_error_log
[error]
[crit]



=== TEST 7: proxy_wasm - resolve_lua, on_tick
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'tick_period=500 \
                              on_tick=resolve_lua \
                              name=localhost,httpbin.org';
        return 200;
    }
--- error_log eval
[
    qr/resolved \(no yielding\) localhost to \[127, 0, 0, 1\]/,
    qr/resolved \(yielding\) httpbin\.org to \[\d+, \d+, \d+, \d+\]/
]
--- no_error_log
[error]
[crit]



=== TEST 8: proxy_wasm - resolve_lua, on_http_call_response
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=127.0.0.1:$TEST_NGINX_SERVER_PORT \
                              path=/dispatch \
                              on_http_call_response=resolve_lua \
                              name=httpbin.org,wikipedia.org';
        return 200;
    }

    location /dispatch {
        return 200;
    }
--- error_log eval
[
    qr/resolved \(yielding\) httpbin\.org to \[\d+, \d+, \d+, \d+\]/,
    qr/resolved \(yielding\) wikipedia\.org to \[\d+, \d+, \d+, \d+\]/
]
--- no_error_log
[error]
[crit]



=== TEST 9: proxy_wasm - resolve_lua, on_foreign_function
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/resolve_lua \
                              on_foreign_function=resolve_lua \
                              name=httpbin.org';
        return 200;
    }
--- error_log eval
[
    qr/resolved \(yielding\) httpbin\.org to \[\d+, \d+, \d+, \d+\]/,
    qr/resolved \(yielding\) httpbin\.org to \[\d+, \d+, \d+, \d+\]/
]
--- no_error_log
[error]
[crit]
