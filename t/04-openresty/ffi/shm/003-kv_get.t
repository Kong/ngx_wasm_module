# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasmX;
use t::TestWasmX::Lua;

skip_no_openresty();

plan_tests(8);
run_tests();

__DATA__

=== TEST 1: shm_kv - get()
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
            local keys = kv1:get_keys()

            for i, k in ipairs(keys) do
                local v = kv1:get(k)
                ngx.log(ngx.INFO,"kv1 " .. k .. ": ", v)
            end
        }
        echo ok;
    }
--- response_body
ok
--- error_log eval
[
    qr/\[info\] .*? kv1 kv1\/k1: hello1/,
    qr/\[info\] .*? kv1 kv1\/k2: hello2/,
]
--- no_error_log
[error]
[crit]
[emerg]
[alert]



=== TEST 2: shm_kv - get(), bad pointer
A call with a bad pointer is simply ignored.

--- valgrind
--- load_nginx_modules: ngx_http_echo_module
--- config
    location /t {
        access_by_lua_block {
            local ffi = require "ffi"
            local C = ffi.C

            ffi.cdef [[
                typedef struct ngx_wa_shm_t  ngx_wa_shm_t;

                int ngx_wa_ffi_shm_get_kv_value(ngx_wa_shm_t *shm,
                                                ngx_str_t *k,
                                                ngx_str_t **v,
                                                uint32_t *cas);
            ]]

            C.ngx_wa_ffi_shm_get_kv_value(nil, nil, nil, nil)
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
