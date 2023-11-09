# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

skip_valgrind();

plan tests => repeat_each() * (blocks() * 9);

run_tests();

__DATA__

=== TEST 1: proxy_wasm - proxy_log() logs all levels
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/log/levels';
        echo ok;
    }
--- response_body
ok
--- error_log eval
[
    qr/\[debug\] .*? \*\d+ \[proxy-wasm\]\["hostcalls" #\d+\] proxy_log trace$/,
    qr/\[info\] .*? \*\d+ \[proxy-wasm\]\["hostcalls" #\d+\] proxy_log info, client:/,
    qr/\[warn\] .*? \*\d+ \[proxy-wasm\]\["hostcalls" #\d+\] proxy_log warn, client:/,
    qr/\[error\] .*? \*\d+ \[proxy-wasm\]\["hostcalls" #\d+\] proxy_log error, client:/,
    qr/\[crit\] .*? \*\d+ \[proxy-wasm\]\["hostcalls" #\d+\] proxy_log critical, client:/
]
--- no_error_log
[alert]
[stub]



=== TEST 2: proxy_wasm - proxy_log() bad log level
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/bad_log_level';
        echo fail;
    }
--- error_code: 500
--- response_body_like: 500 Internal Server Error
--- error_log eval
[
    qr/\[alert\] .*? NYI - proxy_log bad log_level: 100/,
    qr/\[crit\] .*? panicked at/,
    qr/unexpected status: 2/,
]
--- no_error_log
[emerg]
[stub1]
[stub2]
[stub3]
