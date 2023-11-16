# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

plan_tests(5);
run_tests();

__DATA__

=== TEST 1: proxy_wasm - get_vm_configuration() on_vm_start, no config
--- main_config eval
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls;
        return 200;
    }
--- ignore_response_body
--- error_log eval
qr/\[info\] .*? no vm config/
--- no_error_log
[error]
vm config:
cannot parse vm config



=== TEST 2: proxy_wasm - get_vm_configuration() on_vm_start, with config
--- valgrind
--- main_config eval
qq{
    wasm {
        module hostcalls $ENV{TEST_NGINX_CRATES_DIR}/hostcalls.wasm 'hello=world';
    }
}
--- config
    location /t {
        proxy_wasm hostcalls;
        return 200;
    }
--- ignore_response_body
--- error_log eval
qr/\[info\] .*? vm config: hello=world/
--- no_error_log
[error]
no vm config
cannot parse vm config
