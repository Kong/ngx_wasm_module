# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

skip_valgrind();

plan tests => repeat_each() * (blocks() * 6);

run_tests();

__DATA__

=== TEST 1: proxy_wasm Go SDK - helloworld example
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: go_helloworld
--- config
    location /t {
        proxy_wasm go_helloworld;
        echo_sleep 1.1;
        echo ok;
    }
--- response_body
ok
--- error_log eval
[
    qr/\[info\] .*? OnPluginStart from Go!/,
    qr/\[info\] .*? It's [0-9]+: random value: [0-9]+/,
]
--- no_error_log
[error]
[crit]
