# vim:set ft= ts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

skip_valgrind();

plan tests => repeat_each() * (blocks() * 7);

run_tests();

__DATA__

=== TEST 1: proxy_wasm - on_tick
--- load_nginx_modules: ngx_http_echo_module
--- main_config
    wasm {
        module on_tick $TEST_NGINX_CRATES_DIR/on_tick.wasm;
    }

    timer_resolution 10ms;
--- config
    location /t {
        proxy_wasm on_tick;
        echo_sleep 1;
        echo_status 200;
    }
--- ignore_response_body
--- error_log eval
[
    qr/\[info\] .*? \[wasm\] ticking/,
    qr/\[info\] .*? \[wasm\] ticking/,
    qr/\[info\] .*? \[wasm\] ticking/
]
--- no_error_log
[error]
[emerg]
[alert]



=== TEST 2: proxy_wasm - on_tick + http phases updates log context
--- load_nginx_modules: ngx_http_echo_module
--- main_config
    wasm {
        module on_tick $TEST_NGINX_CRATES_DIR/on_tick.wasm;
    }

    timer_resolution 10ms;
--- config
    location /t {
        proxy_wasm on_tick;
        echo_sleep 1;
        echo_status 200;
    }
--- ignore_response_body
--- error_log eval
[
    qr/\[info\] .*? \[wasm\] ticking \<.*?\>$/,
    qr/\[info\] .*? \[wasm\] from http_request_headers \<.*?\>, client: .*?, server: .*?, request:/,
    qr/\[info\] .*? \[wasm\] from http_request_headers \<.*?\>, client: .*?, server: .*?, request:/,
    qr/\[info\] .*? \[wasm\] ticking \<.*?\>$/,
]
--- no_error_log
[error]
[alert]



=== TEST 3: proxy_wasm - a long tick period does not expire
--- load_nginx_modules: ngx_http_echo_module
--- main_config
    wasm {
        module on_phases $TEST_NGINX_CRATES_DIR/on_phases.wasm;
    }

    timer_resolution 200ms;
--- config
    location /t {
        proxy_wasm on_phases 'tick_period=50000';
        echo_sleep 0.150;
        echo ok;
    }
--- response_body
ok
--- no_error_log
on_tick 50000
[warn]
[error]
[crit]
[emerg]



=== TEST 4: proxy_wasm - multiple filters with ticks
--- load_nginx_modules: ngx_http_echo_module
--- main_config
    wasm {
        module on_phases $TEST_NGINX_CRATES_DIR/on_phases.wasm;
    }

    timer_resolution 10ms;
--- config
    location /t {
        proxy_wasm on_phases 'tick_period=20';
        proxy_wasm on_phases 'tick_period=30';
        echo_sleep 0.150;
        echo ok;
    }
--- response_body
ok
--- error_log eval
[
    qr/\[info\] .*? \[wasm\] on_tick 20/,
    qr/\[info\] .*? \[wasm\] on_tick 30/,
    qr/\[info\] .*? \[wasm\] on_tick 20/,
    qr/\[info\] .*? \[wasm\] on_tick 30/,
]
--- no_error_log
[error]
