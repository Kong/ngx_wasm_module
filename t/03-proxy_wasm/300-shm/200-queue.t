# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

skip_valgrind();

plan tests => repeat_each() * 45;

run_tests();

__DATA__

=== TEST 1: proxy_wasm shm - queue push and pop
--- wasm_modules: hostcalls
--- load_nginx_modules: ngx_http_echo_module
--- shm_queue
test 1048576
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/shm/enqueue queue=test status_header=x-status-1 value=hello';
        proxy_wasm hostcalls 'test=/t/shm/dequeue queue=test status_header=x-status-2';
        echo ok;
    }
--- response_headers
x-status-1: 0
x-status-2: 0
x-data: hello
x-exists: 1
--- ignore_response_body
--- no_error_log
[error]
[crit]
wrapping around



=== TEST 2: proxy_wasm shm - pop from empty queue
--- wasm_modules: hostcalls
--- load_nginx_modules: ngx_http_echo_module
--- shm_queue
test 1048576
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/shm/dequeue queue=test';
        echo ok;
    }
--- response_headers
x-status: 0
x-data:
x-exists: 0
--- ignore_response_body
--- no_error_log
[error]
[crit]
wrapping around



=== TEST 3: proxy_wasm shm - buffer size assertion
--- wasm_modules: hostcalls
--- load_nginx_modules: ngx_http_echo_module
--- shm_queue
test 262144
--- config
    location /t {
        echo ok;
    }
--- ignore_response_body
--- error_log
[wasm] shm queue 'test': available buffer size: 253952
--- no_error_log
[error]
[crit]



=== TEST 4: proxy_wasm shm - push edge case (data size = buffer_size - 1)
--- wasm_modules: hostcalls
--- load_nginx_modules: ngx_http_echo_module
--- shm_queue
test 262144
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/shm/enqueue queue=test status_header=x-status-1 length=253947';
        echo ok;
    }
--- response_headers
x-status-1: 0
--- ignore_response_body
--- no_error_log
[error]
[crit]
circular_write: wrapping around



=== TEST 5: proxy_wasm shm - push edge case (data size = buffer_size)
--- wasm_modules: hostcalls
--- load_nginx_modules: ngx_http_echo_module
--- shm_queue
test 262144
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/shm/enqueue queue=test status_header=x-status-1 length=253948';
        echo ok;
    }
--- response_headers
x-status-1: 0
--- ignore_response_body
--- error_log
circular_write: wrapping around
--- no_error_log
[error]
[crit]



=== TEST 6: proxy_wasm shm - pop edge case (data size = buffer_size - 1)
--- wasm_modules: hostcalls
--- load_nginx_modules: ngx_http_echo_module
--- shm_queue
test 262144
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



=== TEST 7: proxy_wasm shm - pop edge case (data size = buffer_size)
--- wasm_modules: hostcalls
--- load_nginx_modules: ngx_http_echo_module
--- shm_queue
test 262144
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
