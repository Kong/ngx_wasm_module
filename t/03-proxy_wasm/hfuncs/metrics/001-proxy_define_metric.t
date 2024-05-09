# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasmX;

our $workers = 2;

workers($workers);
if ($workers > 1) {
    master_on();
}

plan_tests(4);
run_tests();

__DATA__

=== TEST 1: proxy_wasm - define_metric() counter
--- valgrind
--- wasm_modules: hostcalls
--- load_nginx_modules: ngx_http_echo_module
--- config
    location /t {
        proxy_wasm hostcalls 'on_configure=define_metrics \
                              metrics=c1';
        echo ok;
    }
--- grep_error_log eval: qr/\["hostcalls" \#\d\] defined metric \w+ as \d+/
--- grep_error_log_out eval
qr/.*? defined metric c1 as \d+/
--- no_error_log
[error]
[crit]



=== TEST 2: proxy_wasm - define_metric() gauge
--- valgrind
--- wasm_modules: hostcalls
--- load_nginx_modules: ngx_http_echo_module
--- config
    location /t {
        proxy_wasm hostcalls 'on_configure=define_metrics \
                              metrics=g1';
        echo ok;
    }
--- grep_error_log eval: qr/\["hostcalls" \#\d\] defined metric \w+ as \d+/
--- grep_error_log_out eval
qr/.*? defined metric g1 as \d+/
--- no_error_log
[error]
[crit]



=== TEST 3: proxy_wasm - define_metric() histogram
--- valgrind
--- wasm_modules: hostcalls
--- load_nginx_modules: ngx_http_echo_module
--- config
    location /t {
        proxy_wasm hostcalls 'on_configure=define_metrics \
                              metrics=h1';
        echo ok;
    }
--- grep_error_log eval: qr/\["hostcalls" \#\d\] defined metric \w+ as \d+/
--- grep_error_log_out eval
qr/.*? defined metric h1 as \d+/
--- no_error_log
[error]
[crit]



=== TEST 4: proxy_wasm - define_metric() redefinition
Definition happens only once, a second call defining an existing metric simply returns its id.
--- skip_no_debug
--- valgrind
--- wasm_modules: hostcalls
--- load_nginx_modules: ngx_http_echo_module
--- config
    location /t {
        proxy_wasm hostcalls 'on_configure=define_metrics \
                              metrics=c1,g1,h1';
        proxy_wasm hostcalls 'on_configure=define_metrics \
                              metrics=c1,g1,h1';
        echo ok;
    }
--- error_log eval
qr/wasm returning existing metric id "\d+"/
--- no_error_log
[error]
[crit]
