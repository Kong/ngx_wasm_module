# vim:set ft= ts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

skip_valgrind();

plan tests => repeat_each() * (blocks() * 7);

run_tests();

__DATA__

=== TEST 1: proxy_log() logs all levels
--- wasm_modules: hostcalls
--- load_nginx_modules: ngx_http_echo_module
--- config
    location /t {
        proxy_wasm hostcalls;
        echo ok;
    }
--- request
GET /t/log/levels
--- error_log eval
[
    qr/\[debug\] .*? \[wasm\] proxy_log trace/,
    qr/\[info\] .*? \[wasm\] proxy_log info/,
    qr/\[warn\] .*? \[wasm\] proxy_log warn/,
    qr/\[error\] .*? \[wasm\] proxy_log error/,
    qr/\[crit\] .*? \[wasm\] proxy_log critical/
]
--- no_error_log
[alert]
