# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

plan_tests(6);
run_tests();

__DATA__

=== TEST 1: proxy_wasm queue shm - push when data_size == buffer_size
--- skip_no_debug
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- shm_queue: test 262144
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/shm/enqueue \
                              queue=test \
                              length=253948';
        echo ok;
    }
--- response_headers
status-enqueue: 0
--- response_body
ok
--- error_log
circular_write: wrapping around
--- no_error_log
[error]
[crit]



=== TEST 2: proxy_wasm queue shm - push when data size == buffer_size + 1
--- valgrind
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- shm_queue: test 262144
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/shm/enqueue \
                              queue=test \
                              length=253949';
        echo ok;
    }
--- error_code: 500
--- response_body_like: 500 Internal Server Error
--- grep_error_log eval: qr/.*?could not enqueue.*/
--- grep_error_log_out eval
qr~(\[error\]|Uncaught RuntimeError|\s+).*?host trap \(internal error\): could not enqueue \(queue is full\).*~
--- no_error_log
[crit]
[emerg]
circular_write: wrapping around



=== TEST 3: proxy_wasm queue shm - push when data size == buffer_size - 1
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- shm_queue: test 262144
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/shm/enqueue \
                              queue=test \
                              length=253947';
        echo ok;
    }
--- response_headers
status-enqueue: 0
--- response_body
ok
--- no_error_log
[error]
[crit]
circular_write: wrapping around
