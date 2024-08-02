# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasmX;
use t::TestWasmX::Lua;

skip_no_openresty();

plan_tests(9);
run_tests();

__DATA__

=== TEST 1: shm - get_keys()
--- valgrind
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- shm_kv: kv1 1m
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data \
                              key=kv1/k1 \
                              value=hello1';

        proxy_wasm hostcalls 'test=/t/shm/set_shared_data \
                              key=kv1/k2 \
                              value=hello2';

        access_by_lua_block {
            local shm = require "resty.wasmx.shm"
            local keys = shm.kv1:get_keys()

            ngx.log(ngx.INFO, #keys, " key(s) retrieved")

            for i, k in ipairs(keys) do
                ngx.log(ngx.INFO,"kv1 key: ", k)
            end
        }

        echo ok;
    }
--- response_body
ok
--- error_log eval
[
    qr/\[info\] .*? 2 key\(s\) retrieved/,
    qr/\[info\] .*? kv1 key: kv1\/k1/,
    qr/\[info\] .*? kv1 key: kv1\/k2/,
]
--- no_error_log
[error]
[crit]
[emerg]
[alert]



=== TEST 2: shm - get_keys(), with limit
--- valgrind
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- shm_kv: kv1 1m
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data \
                              key=kv1/k1 \
                              value=hello1';

        proxy_wasm hostcalls 'test=/t/shm/set_shared_data \
                              key=kv1/k2 \
                              value=hello2';

        access_by_lua_block {
            local shm = require "resty.wasmx.shm"

            local kv1 = shm.kv1
            local keys = kv1:get_keys(1)

            ngx.log(ngx.INFO, #keys, " key(s) retrieved")

            for i, k in ipairs(keys) do
                ngx.log(ngx.INFO,"kv1 key: ", k)
            end
        }

        echo ok;
    }
--- response_body
ok
--- error_log eval
[
    qr/\[info\] .*? 1 key\(s\) retrieved/,
    qr/\[info\] .*? kv1 key: kv1\/k1/,
]
--- no_error_log
[error]
[crit]
[emerg]
[alert]
qr/\[info\] .*? kv1 key: kv1\/t2/,



=== TEST 3: shm - get_keys(), queue
--- valgrind
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- shm_queue: q1 1m
--- config
    location /t {
        access_by_lua_block {
            local shm = require "resty.wasmx.shm"

            shm.q1:get_keys()
        }

        echo fail;
    }
--- error_code: 500
--- error_log eval
qr/\[error\] .*? attempt to call method 'get_keys' \(a nil value\)/
--- no_error_log
[crit]
[emerg]
[alert]
[stub]
[stub]
[stub]
[stub]



=== TEST 4: shm - get_keys(), bad pointer
A call with a bad pointer is simply ignored.

--- valgrind
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- shm_kv: kv1 1m
--- config
    location /t {
        access_by_lua_block {
            local ffi = require "ffi"
            local C = ffi.C

            ffi.cdef [[
                typedef uintptr_t              ngx_uint_t;
                typedef struct ngx_wasm_shm_t  ngx_wasm_shm_t;

                int ngx_wa_ffi_shm_get_keys(ngx_wasm_shm_t *shm,
                                            ngx_uint_t n,
                                            ngx_str_t **keys);
            ]]

            C.ngx_wa_ffi_shm_get_keys(nil, 0, nil)
        }

        echo ok;
    }
--- response_body
ok
--- no_error_log
[error]
[crit]
[emerg]
[alert]
[stub]
[stub]
[stub]
