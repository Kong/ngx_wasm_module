# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasmX;

plan_tests(4);
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
qr/host trap \(internal error\): could not increment metric id "\d+": metric not a counter/
--- no_error_log
[crit]
[emerg]



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
qr/host trap \(internal error\): could not increment metric id "\d+": metric not found/
--- no_error_log
[crit]
[emerg]
