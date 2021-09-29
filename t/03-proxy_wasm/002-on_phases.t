# vim:set ft= ts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

skip_valgrind();

#repeat_each(2);

plan tests => repeat_each() * (blocks() * 5);

run_tests();

__DATA__

=== TEST 1: proxy_wasm - on_vm_start
--- wasm_modules: on_phases
--- config
    location /t {
        proxy_wasm on_phases;
        return 200;
    }
--- response_body
--- error_log eval
qr/\[info\] .*? on_vm_start, config_size: 0/
--- no_error_log
[error]
[crit]



=== TEST 2: proxy_wasm - on_configure without configuration
--- wasm_modules: on_phases
--- config
    location /t {
        proxy_wasm on_phases;
        return 200;
    }
--- response_body
--- error_log eval
qr/\[info\] .*? on_configure, config_size: 0/
--- no_error_log
[error]
[crit]



=== TEST 3: proxy_wasm - on_configure with configuration
--- wasm_modules: on_phases
--- config
    location /t {
        proxy_wasm on_phases 'key=value foo=bar';
        return 200;
    }
--- response_body
--- error_log eval
[
    qr/\[info\] .*? #0 on_configure, config_size: 17/,
    qr/\[info\] .*? #0 config: key=value foo=bar/
]
--- no_error_log
[error]
