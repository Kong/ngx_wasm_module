# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

our $nginxV = $t::TestWasm::nginxV;

plan tests => repeat_each() * (blocks() * 4);

run_tests();

__DATA__

=== TEST 1: load a WASI function from the VM, not the host
--- skip_eval: 4: $::nginxV =~ m/v8/
--- wasm_modules: wasi_vm_tests
--- config
    location /t {
        wasm_call rewrite wasi_vm_tests test_wasi_non_host;
    }
--- response_body
test passed
--- no_error_log
[error]
[crit]



=== TEST 2: V8 does not provide any non-host WASI functions
'daemon off' must be set to check exit_code is 2
Valgrind mode already writes 'daemon off'
HUP mode does not catch the worker exit_code
--- skip_eval: 4: $::nginxV !~ m/v8/ || $ENV{TEST_NGINX_USE_HUP} == 1
--- wasm_modules: wasi_vm_tests
--- main_config eval
$ENV{TEST_NGINX_USE_VALGRIND} ? '' : 'daemon off;'
--- config
    location /t {
        wasm_call rewrite wasi_vm_tests test_wasi_non_host;
    }
--- error_log eval
qr/\[emerg\] .*? failed linking "wasi_vm_tests"/
--- no_error_log
[crit]
stub
--- must_die: 2
