# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasmX::Lua;

skip_no_openresty();

plan_tests(4);
run_tests();

__DATA__

=== TEST 1: shm_kv - get() sanity
--- valgrind
--- wasm_modules: hostcalls
--- shm_kv: kv 16k
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data \
                              key=kv/k1 \
                              value=v1';

        proxy_wasm hostcalls 'test=/t/shm/set_shared_data \
                              key=kv/k2 \
                              value=v2';

        access_by_lua_block {
            local shm = require "resty.wasmx.shm"

            local v, cas = shm.kv:get("kv/k1")
            assert(cas == 1)
            ngx.say(v)

            v, cas = shm.kv:get("kv/k2")
            assert(cas == 1)
            ngx.say(v)
        }
    }
--- response_body
v1
v2
--- no_error_log
[error]
[crit]



=== TEST 2: shm_kv - get() metric not found
--- valgrind
--- shm_kv: kv 16k
--- config
    location /t {
        access_by_lua_block {
            local shm = require "resty.wasmx.shm"

            local v, cas = shm.kv:get("none")
            assert(cas == nil)
            ngx.say(v)
        }
    }
--- response_body
nil
--- no_error_log
[error]
[crit]



=== TEST 3: shm_kv - get() bad args
--- shm_kv: kv 16k
--- config
    location /t {
        access_by_lua_block {
            local shm = require "resty.wasmx.shm"

            local _, perr = pcall(shm.kv.get, {}, false)
            ngx.say(perr)
        }
    }
--- response_body
key must be a string
--- no_error_log
[crit]
[emerg]



=== TEST 4: shm_kv - get() on locked shm
Do not wait on lock if shm is already locked.
--- wasm_modules: hostcalls
--- shm_kv: kv 16k
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data \
                              key=kv/k1 \
                              value=v1';

        access_by_lua_block {
            local shm = require "resty.wasmx.shm"

            shm.kv:lock()

            local v = shm.kv:get("kv/k1")
            ngx.say(v)

            shm.kv:unlock()
        }
    }
--- response_body
v1
--- no_error_log
[error]
[crit]
