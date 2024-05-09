# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasmX;

plan_tests(5);
run_tests();

__DATA__

=== TEST 1: proxy_wasm - record_metric() counter
--- valgrind
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'on_configure=define_metrics \
                              on=request_headers \
                              test=/t/metrics/toggle_counters \
                              metrics=c1,c2';
        echo ok;
    }
--- error_code: 500
--- error_log eval
[
    qr/host trap \(internal error\): could not record value "[0-9]+" on metric with id "[0-9]+"; cannot record on counters.*/,
]
--- no_error_log
[crit]
[emerg]
[alert]



=== TEST 2: proxy_wasm - record_metric() invalid metric id
--- valgrind
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'on=request_headers \
                              test=/t/metrics/set_invalid_gauge';
        echo ok;
    }
--- error_code: 500
--- error_log eval
[
    qr/host trap \(internal error\): could not record value "[0-9]+" on metric with id "[0-9]+"; not found.*/,
]
--- no_error_log
[crit]
[emerg]
[alert]
