# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

plan tests => repeat_each() * (blocks() * 7);

run_tests();

__DATA__

=== TEST 1: proxy_wasm - on_tick
Tick is triggered even without hitting the configured location
since it runs at root level context.
--- load_nginx_modules: ngx_http_echo_module
--- main_config
    wasm {
        module on_tick $TEST_NGINX_CRATES_DIR/on_tick.wasm;
    }

    timer_resolution 10ms;
--- config
    location /tick {
        internal;
        proxy_wasm on_tick;
    }

    location /t {
        echo_sleep 1;
        echo_status 200;
    }
--- ignore_response_body
--- error_log eval
[
    qr/\[info\] \d+#\d+: ticking/,
    qr/\[info\] \d+#\d+: ticking/,
    qr/\[info\] \d+#\d+: ticking/,
]
--- no_error_log
[error]
[crit]
[alert]



=== TEST 2: proxy_wasm - on_tick + http phases updates log context
--- wait: 1
--- load_nginx_modules: ngx_http_echo_module
--- main_config
    timer_resolution 10ms;

    wasm {
        module on_tick $TEST_NGINX_CRATES_DIR/on_tick.wasm;
    }
--- config
    location /t {
        proxy_wasm on_tick;
        echo_sleep 1;
        echo_status 200;
    }
--- ignore_response_body
--- error_log eval
[
    qr/\[info\] \d+#\d+: ticking/,
    qr/\[info\] .*? from request_headers, client: .*?, server: .*?, request:/,
    qr/\[info\] \d+#\d+: ticking/,
]
--- no_error_log
[error]
[crit]
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
[error]
[crit]
[alert]
[stderr]



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
    qr/\[info\] \d+#\d+: on_tick 20/,
    qr/\[info\] \d+#\d+: on_tick 30/,
    qr/\[info\] \d+#\d+: on_tick 20/,
    qr/\[info\] \d+#\d+: on_tick 30/,
]
--- no_error_log
[error]



=== TEST 5: proxy_wasm - multiple ticks
'daemon off' must be set to check exit_code is 2
Valgrind mode already writes 'daemon off'
HUP mode does not catch the worker exit_code

Wasmtime trap format:
    [error] error while executing ...
    [stacktrace]
    Caused by:
        [trap msg]

Wasmer trap format:
    [error] [trap msg]

V8 trap format:
    [error] Uncaught RuntimeError: [trap msg]
--- skip_eval: 7: $ENV{TEST_NGINX_USE_HUP} == 1
--- main_config eval
qq{
    wasm {
        module on_tick $ENV{TEST_NGINX_CRATES_DIR}/on_tick.wasm;
    }
}.(defined $ENV{TEST_NGINX_USE_VALGRIND} ? '' : 'daemon off;')
--- config
    proxy_wasm on_tick 'double_tick';
--- ignore_response_body
--- grep_error_log eval: qr/.*?(\[emerg\]|tick_period already set).*/
--- grep_error_log_out eval
qr/.*?(\[error\]|Uncaught RuntimeError: |\s+)tick_period already set <.*
.*?\[emerg\] .*? proxy_wasm failed initializing \"on_tick\" filter.*/
--- no_error_log
[warn]
[crit]
stub
stub
stub
--- must_die
