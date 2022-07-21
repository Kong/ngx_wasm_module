# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

skip_valgrind();

plan tests => repeat_each() * (blocks() * 4);

run_tests();

__DATA__

=== TEST 1:  bench - mandelbrot
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
X-Prefix: test-long-long-long-prefix
--- response_body
test-long-long-long-prefix-72923
--- no_error_log
[error]
[crit]
