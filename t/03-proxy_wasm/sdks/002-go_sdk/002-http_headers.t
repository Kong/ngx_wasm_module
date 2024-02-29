# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasmX;

skip_no_go_sdk();

plan_tests(6);
run_tests();

__DATA__

=== TEST 1: proxy_wasm Go SDK - http_headers example
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: go_http_headers
--- config
    location /t {
        proxy_wasm go_http_headers;
        echo ok;
    }
--- response_body
ok
--- error_log eval
[
    qr/\[info\] .*? request header --> test: best/,
    qr/\[info\] .*? response header <-- Server: (nginx|openresty)/,
]
--- no_error_log
[error]
[crit]
