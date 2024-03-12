# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

plan_tests(5);
run_tests();

# Test cases:
# - assert metrics aren't updated instantly
# - assert increment_metric increments counter's value by the provided offset
# - assert record_metric set gauge's value to the provided value
# - assert metric updates are applied after
#   a. configured timeout
#   b. configured buffer size trigger
#
# - assert increment_metric/record_metric can be called from all phases
# - assert increment_metric/record_metric can be called from root phases, .e.g., on_tick

__DATA__

=== TEST 1: proxy_wasm metrics shm - define a new metric
--- valgrind
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- metrics: 1m
--- main_config
    env WASMTIME_BACKTRACE_DETAILS=1;
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/metrics/increment_counters \
                              counters=10';
        proxy_wasm hostcalls 'test=/t/metrics/get_metric \
                              counters=10 \
                              metric_name=c1';
        echo ok;
    }
--- request eval
["GET /t"]
--- no_error_log
[error]
[crit]
[emerg]
[alert]
