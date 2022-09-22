# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

skip_valgrind();

plan tests => repeat_each() * (blocks() * 12);

run_tests();

__DATA__

=== TEST 1: proxy_wasm shm - kv empty value and deletion should be different
--- wasm_modules: hostcalls
--- load_nginx_modules: ngx_http_echo_module
--- shm_kv
kv1 1048576
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data key=kv1/test cas=0 value=';
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data key=kv1/test cas_header=x-cas-1 data_header=x-data-1 exists_header=x-exists-1';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data key=kv1/test cas=1';
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data key=kv1/test cas_header=x-cas-2 data_header=x-data-2 exists_header=x-exists-2';
        echo ok;
    }
--- response_headers
x-cas-1: 1
x-data-1:
x-exists-1: 1
x-cas-2: 0
x-data-2:
x-exists-2: 0
--- ignore_response_body
--- no_error_log
[error]
[crit]
stub
stub
stub



=== TEST 2: proxy_wasm shm - kv cas
--- wasm_modules: hostcalls
--- load_nginx_modules: ngx_http_echo_module
--- shm_kv
kv1 1048576
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data key=kv1/test value=hello ok_header=x-ok-1';
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data key=kv1/test cas_header=x-cas-1 data_header=x-data-1';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data key=kv1/test value=world ok_header=x-ok-2';
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data key=kv1/test cas_header=x-cas-2 data_header=x-data-2';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data key=kv1/test cas=1 value=world ok_header=x-ok-3';
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data key=kv1/test cas_header=x-cas-3 data_header=x-data-3';
        echo ok;
    }
--- response_headers
x-ok-1: 1
x-cas-1: 1
x-data-1: hello
x-ok-2: 0
x-cas-2: 1
x-data-2: hello
x-ok-3: 1
x-cas-3: 2
x-data-3: world
--- ignore_response_body
--- no_error_log
[error]
[crit]



=== TEST 3: proxy_wasm shm - kv set non-existing namespace, default fallback
--- wasm_modules: hostcalls
--- load_nginx_modules: ngx_http_echo_module
--- shm_kv
* 64k
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data key=kv2/test value=hello';
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data key=kv2/test cas_header=x-cas-1 data_header=x-data-1 exists_header=x-exists-1';
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
stub
stub
stub
