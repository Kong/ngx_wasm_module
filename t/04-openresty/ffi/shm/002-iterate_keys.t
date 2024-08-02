# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasmX;
use t::TestWasmX::Lua;

skip_no_openresty();

plan_tests(6);
run_tests();

__DATA__

=== TEST 1: shm - iterate_keys()
--- valgrind
--- main_config
    wasm {
        shm_kv kv1 16k;
    }
--- config
    location /t {
        access_by_lua_block {
            local shm = require "resty.wasmx.shm"

            local data = { k1 = "v1", k2 = "v2" }
            local retrieved_keys = {}

            for k, v in pairs(data) do
                shm.kv1:set(k, v, 0)
            end

            shm.kv1:lock()

            for k in shm.kv1:iterate_keys() do
                retrieved_keys[k] = true
            end

            shm.kv1:unlock()

            assert(retrieved_keys.k1)
            assert(retrieved_keys.k2)
            assert(retrieved_keys.inexistent_key == nil)

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



=== TEST 2: shm - iterate_keys(), with batch size
--- valgrind
--- main_config
    wasm {
        shm_kv kv1 16k;
    }
--- config
    location /t {
        access_by_lua_block {
            local shm = require "resty.wasmx.shm"

            local data = { k1 = "v1", k2 = "v2" }
            local retrieved_keys = {}
            local batch_size = 1

            for k, v in pairs(data) do
                shm.kv1:set(k, v, 0)
            end

            shm.kv1:lock()

            for k in shm.kv1:iterate_keys(batch_size) do
                retrieved_keys[k] = true
            end

            shm.kv1:unlock()

            assert(retrieved_keys.k1)
            assert(retrieved_keys.k2)
            assert(retrieved_keys.inexistent_key == nil)

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



=== TEST 3: shm - iterate_keys(), no keys
--- valgrind
--- main_config
    wasm {
        shm_kv kv1 16k;
    }
--- config
    location /t {
        access_by_lua_block {
            local shm = require "resty.wasmx.shm"

            shm.kv1:lock()

            for k in shm.kv1:iterate_keys() do
                ngx.say("fail")
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



=== TEST 4: shm - iterate_keys(), unlocked
--- valgrind
--- main_config
    wasm {
        shm_kv kv1 16k;
    }
--- config
    location /t {
        access_by_lua_block {
            local shm = require "resty.wasmx.shm"

            for k in shm.kv1:iterate_keys() do
            end
        }
    }
--- error_code: 500
--- error_log eval
qr/\[error\] .*? attempt to iterate over the keys of an unlocked shm zone. please call resty.wasmx.shm.kv1:lock\(\) before calling iterate_keys\(\) and resty.wasmx.shm.kv1:unlock\(\) after/
--- no_error_log
[crit]
[emerg]
[alert]
[stub]



=== TEST 5: shm - iterate_keys(), queue
--- valgrind
--- main_config
    wasm {
        shm_queue q1 16k;
    }
--- config
    location /t {
        access_by_lua_block {
            local shm = require "resty.wasmx.shm"

            for k in shm.q1:iterate_keys() do
            end
        }
    }
--- error_code: 500
--- error_log eval
qr/\[error\] .*? attempt to call method 'iterate_keys' \(a nil value\)/
--- no_error_log
[crit]
[emerg]
[alert]
[stub]
