# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasmX;

skip_hup();

workers(2);
master_on();

plan_tests(5);
run_tests();

__DATA__

=== TEST 1: proxy_wasm - record_metric() gauge
A gauge's value is equal to the last value set by any of the workers.
The first filter, bound to worker 0, sets g1 to 1; the second one, bound to
worker 1, sets g1 to 2.

--- skip_no_debug
--- wasm_modules: hostcalls
--- load_nginx_modules: ngx_http_echo_module
--- config
    location /t {
        proxy_wasm hostcalls 'on_configure=define_metrics \
                              on_tick=set_gauges \
                              tick_period=100 \
                              n_sync_calls=1 \
                              on_worker=0 \
                              value=1 \
                              metrics=g1';

        proxy_wasm hostcalls 'on_configure=define_metrics \
                              on_tick=set_gauges \
                              tick_period=500 \
                              n_sync_calls=1 \
                              on_worker=1 \
                              value=2 \
                              metrics=g1';
        echo ok;
    }
--- wait: 1
--- grep_error_log eval: qr/\["hostcalls" \#\d\] record \d+ on g1/
--- grep_error_log_out eval
qr/.*? record 1 on g1.*
.*? record 2 on g1.*/
--- no_error_log
[error]
[crit]
[emerg]



=== TEST 2: proxy_wasm - record_metric() histogram
Record values to a histogram so that each of its bins has counter equals to 1.

--- skip_no_debug
--- valgrind
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config eval
my $filters;

foreach my $exp (0 .. 17) {
    my $v = 2 ** $exp;
    $filters .= "
        proxy_wasm hostcalls 'on_configure=define_metrics \
                              test=/t/metrics/record_histograms \
                              metrics=h1 \
                              value=$v';";
}

qq{
    location /t {
        $filters
        echo ok;
    }
}
--- error_log eval
[
    "growing histogram",
    qr/histogram "\d+": 1: 1; 2: 1; 4: 1; 8: 1; 16: 1; 32: 1; 64: 1; 128: 1; 256: 1; 512: 1; 1024: 1; 2048: 1; 4096: 1; 8192: 1; 16384: 1; 32768: 1; 65536: 1; 4294967295: 1; SUM: 262143/
]
--- no_error_log
[error]
[crit]
