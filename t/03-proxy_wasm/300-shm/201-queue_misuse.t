# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

skip_valgrind();

plan tests => repeat_each() * (blocks() * 4);

run_tests();

__DATA__

=== TEST 1: proxy_wasm shm - pop from non-existing queue
--- wasm_modules: hostcalls
--- load_nginx_modules: ngx_http_echo_module
--- shm_queue
test 1048576
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/shm/dequeue queue=test2';
        echo ok;
    }
--- error_code: 500
--- response_body eval
qr/500 Internal Server Error/
--- error_log
panicked at 'unexpected status: 1'
--- no_error_log
stub



=== TEST 2: proxy_wasm shm - queue misuse kv
--- wasm_modules: hostcalls
--- load_nginx_modules: ngx_http_echo_module
--- shm_kv
test 1048576
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/shm/enqueue queue=test status_header=x-status-1 value=hello';
        echo ok;
    }
--- error_code: 500
--- response_body eval
qr/500 Internal Server Error/
--- error_log
shm "test" is not a queue
panicked at 'unexpected status: 1'
--- no_error_log
