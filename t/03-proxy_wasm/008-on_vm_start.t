# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

plan tests => repeat_each() * (blocks() * 3);

our $compiler;
if ($::nginxV =~ /wasmer/) {
    $compiler = "singlepass";
} else {
    $compiler = "auto";
}

run_tests();

__DATA__

=== TEST 1: proxy_wasm - on_vm_start
VM start is triggered even without hitting the configured location
since it runs at root level context.
--- load_nginx_modules: ngx_http_echo_module
--- main_config eval
qq{
    wasm {
        compiler $::compiler;
        module on_vm_start $ENV{TEST_NGINX_CRATES_DIR}/on_vm_start.wasm;
    }

    timer_resolution 10ms;
}
--- config
    location /tick {
        internal;
        proxy_wasm on_vm_start;
    }

    location /t {
        echo_sleep 0.1;
        echo_status 200;
    }
--- ignore_response_body
--- error_log eval
[
    qr/\[info\] .* vm config: $::compiler/,
]
--- no_error_log
[error]
