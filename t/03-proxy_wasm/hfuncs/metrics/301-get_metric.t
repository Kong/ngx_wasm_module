# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasmX;

plan_tests(6);
run_tests();

__DATA__

=== TEST 1: proxy_wasm metrics - get_metric() counter
--- valgrind
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'on_configure=define_and_increment_counters \
                              metrics=c1 \
                              on=request_headers \
                              test=/t/log/metrics';
        echo ok;
    }
--- grep_error_log eval: qr/\["hostcalls" \#\d\] c1: 1.*/
--- grep_error_log_out eval
qr/.*? c1: 1 .*/
--- no_error_log
[crit]
[emerg]
[alert]
[stub]



=== TEST 2: proxy_wasm metrics - get_metric() gauge
--- valgrind
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'on_configure=define_and_toggle_gauges \
                              metrics=g1 \
                              on=request_headers \
                              test=/t/log/metrics';
        echo ok;
    }
--- grep_error_log eval: qr/\["hostcalls" \#\d\] g1: 1.*/
--- grep_error_log_out eval
qr/.*? g1: 1 .*/
--- no_error_log
[crit]
[emerg]
[alert]
[stub]
