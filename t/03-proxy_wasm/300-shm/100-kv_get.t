# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

skip_valgrind();

plan tests => repeat_each() * (blocks() * 9);

run_tests();

__DATA__

=== TEST 1: proxy_wasm shm - kv get non-existing value
--- wasm_modules: hostcalls
--- load_nginx_modules: ngx_http_echo_module
--- shm_kv
kv1 1048576
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data key=kv1/test value=hello';
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data key=kv1/test cas_header=x-cas-1 data_header=x-data-1 exists_header=x-exists-1';
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data key=kv1/test2 cas_header=x-cas-2 data_header=x-data-2 exists_header=x-exists-2';
        echo ok;
    }
--- response_headers
x-cas-1: 1
x-data-1: hello
x-exists-1: 1
x-cas-2: 0
x-data-2:
x-exists-2: 0
--- ignore_response_body
--- no_error_log
[error]
[crit]



=== TEST 2: proxy_wasm shm - kv get default namespace
--- wasm_modules: hostcalls
--- load_nginx_modules: ngx_http_echo_module
--- shm_kv
* 64k
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data key=test value=hello';
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data key=test cas_header=x-cas-1 data_header=x-data-1 exists_header=x-exists-1';
        echo ok;
    }
--- response_headers
x-cas-1: 1
x-data-1: hello
x-exists-1: 1
--- ignore_response_body
--- no_error_log
[error]
[crit]
stub
stub
stub



=== TEST 3: proxy_wasm shm - kv get non-existing namespace
--- wasm_modules: hostcalls
--- load_nginx_modules: ngx_http_echo_module
--- shm_kv
kv1 64k
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data key=kv2/test cas_header=x-cas-1 data_header=x-data-1 exists_header=x-exists-1';
        echo ok;
    }
--- response_headers
x-exists-1: 0
--- ignore_response_body
--- error_log
cannot resolve the namespace for key "kv2/test"
--- no_error_log
[crit]
stub
stub
stub
stub
stub
