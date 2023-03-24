# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

skip_valgrind();

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



=== TEST 4: proxy_wasm - on_configure returns false
'daemon off' must be set to check exit_code is 2
Valgrind mode already writes 'daemon off'
HUP mode does not catch the worker exit_code
--- skip_eval: 5: $ENV{TEST_NGINX_USE_HUP} == 1
--- main_config eval
qq{
    wasm {
        module on_phases $ENV{TEST_NGINX_CRATES_DIR}/on_phases.wasm;
    }
}.(defined $ENV{TEST_NGINX_USE_VALGRIND} ? '' : 'daemon off;')
--- config
    location /t {
        proxy_wasm on_phases 'fail_configure=true';
        return 200;
    }
--- error_code: 500
--- response_body_like: 500 Internal Server Error
--- error_log eval
qr/\[emerg\] .*? proxy_wasm failed initializing "on_phases" filter \(initialization failed\)/
--- no_error_log
[error]
[crit]
[alert]
--- must_die: 2
