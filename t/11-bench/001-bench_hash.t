# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasmX;

plan_tests(4);
run_tests();

__DATA__

=== TEST 1: bench - mandelbrot
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: benchmarks
--- config
    location /t {
        proxy_wasm benchmarks;
        echo fail;
    }
--- request
GET /t/hash
--- more_headers
X-Prefix: test-prefix-3
--- response_body
test-prefix-3-1873
--- no_error_log
[error]
[crit]
