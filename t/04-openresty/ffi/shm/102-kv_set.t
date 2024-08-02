# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasmX;
use t::TestWasmX::Lua;

skip_no_openresty();

plan_tests(6);
run_tests();

__DATA__

=== TEST 1: shm_kv - set()
--- valgrind
--- main_config
    wasm {
        shm_kv kv1 16k;
    }
--- config
    location /t {
        access_by_lua_block {
            local shm = require "resty.wasmx.shm"

            local values = { k1 = "v1", k2 = "v2" }
            local cas = 0

            for k, v in pairs(values) do
                local written, err = shm.kv1:set(k, v, cas)
                assert(written == 1)
                assert(err == nil)
            end

            for k, expected_v in pairs(values) do
                local v, cas, err = shm.kv1:get(k)
                assert(v == expected_v)
                assert(cas == 1)
                assert(err == nil)
            end

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



=== TEST 2: shm_kv - set(), locked
--- valgrind
--- main_config
    wasm {
        shm_kv kv1 16k;
    }
--- config
    location /t {
        access_by_lua_block {
            local shm = require "resty.wasmx.shm"

            local initial_values = { k1 = "v1", k2 = "v2" }
            local expected_values = { k1 = "nv1", k2 = "nv2" }
            local cas = 0

            for k, v in pairs(initial_values) do
                local written, err = shm.kv1:set(k, v, cas)
                assert(written == 1)
                assert(err == nil)
            end

            shm.kv1:lock()

            for k in shm.kv1:iterate_keys() do
                local written, err = shm.kv1:set(k, expected_values[k], cas + 1)
                assert(written == 1)
                assert(err == nil)
            end

            shm.kv1:unlock()

            for k, expected_v in pairs(expected_values) do
                local v, cas, err = shm.kv1:get(k)
                assert(v == expected_v)
                assert(cas == 2)
                assert(err == nil)
            end

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
