# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasmX;
use Test::Nginx::Socket skip_all => 'HTTP trailers support is not ready yet';

plan_tests(5);
run_tests();

__DATA__

=== TEST 1: proxy_wasm - trap on_response_trailers
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'on=response_trailers test=/t/trap';
        return 200;
    }
--- response_body
--- error_log eval
[
    qr/\[crit\] .*? panicked at/,
    qr/custom trap/,
]
--- no_error_log
[emerg]
