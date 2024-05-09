# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasmX;

skip_hup();

our $workers = 2;

workers($workers);
master_on();

plan_tests(6);
run_tests();

__DATA__

=== TEST 1: proxy_wasm - increment_metric() sanity
A counter's value should reflect increments made by all workers.

--- valgrind
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'on_configure=define_and_increment_counters \
                              metrics=c1';
        echo ok;
    }
--- error_log eval
qr/c1: $::workers at Configure/
--- no_error_log
[error]
[crit]
[emerg]
[alert]
