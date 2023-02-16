# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

skip_valgrind();
skip_no_tinygo();

plan tests => repeat_each() * (blocks() * 5);

run_tests();

__DATA__

=== TEST 1: proxy_wasm Go SDK - http_routing example (configuration A)
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: go_http_routing
--- config
    location /t {
        proxy_wasm go_http_routing 'A';
        echo ok;
    }
--- more_headers
content-type: text/html
--- request
POST /t

{ "my_key": "my_value" }
--- response_body
ok
--- error_log eval
qr/\[info\] .*? successfully loaded "go_http_routing"/
--- no_error_log
[error]
[crit]



=== TEST 2: proxy_wasm Go SDK - http_routing example (configuration B)
--- SKIP: fails with 'cannot set read-only ":authority" header'
--- wasm_modules: go_http_routing
--- load_nginx_modules: ngx_http_echo_module
--- config
    location /t {
        proxy_wasm go_http_routing 'B';
        echo ok;
    }
--- more_headers
content-type: text/html
--- request
POST /t

{ "my_key": "my_value" }
--- response_body
ok
--- error_log eval
qr/\[info\] .*? successfully loaded "go_http_routing"/
--- no_error_log
[error]
[crit]
