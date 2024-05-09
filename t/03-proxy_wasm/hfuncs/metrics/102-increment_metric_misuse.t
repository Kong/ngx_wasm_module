# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasmX;

plan_tests(5);
run_tests();

__DATA__

=== TEST 1: proxy_wasm - increment_metric() gauge
--- valgrind
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config eval
qq{
    location /t {
        proxy_wasm hostcalls 'on_configure=define_metrics \
                              on=request_headers \
                              test=/t/metrics/increment_gauges \
                              metrics=g1,g2';
        echo ok;
    }
}
--- error_code: 500
--- error_log eval
[
    qr/host trap \(internal error\): could not increment by "[0-9]+" metric with id "[0-9]+"; can only increment counters.*/,
]
--- no_error_log
[crit]
[emerg]
[alert]



=== TEST 2: proxy_wasm - increment_metric() invalid metric id
--- valgrind
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'on=request_headers \
                              test=/t/metrics/increment_invalid_counter';
        echo ok;
    }
--- error_code: 500
--- error_log eval
[
    qr/host trap \(internal error\): could not increment by "[0-9]+" metric with id "[0-9]+"; not found.*/,
]
--- no_error_log
[crit]
[emerg]
[alert]
