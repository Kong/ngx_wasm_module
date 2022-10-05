# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

our $compiler;
if ($::nginxV =~ /wasmer/) {
    $compiler = "singlepass";
} else {
    $compiler = "auto";
}

plan tests => repeat_each() * (blocks() * 3);

run_tests();

__DATA__

=== TEST 1: proxy_wasm - get_vm_configuration() on_vm_start
--- main_config eval
qq{
    wasm {
        compiler $::compiler;
        module   hostcalls $ENV{TEST_NGINX_CRATES_DIR}/hostcalls.wasm;
    }
}
--- config
    location /t {
        proxy_wasm hostcalls;
        return 200;
    }
--- ignore_response_body
--- error_log eval
qr/\[info\] .*? vm config: $::compiler/
--- no_error_log
[error]
