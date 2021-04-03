# vim:set ft= ts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

skip_valgrind();

plan tests => repeat_each() * (blocks() * 4);

run_tests();

__DATA__

=== TEST 1: proxy_wasm - get_current_time()
--- wasm_modules: hostcalls
--- load_nginx_modules: ngx_http_echo_module
--- config
    location /t {
        proxy_wasm hostcalls;
        echo ok;
    }
--- request
GET /t/log_current_time
--- error_log eval
[
    qr/\[info\] .*? \[wasm\] now: 2.*? UTC/,
]
--- no_error_log
[error]
[alert]
