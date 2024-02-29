# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasmX;

plan_tests(5);
run_tests();

__DATA__

=== TEST 1: proxy_wasm - set_property() - non-changeable property: request_headers
--- valgrind
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config eval
qq{
    location /t {
        proxy_wasm hostcalls 'on=request_headers \
                              test=/t/set_property \
                              name=request.path';
        echo ok;
    }
}
--- error_code: 500
--- response_body_like eval: qr/500 Internal Server Error/
--- error_log eval
[
    qr/\[error\] .*? variable "request_uri" is not changeable/,
    qr/\[crit\] .*? panicked at/,
    qr/unexpected status: 10/,
]



=== TEST 2: proxy_wasm - set_property() - unknown property on: request_headers

In spite of the panic below, our proxy-wasm implementation does not
cause a critical failure when a property is not found.

The Rust SDK, which we're using here, panics with "unexpected status:
1", (where 1 means NotFound), but other SDKs, such as the Go SDK,
forward the NotFound status back to the caller.

--- wasm_modules: hostcalls
--- load_nginx_modules: ngx_http_echo_module
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/set_property \
                              name=nonexistent_property';
        echo ok;
    }
--- error_code: 500
--- response_body_like eval: qr/500 Internal Server Error/
--- error_log eval
[
    qr/\[crit\] .*? panicked at/,
    qr/unexpected status: 1/
]
--- no_error_log
[emerg]
