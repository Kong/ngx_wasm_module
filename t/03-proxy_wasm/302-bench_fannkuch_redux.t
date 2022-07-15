# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

skip_valgrind();

plan tests => repeat_each() * (blocks() * 4);

run_tests();

__DATA__

=== TEST 1: proxy_wasm - bench - fannkuch_redux
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: benchmarks
--- config
    location /t {
        proxy_wasm benchmarks;
        echo fail;
    }
--- request
GET /t/fannkuch_redux
--- more_headers
X-N: 9
--- response_body
8629
Pfannkuchen(9) = 30
--- no_error_log
[error]
[crit]
