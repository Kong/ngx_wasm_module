# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

skip_valgrind();

plan tests => repeat_each() * (blocks() * 8);

run_tests();

__DATA__

=== TEST 1: proxy_wasm shm - pop edge case (data size = buffer_size - 1)
--- wasm_modules: hostcalls
--- load_nginx_modules: ngx_http_echo_module
--- shm_queue
test 256k
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/shm/enqueue queue=test status_header=x-status-1 length=253947';
        proxy_wasm hostcalls 'test=/t/shm/dequeue queue=test status_header=x-status-2 exists_header=x-exists-2 data_header=';
        echo ok;
    }
--- response_headers
x-status-1: 0
x-status-2: 0
x-exists-2: 1
--- ignore_response_body
--- error_log
--- no_error_log
[error]
[crit]
circular_read: wrapping around
circular_write: wrapping around



=== TEST 2: proxy_wasm shm - pop edge case (data size = buffer_size)
--- wasm_modules: hostcalls
--- load_nginx_modules: ngx_http_echo_module
--- shm_queue
test 256k
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/shm/enqueue queue=test status_header=x-status-1 length=253948';
        proxy_wasm hostcalls 'test=/t/shm/dequeue queue=test status_header=x-status-2 exists_header=x-exists-2 data_header=';
        echo ok;
    }
--- response_headers
x-status-1: 0
x-status-2: 0
x-exists-2: 1
--- ignore_response_body
--- error_log
circular_read: wrapping around
circular_write: wrapping around
--- no_error_log
[error]
[crit]
