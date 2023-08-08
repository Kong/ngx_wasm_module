# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

skip_valgrind();

plan tests => repeat_each() * (blocks() * 10);

run_tests();

__DATA__

=== TEST 1: proxy_wasm queue shm - pop when data size == buffer_size - 1
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- shm_queue: test 256k
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/shm/enqueue \
                              queue=test \
                              length=253947';
        proxy_wasm hostcalls 'test=/t/shm/dequeue \
                              queue=test';
        echo ok;
    }
--- response_headers
status-enqueue: 0
status-dequeue: 0
exists: 1
--- response_body
ok
--- no_error_log
[error]
[crit]
circular_read: wrapping around
circular_write: wrapping around
[stub]



=== TEST 2: proxy_wasm queue shm - pop when data size == buffer_size
--- skip_no_debug: 10
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- shm_queue: test 256k
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/shm/enqueue \
                              queue=test \
                              length=253948';
        proxy_wasm hostcalls 'test=/t/shm/dequeue \
                              queue=test';
        echo ok;
    }
--- response_headers
status-enqueue: 0
status-dequeue: 0
exists: 1
--- response_body
ok
--- error_log
circular_read: wrapping around
circular_write: wrapping around
--- no_error_log
[error]
[crit]
[emerg]
