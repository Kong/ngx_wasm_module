# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

skip_valgrind();

plan tests => repeat_each() * (blocks() * 10);

run_tests();

__DATA__

=== TEST 1: proxy_wasm queue shm - push and pop a value
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- shm_queue: test 1m
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/shm/enqueue \
                              queue=test \
                              value=hello';
        proxy_wasm hostcalls 'test=/t/shm/dequeue \
                              queue=test';
        echo ok;
    }
--- response_headers
status-enqueue: 0
status-dequeue: 0
exists: 1
data: hello
--- response_body
ok
--- no_error_log
[error]
[crit]
[emerg]
wrapping around



=== TEST 2: proxy_wasm queue shm - pop from empty queue
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- shm_queue: test 1m
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/shm/dequeue queue=test';
        echo ok;
    }
--- response_headers
status-dequeue: 0
exists: 0
data:
--- response_body
ok
--- no_error_log
[error]
[crit]
[emerg]
wrapping around
stub
