# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

skip_valgrind();

plan tests => repeat_each() * (blocks() * 12);

run_tests();

__DATA__

=== TEST 1: proxy_wasm key/value shm - get existing and non-existing value
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- shm_kv: kv1 1m
--- config
    location /t {
        # set kv1/test=hello
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data \
                              key=kv1/test \
                              value=hello';
        # get kv1/test
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data \
                              key=kv1/test';
        # get kv1/nop
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data \
                              key=kv1/nop \
                              header_cas=cas-2 \
                              header_data=data-2 \
                              header_exists=exists-2';
        echo ok;
    }
--- response_headers
cas: 1
exists: 1
data: hello
cas-2: 0
exists-2: 0
data-2:
--- response_body
ok
--- no_error_log
[error]
[crit]
[emerg]
[alert]



=== TEST 2: proxy_wasm key/value shm - get existing value in default namespace
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- shm_kv: * 64k
--- config
    location /t {
        # set test=hello
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data \
                              key=test \
                              value=hello';
        # get test
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data \
                              key=test';
        echo ok;
    }
--- response_headers
cas: 1
exists: 1
data: hello
--- response_body
ok
--- no_error_log
[error]
[crit]
[emerg]
[alert]
stub
stub
stub



=== TEST 3: proxy_wasm key/value shm - set empty value vs delete value
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- shm_kv: kv1 1m
--- config
    location /t {
        # set kv1/test=
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data \
                              key=kv1/test \
                              cas=0 \
                              value=';
        # get gv1/test (exists but empty)
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data \
                              key=kv1/test';
        # delete kv1/test
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data \
                              key=kv1/test \
                              cas=1';
        # get kv1/test
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data \
                              key=kv1/test \
                              header_cas=cas-2 \
                              header_data=data-2 \
                              header_exists=exists-2';
        echo ok;
    }
--- response_headers
cas: 1
exists: 1
data:
cas-2: 0
exists-2: 0
data-2:
--- response_body
ok
--- no_error_log
[error]
[crit]
[emerg]
[alert]



=== TEST 4: proxy_wasm key/value shm - set empty value vs delete value (eviction=lru)
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- shm_kv: kv1 1m eviction=lru
--- config
    location /t {
        # set kv1/test=
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data \
                              key=kv1/test \
                              cas=0 \
                              value=';
        # get gv1/test (exists but empty)
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data \
                              key=kv1/test';
        # delete kv1/test
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data \
                              key=kv1/test \
                              cas=1';
        # get kv1/test
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data \
                              key=kv1/test \
                              header_cas=cas-2 \
                              header_data=data-2 \
                              header_exists=exists-2';
        echo ok;
    }
--- response_headers
cas: 1
exists: 1
data:
cas-2: 0
exists-2: 0
data-2:
--- response_body
ok
--- no_error_log
[error]
[crit]
[emerg]
[alert]
stub
stub
stub



=== TEST 5: proxy_wasm key/value shm - attempt to delete missing value is a no-op
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- shm_kv: kv1 1m eviction=lru
--- config
    location /t {
        # set kv1/test=
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data \
                              key=kv1/test \
                              cas=0';
        # get kv1/test
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data \
                              key=kv1/test \
                              header_cas=cas \
                              header_data=data \
                              header_exists=exists';
        echo ok;
    }
--- response_headers
cas: 0
exists: 0
data:
--- response_body
ok
--- no_error_log
[error]
[crit]
[emerg]
[alert]



=== TEST 6: proxy_wasm key/value shm - set with cas
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- shm_kv: kv1 1m
--- config
    location /t {
        # set kv1/test=hello
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data \
                              key=kv1/test \
                              value=hello \
                              header_ok=set-ok-1';
        # get kv1/test (1)
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data \
                              key=kv1/test \
                              header_cas=cas-1 \
                              header_data=data-1';
        # set kv1/test=world
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data \
                              key=kv1/test \
                              value=world \
                              header_ok=set-ok-2';
        # get kv1/test
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data \
                              key=kv1/test \
                              header_cas=cas-2 \
                              header_data=data-2';
        # cas kv1/test=world
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data \
                              key=kv1/test \
                              value=world \
                              cas=1 \
                              header_ok=set-ok-3';
        # get kv1/test
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data \
                              key=kv1/test \
                              header_cas=cas-3 \
                              header_data=data-3';
        echo ok;
    }
--- response_headers
set-ok-1: 1
cas-1: 1
data-1: hello
set-ok-2: 0
cas-2: 1
data-2: hello
set-ok-3: 1
cas-3: 2
data-3: world
--- response_body
ok
--- no_error_log
[error]



=== TEST 7: proxy_wasm key/value shm - set reusing node storage
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- shm_kv: kv1 1m
--- config
    location /t {
        # set kv1/test=hello
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data \
                              key=kv1/test \
                              value=hello \
                              header_ok=set-ok-1';
        # get kv1/test (1)
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data \
                              key=kv1/test \
                              header_cas=cas-1 \
                              header_data=data-1';
        # set long value
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data \
                              key=kv1/test \
                              value=worldworld \
                              cas=1 \
                              header_ok=set-ok-2';
        # get kv1/test
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data \
                              key=kv1/test \
                              header_cas=cas-2 \
                              header_data=data-2';
        # set shorter value, same key
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data \
                              key=kv1/test \
                              value=world \
                              cas=2 \
                              header_ok=set-ok-3';
        # get kv1/test
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data \
                              key=kv1/test \
                              header_cas=cas-3 \
                              header_data=data-3';
        echo ok;
    }
--- response_headers
set-ok-1: 1
cas-1: 1
data-1: hello
set-ok-2: 1
cas-2: 2
data-2: worldworld
set-ok-3: 1
cas-3: 3
data-3: world
--- response_body
ok
--- no_error_log
[error]



=== TEST 8: proxy_wasm key/value shm - set in non-existing namespace fallbacks to global namespace
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- shm_kv: * 64k
--- config
    location /t {
        # set kv2/test=hello
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data \
                              key=kv2/test \
                              value=hello';
        # get kv2/test
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data \
                              key=kv2/test';
        echo ok;
    }
--- response_headers
set-ok: 1
cas: 1
exists: 1
data: hello
--- response_body
ok
--- no_error_log
[error]
[crit]
[emerg]
[alert]
stub
stub
