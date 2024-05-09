# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasmX;

plan_tests(6);
run_tests();

__DATA__

=== TEST 1: proxy_wasm metrics - get_metric() invalid metric id
--- valgrind
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'on=request_headers \
                              test=/t/metrics/get_invalid_metric';
        echo ok;
    }
--- error_code: 500
--- error_log eval
[
    qr/host trap \(internal error\): could not retrieve metric with id "[0-9]+"; not found.*/,
]
--- no_error_log
[crit]
[emerg]
[alert]
[stub]



=== TEST 2: proxy_wasm metrics - get_metric() histogram
--- valgrind
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'on=request_headers \
                              test=/t/metrics/get_histogram';
        echo ok;
    }
--- error_code: 500
--- error_log eval
[
    qr/host trap \(internal error\): could not retrieve metric with id "[0-9]+"; cannot retrieve histograms.*/,
]
--- no_error_log
[crit]
[emerg]
[alert]
[stub]
