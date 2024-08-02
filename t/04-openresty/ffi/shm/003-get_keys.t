# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasmX;
use t::TestWasmX::Lua;

skip_no_openresty();

plan_tests(6);
run_tests();

__DATA__

=== TEST 1: shm - get_keys()
--- valgrind
--- wasm_modules: hostcalls
--- shm_kv: kv1 16k
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

            local retrieved_keys = {}

            for _, k in ipairs(shm.kv1:get_keys()) do
                retrieved_keys[k] = true
            end

            assert(retrieved_keys["kv1/k1"])
            assert(retrieved_keys["kv1/k2"])

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



=== TEST 2: shm - get_keys(), with limit
--- valgrind
--- wasm_modules: hostcalls
--- shm_kv: kv1 16k
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

            local retrieved_keys = {}
            local limit = 1

            for _, k in ipairs(shm.kv1:get_keys(limit)) do
                retrieved_keys[k] = true
            end

            assert(retrieved_keys["kv1/k1"])
            assert(retrieved_keys["kv1/k2"] == nil)

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



=== TEST 3: shm - get_keys(), no keys
--- valgrind
--- main_config
    wasm {
        shm_kv kv1 16k;
    }
--- config
    location /t {
        access_by_lua_block {
            local shm = require "resty.wasmx.shm"

            assert(#shm.kv1:get_keys() == 0)
            assert(#shm.kv1:get_keys(0) == 0)
            assert(#shm.kv1:get_keys(1) == 0)

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



=== TEST 4: shm - get_keys(), queue
--- valgrind
--- main_config
    wasm {
        shm_queue q1 16k;
    }
--- config
    location /t {
        access_by_lua_block {
            local shm = require "resty.wasmx.shm"

            shm.q1:get_keys()
        }
    }
--- error_code: 500
--- error_log eval
qr/\[error\] .*? attempt to call method 'get_keys' \(a nil value\)/
--- no_error_log
[crit]
[emerg]
[alert]
[stub]
