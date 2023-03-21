# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

skip_valgrind('wasmtime');
skip_no_tinygo();

plan tests => repeat_each() * (blocks() * 5);

run_tests();

__DATA__

=== TEST 1: proxy_wasm Go SDK - vm_plugin_configuration example
--- load_nginx_modules: ngx_http_echo_module
--- main_config eval
qq{
    wasm {
        module go_vm_plugin_configuration $ENV{TEST_NGINX_CRATES_DIR}/go_vm_plugin_configuration.wasm 'hello=world';
    }

    timer_resolution 10ms;
}
--- config
    location /tick {
        internal;
        proxy_wasm go_vm_plugin_configuration 'default config';
    }

    location /t {
        echo_sleep 0.1;
        echo_status 200;
    }
--- ignore_response_body
--- error_log eval
[
    qr/\[info\] .*? successfully loaded "go_vm_plugin_configuration"/,
    qr/\[info\] .*? vm config: hello=world/,
    qr/\[info\] .*? plugin config: default config/,
]
--- no_error_log
[error]
