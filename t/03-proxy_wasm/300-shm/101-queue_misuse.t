# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

skip_valgrind();

plan tests => repeat_each() * (blocks() * 4);

run_tests();

__DATA__

=== TEST 1: proxy_wasm queue shm - push to non-existing queue
Wasmtime trap format:
    [error] error while executing ...
    [stacktrace]
    Caused by:
        [trap msg]

Wasmer trap format:
    [error] [trap msg]

V8 trap format:
    [error] Uncaught RuntimeError: [trap msg]
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- shm_queue: test 1m
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/shm/enqueue queue=test2';
        echo failed;
    }
--- error_code: 500
--- response_body_like: 500 Internal Server Error
--- grep_error_log eval: qr/.*?could not find queue.*/
--- grep_error_log_out eval
qr/(\[error\]|Uncaught RuntimeError|\s+).*?host trap \(bad usage\): could not find queue.*/
--- no_error_log
[crit]



=== TEST 2: proxy_wasm queue shm - pop from non-existing queue
Wasmtime trap format:
    [error] error while executing ...
    [stacktrace]
    Caused by:
        [trap msg]

Wasmer trap format:
    [error] [trap msg]

V8 trap format:
    [error] Uncaught RuntimeError: [trap msg]
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- shm_queue: test 1m
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/shm/dequeue queue=test2';
        echo failed;
    }
--- error_code: 500
--- response_body_like: 500 Internal Server Error
--- grep_error_log eval: qr/.*?could not find queue.*/
--- grep_error_log_out eval
qr/(\[error\]|Uncaught RuntimeError|\s+).*?host trap \(bad usage\): could not find queue.*/
--- no_error_log
[crit]



=== TEST 3: proxy_wasm queue shm - push with raw token (oob index)
Wasmtime trap format:
    [error] error while executing ...
    [stacktrace]
    Caused by:
        [trap msg]

Wasmer trap format:
    [error] [trap msg]

V8 trap format:
    [error] Uncaught RuntimeError: [trap msg]
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- shm_queue: test 1m
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/shm/enqueue \
                              queue_id=999 \
                              value=hello';
        echo failed;
    }
--- error_code: 500
--- response_body_like: 500 Internal Server Error
--- grep_error_log eval: qr/.*?could not find queue.*/
--- grep_error_log_out eval
qr/(\[error\]|Uncaught RuntimeError|\s+).*?host trap \(bad usage\): could not find queue.*/
--- no_error_log
[crit]



=== TEST 4: proxy_wasm queue shm - pop with raw token (oob index)
Wasmtime trap format:
    [error] error while executing ...
    [stacktrace]
    Caused by:
        [trap msg]

Wasmer trap format:
    [error] [trap msg]

V8 trap format:
    [error] Uncaught RuntimeError: [trap msg]
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- shm_queue: test 1m
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/shm/dequeue \
                              queue_id=999';
        echo failed;
    }
--- error_code: 500
--- response_body_like: 500 Internal Server Error
--- grep_error_log eval: qr/.*?could not find queue.*/
--- grep_error_log_out eval
qr/(\[error\]|Uncaught RuntimeError|\s+).*?host trap \(bad usage\): could not find queue.*/
--- no_error_log
[crit]



=== TEST 5: proxy_wasm queue shm - attempt to set key/value
Wasmtime trap format:
    [error] error while executing ...
    [stacktrace]
    Caused by:
        [trap msg]

Wasmer trap format:
    [error] [trap msg]

V8 trap format:
    [error] Uncaught RuntimeError: [trap msg]
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- shm_queue: queue1 64k
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data \
                              key=queue1/test \
                              value=hello';
        echo failed;
    }
--- error_code: 500
--- response_body_like: 500 Internal Server Error
--- grep_error_log eval: qr/.*?attempt to set.*/
--- grep_error_log_out eval
qr~(\[error\]|Uncaught RuntimeError|\s+).*?host trap \(bad usage\): attempt to set key/value in a queue.*~
--- no_error_log
[crit]



=== TEST 6: proxy_wasm queue shm - attempt to get key/value
Wasmtime trap format:
    [error] error while executing ...
    [stacktrace]
    Caused by:
        [trap msg]

Wasmer trap format:
    [error] [trap msg]

V8 trap format:
    [error] Uncaught RuntimeError: [trap msg]
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- shm_queue: queue1 64k
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data \
                              key=queue1/test';
        echo failed;
    }
--- error_code: 500
--- response_body_like: 500 Internal Server Error
--- grep_error_log eval: qr/.*?attempt to get.*/
--- grep_error_log_out eval
qr~(\[error\]|Uncaught RuntimeError|\s+).*?host trap \(bad usage\): attempt to get key/value from a queue.*~
--- no_error_log
[crit]



=== TEST 7: proxy_wasm queue shm - attempt to get key/value
Wasmtime trap format:
    [error] error while executing ...
    [stacktrace]
    Caused by:
        [trap msg]

Wasmer trap format:
    [error] [trap msg]

V8 trap format:
    [error] Uncaught RuntimeError: [trap msg]
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- shm_queue: queue1 64k
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data \
                              key=queue1/test';
        echo failed;
    }
--- error_code: 500
--- response_body_like: 500 Internal Server Error
--- grep_error_log eval: qr/.*?attempt to get.*/
--- grep_error_log_out eval
qr~(\[error\]|Uncaught RuntimeError|\s+).*?host trap \(bad usage\): attempt to get key/value from a queue.*~
--- no_error_log
[crit]



=== TEST 8: proxy_wasm queue shm - attempt to enqueue in a key/value shm by id
Wasmtime trap format:
    [error] error while executing ...
    [stacktrace]
    Caused by:
        [trap msg]

Wasmer trap format:
    [error] [trap msg]

V8 trap format:
    [error] Uncaught RuntimeError: [trap msg]
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- shm_kv: kv 64k
--- shm_queue: queue 64k
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/shm/enqueue \
                              queue_id=0 \
                              value=hello';
        echo failed;
    }
--- error_code: 500
--- response_body_like: 500 Internal Server Error
--- grep_error_log eval: qr/.*?attempt to use a.*/
--- grep_error_log_out eval
qr~(\[error\]|Uncaught RuntimeError|\s+).*?host trap \(bad usage\): attempt to use a key/value shm store as a queue.*~
--- no_error_log
[crit]



=== TEST 9: proxy_wasm queue shm - attempt to dequeue from a key/value shm by id
Wasmtime trap format:
    [error] error while executing ...
    [stacktrace]
    Caused by:
        [trap msg]

Wasmer trap format:
    [error] [trap msg]

V8 trap format:
    [error] Uncaught RuntimeError: [trap msg]
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- shm_kv: kv 64k
--- shm_queue: queue 64k
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/shm/dequeue \
                              queue_id=0 \
                              value=hello';
        echo failed;
    }
--- error_code: 500
--- response_body_like: 500 Internal Server Error
--- grep_error_log eval: qr/.*?attempt to use a.*/
--- grep_error_log_out eval
qr~(\[error\]|Uncaught RuntimeError|\s+).*?host trap \(bad usage\): attempt to use a key/value shm store as a queue.*~
--- no_error_log
[crit]
