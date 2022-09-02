# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

skip_valgrind();

plan tests => repeat_each() * (blocks() * 4);

run_tests();

__DATA__

=== TEST 1: proxy_wasm key/value shm - set in non-existing namespace
Throws a trap
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- shm_kv: kv1 64k
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data \
                              key=kv2/test \
                              value=hello';
        echo failed;
    }
--- error_code: 500
--- response_body_like: 500 Internal Server Error
--- grep_error_log eval: qr/\[error\] .*/
--- grep_error_log_out eval
qr~\[error\] .*? trap in proxy_on_request_headers:.*? failed setting value to shm \(could not resolve namespace\).*~
--- no_error_log
[crit]



=== TEST 2: proxy_wasm key/value shm - get from non-existing namespace
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- shm_kv: kv1 64k
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data \
                              key=kv2/test';
        echo failed;
    }
--- error_code: 500
--- response_body_like: 500 Internal Server Error
--- grep_error_log eval: qr/\[error\] .*/
--- grep_error_log_out eval
qr~\[error\] .*? trap in proxy_on_request_headers:.*? failed getting value from shm \(could not resolve namespace\).*~
--- no_error_log
[crit]



=== TEST 3: proxy_wasm key/value shm - attempt to enqueue
Throws a trap
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- shm_kv: kv1 1m
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/shm/enqueue \
                              queue=kv1 \
                              value=hello';
        echo failed;
    }
--- error_code: 500
--- response_body_like: 500 Internal Server Error
--- grep_error_log eval: qr/\[error\] .*/
--- grep_error_log_out eval
qr~\[error\] .*? trap in proxy_on_request_headers:.*? attempt to use a key/value shm store as a queue.*~
--- no_error_log
[crit]



=== TEST 4: proxy_wasm key/value shm - attempt to dequeue
Throws a trap
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- shm_kv: kv1 1m
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/shm/dequeue \
                              queue=kv1 \
                              value=hello';
        echo failed;
    }
--- error_code: 500
--- response_body_like: 500 Internal Server Error
--- grep_error_log eval: qr/\[error\] .*/
--- grep_error_log_out eval
qr~\[error\] .*? trap in proxy_on_request_headers:.*? attempt to use a key/value shm store as a queue.*~
--- no_error_log
[crit]
