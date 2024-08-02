# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasmX;
use t::TestWasmX::Lua;

skip_no_openresty();

plan_tests(6);
run_tests();

__DATA__

=== TEST 1: shm_kv - get()
--- valgrind
--- wasm_modules: hostcalls
--- shm_kv: kv1 16k
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data \
                              key=kv1/k1 \
                              value=v1';

        proxy_wasm hostcalls 'test=/t/shm/set_shared_data \
                              key=kv1/k2 \
                              value=v2';

        access_by_lua_block {
            local shm = require "resty.wasmx.shm"

            local expected = { ["kv1/k1"] = "v1", ["kv1/k2"] = "v2" }

            shm.kv1:lock()

            for k in shm.kv1:iterate_keys() do
                local v, cas, err = shm.kv1:get(k)
                assert(v == expected[k])
                assert(cas == 1)
            end

            shm.kv1:unlock()

            ngx.say("ok")
        }
    }
--- response_body
ok
--- no_error_log
[error]
[crit]
[emerg]
[alert]



=== TEST 2: shm_kv - get(), unlocked
--- valgrind
--- main_config
    wasm {
        shm_kv kv1 16k;
    }
--- config
    location /t {
        access_by_lua_block {
            local shm = require "resty.wasmx.shm"

            local initial_cas = 0
            local k = "k1"
            local expected_v = "v1"

            local written, err = shm.kv1:set(k, expected_v, initial_cas)
            assert(written == 1)
            assert(err == nil)

            local v, cas, err = shm.kv1:get(k)
            assert(v == expected_v)
            assert(cas == initial_cas + 1)
            assert(err == nil)

            ngx.say("ok")
        }
    }
--- response_body
ok
--- no_error_log
[error]
[crit]
[emerg]
[alert]
