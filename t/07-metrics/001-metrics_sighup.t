# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:
use strict;
use lib '.';
use t::TestWasmX;

skip_no_hup();

our $workers = 2;

our $metrics = "c2,g2,h2";

workers($workers);
if ($workers > 1) {
    master_on();
}

no_shuffle();
plan_tests(8);
run_tests();

__DATA__

=== TEST 1: SIGHUP metrics - define metrics and increment counters
Evaluating counters values in error_log rather than in response headers as some
worker(s) might not have done their increment by the time response_headers phase
is invoked.

--- valgrind
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config eval
qq{
    location /t {
        proxy_wasm hostcalls 'on_configure=define_and_increment_counters \
                              metrics=$::metrics';
        echo ok;
    }
}
--- error_log eval
qr/c2: $::workers.*/
--- no_error_log
[error]
[crit]
[emerg]
[alert]
[stub]
[stub]



=== TEST 2: SIGHUP metrics - shm preserved, no realloc
--- valgrind
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config eval
qq{
    location /t {
        proxy_wasm hostcalls 'on_configure=define_and_increment_counters \
                              on=response_headers \
                              test=/t/metrics/get \
                              metrics=$::metrics';
        echo ok;
    }
}
--- response_headers
c1: 4
c2: 4
--- no_error_log
reallocating metric
[error]
[crit]
[emerg]
[alert]



=== TEST 3: SIGHUP metrics - increased worker_processes - shm preserved, realloc
--- workers: 4
--- valgrind
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config eval
qq{
    location /t {
        proxy_wasm hostcalls 'on_configure=define_and_increment_counters \
                              on=response_headers \
                              test=/t/metrics/get \
                              metrics=$::metrics';
        echo ok;
    }
}
--- response_headers
c1: 8
c2: 8
--- error_log: reallocating metric
--- no_error_log
[error]
[crit]
[emerg]
[alert]



=== TEST 4: SIGHUP metrics - decreased worker_processes - shm preserved, realloc
--- workers: 2
--- valgrind
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config eval
qq{
    location /t {
        proxy_wasm hostcalls 'on_configure=define_and_increment_counters \
                              on=response_headers \
                              test=/t/metrics/get \
                              metrics=$::metrics';
        echo ok;
    }
}
--- response_headers
c1: 10
c2: 10
--- error_log: reallocating metric
--- no_error_log
[error]
[crit]
[emerg]
[alert]



=== TEST 5: SIGHUP metrics - decreased slab_size - shm preserved, realloc
--- workers: 2
--- valgrind
--- load_nginx_modules: ngx_http_echo_module
--- main_config eval
qq{
    wasm {
        module hostcalls $ENV{TEST_NGINX_CRATES_DIR}/hostcalls.wasm;

        metrics {
            slab_size 4m;
        }
    }
}
--- config eval
qq{
    location /t {
        proxy_wasm hostcalls 'on_configure=define_and_increment_counters \
                              on=response_headers \
                              test=/t/metrics/get \
                              metrics=$::metrics';
        echo ok;
    }
}
--- response_headers
c1: 12
c2: 12
--- error_log: reallocating metric
--- no_error_log
[error]
[crit]
[emerg]
[alert]



=== TEST 6: SIGHUP metrics - increased slab_size - shm preserved, realloc
--- workers: 2
--- valgrind
--- load_nginx_modules: ngx_http_echo_module
--- main_config eval
qq{
    wasm {
        module hostcalls $ENV{TEST_NGINX_CRATES_DIR}/hostcalls.wasm;

        metrics {
            slab_size 5m;
        }
    }
}
--- config eval
qq{
    location /t {
        proxy_wasm hostcalls 'on_configure=define_and_increment_counters \
                              on=response_headers \
                              test=/t/metrics/get \
                              metrics=$::metrics';
        echo ok;
    }
}
--- response_headers
c1: 14
c2: 14
--- error_log: reallocating metric
--- no_error_log
[error]
[crit]
[emerg]
[alert]
