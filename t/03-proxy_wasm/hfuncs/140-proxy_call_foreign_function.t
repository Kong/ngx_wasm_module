# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasmX;

plan_tests(4);
run_tests();

__DATA__

=== TEST 1: proxy_wasm - call_foreign_function(), unknown function
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/call_foreign_function \
                              name=foo';
    }
--- error_code: 500
--- error_log eval
qr/host trap \(internal error\): unknown foreign function/
--- no_error_log
[crit]
[emerg]



=== TEST 2: proxy_wasm - call_foreign_function(), known function
--- skip_eval: 4: $::nginxV =~ m/openresty/
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/call_foreign_function \
                              name=resolve_lua';
    }
--- error_code: 500
--- error_log eval
qr/host trap \(internal error\): cannot resolve, no lua support/
--- no_error_log
[crit]
[emerg]
