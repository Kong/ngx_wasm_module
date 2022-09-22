# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

skip_valgrind();

plan tests => repeat_each() * (blocks() * 5);

run_tests();

__DATA__

=== TEST 1: proxy_wasm shm - push edge case (data size = buffer_size)
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



=== TEST 2: proxy_wasm shm - push edge case (data size = buffer_size + 1)
--- wasm_modules: hostcalls
--- load_nginx_modules: ngx_http_echo_module
--- shm_queue
test 262144
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/shm/enqueue queue=test status_header=x-status-1 length=253949';
        echo ok;
    }
--- error_code: 500
--- response_body eval
qr/500 Internal Server Error/
--- error_log
shm queue "test": queue full, cannot push
panicked at 'unexpected status: 10'
--- no_error_log
stub



=== TEST 3: proxy_wasm shm - push edge case (data size = buffer_size - 1)
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
