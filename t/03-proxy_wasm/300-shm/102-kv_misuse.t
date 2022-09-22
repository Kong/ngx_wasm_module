# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

skip_valgrind();

plan tests => repeat_each() * (blocks() * 4);

run_tests();

__DATA__

=== TEST 1: proxy_wasm shm - kv set non-existing namespace
--- wasm_modules: hostcalls
--- load_nginx_modules: ngx_http_echo_module
--- shm_kv
kv1 64k
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data key=kv2/test value=hello';
        echo ok;
    }
--- error_code: 500
--- response_body eval
qr/500 Internal Server Error/
--- error_log
cannot resolve the namespace for key "kv2/test"
panicked at 'unexpected status: 1'



=== TEST 2: proxy_wasm shm - kv misuse queue shm
--- wasm_modules: hostcalls
--- load_nginx_modules: ngx_http_echo_module
--- shm_queue
queue1 64k
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data key=queue1/test value=hello';
        echo ok;
    }
--- error_code: 500
--- response_body eval
qr/500 Internal Server Error/
--- error_log
shm "queue1" is not a kv
panicked at 'unexpected status: 1'
