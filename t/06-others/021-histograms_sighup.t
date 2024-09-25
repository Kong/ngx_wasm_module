# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:
use strict;
use lib '.';
use t::TestWasmX;

skip_no_hup();
skip_no_debug();

our $workers = 2;
our $total = 0;

workers($workers);
if ($workers > 1) {
    master_on();
}

no_shuffle();
plan_tests(4);
run_tests();

__DATA__

=== TEST 1: SIGHUP metrics - define metrics and record histograms
--- valgrind
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config eval
qq{
    location /t {
        proxy_wasm hostcalls 'on_configure=define_and_record_histograms \
                              metrics=c2,g2,h2';
        echo ok;
    }
}
--- grep_error_log eval: qr/histogram "\d+":( \d+: \d+;)+/
--- grep_error_log_out eval
$::total += $::workers;
qr/histogram "\d+": 1: $::total; 4294967295: 0;/
--- no_error_log
[error]
[crit]



=== TEST 2: SIGHUP metrics - shm preserved, no realloc
--- valgrind
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config eval
qq{
    location /t {
        proxy_wasm hostcalls 'on_configure=define_and_record_histograms \
                              metrics=h2';
        echo ok;
    }
}
--- grep_error_log eval: qr/histogram "\d+":( \d+: \d+;)+/
--- grep_error_log_out eval
$::total += $::workers;
qr/histogram "\d+": 1: $::total; 4294967295: 0;/
--- no_error_log
[error]
[crit]



=== TEST 3: SIGHUP metrics - increased worker_processes - shm preserved, realloc
--- workers: 4
--- valgrind
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config eval
qq{
    location /t {
        proxy_wasm hostcalls 'on_configure=define_and_record_histograms \
                              metrics=h2';
        echo ok;
    }
}
--- grep_error_log eval: qr/histogram "\d+":( \d+: \d+;)+/
--- grep_error_log_out eval
$::total += 4;
qr/histogram "\d+": 1: $::total; 4294967295: 0;/
--- no_error_log
[error]
[crit]



=== TEST 4: SIGHUP metrics - decreased worker_processes - shm preserved, realloc
--- workers: 2
--- valgrind
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config eval
qq{
    location /t {
        proxy_wasm hostcalls 'on_configure=define_and_record_histograms \
                              metrics=h2';
        echo ok;
    }
}
--- grep_error_log eval: qr/histogram "\d+":( \d+: \d+;)+/
--- grep_error_log_out eval
$::total += 2;
qr/histogram "\d+": 1: $::total; 4294967295: 0;/
--- no_error_log
[error]
[crit]



=== TEST 5: SIGHUP metrics - decreased slab_size - shm preserved, realloc
--- valgrind
--- load_nginx_modules: ngx_http_echo_module
--- main_config eval
qq{
    wasm {
        module hostcalls $ENV{TEST_NGINX_CRATES_DIR}/hostcalls.wasm;

        metrics {
            slab_size 120k;
        }
    }
}
--- config eval
qq{
    location /t {
        proxy_wasm hostcalls 'on_configure=define_metrics \
                              on=request_headers \
                              test=/t/metrics/record_histograms \
                              metrics=h2';
        echo ok;
    }
}
--- grep_error_log eval: qr/histogram "\d+":( \d+: \d+;)+/
--- grep_error_log_out eval
$::total += 1;
qr/histogram "\d+": 1: $::total; 4294967295: 0;/
--- no_error_log
[error]
[crit]



=== TEST 6: SIGHUP metrics - increased slab_size - shm preserved, realloc
--- valgrind
--- load_nginx_modules: ngx_http_echo_module
--- main_config eval
qq{
    wasm {
        module hostcalls $ENV{TEST_NGINX_CRATES_DIR}/hostcalls.wasm;

        metrics {
            slab_size 16k;
        }
    }
}
--- config eval
qq{
    location /t {
        proxy_wasm hostcalls 'on_configure=define_metrics \
                              on=response_headers \
                              test=/t/metrics/record_histograms \
                              metrics=h2';
        echo ok;
    }
}
--- grep_error_log eval: qr/histogram "\d+":( \d+: \d+;)+/
--- grep_error_log_out eval
$::total += 1;
qr/histogram "\d+": 1: $::total; 4294967295: 0;/
--- no_error_log
[error]
[crit]
