# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasmX::Lua;

skip_no_openresty();

plan_tests(4);
run_tests();

__DATA__

=== TEST 1: shm_kv - set() sanity
--- valgrind
--- wasm_modules: hostcalls
--- shm_kv: kv 16k
--- config
    location /t {
        access_by_lua_block {
            local shm = require "resty.wasmx.shm"

            local written, err = shm.kv:set("kv/k1", "v1")
            assert(err == nil)
            assert(written == 1)

            written, err = shm.kv:set("kv/k2", "v2")
            assert(err == nil)
            assert(written == 1)

            ngx.say("ok")
        }

        proxy_wasm hostcalls 'test=/t/shm/log_shared_data \
                              on=log \
                              key=kv/k1';

        proxy_wasm hostcalls 'test=/t/shm/log_shared_data \
                              on=log \
                              key=kv/k2';
    }
--- ignore_response_body
--- error_log
kv/k1: "v1" 1
kv/k2: "v2" 1
--- no_error_log
[error]



=== TEST 2: shm_kv - set() no memory
--- shm_kv: kv 16k eviction=none
--- config
    location /t {
        access_by_lua_block {
            local shm = require "resty.wasmx.shm"

            for i = 1, 5 do
                local _, err = shm.kv:set(tostring(i), string.rep(".", 2^10))
                if err then
                    ngx.log(ngx.ERR, err)
                    break
                end
            end

            ngx.say("ok")
        }
    }
--- ignore_response_body
--- error_log eval
[
    qr/\[crit\] .*? "kv" shm store: no memory; cannot allocate pair with key size 1 and value size 1024/,
    qr/\[error\] .*? access_by_lua\(.*?\):\d+: no memory/,
]
--- no_error_log
[emerg]



=== TEST 3: shm_kv - set() bad args
--- shm_kv: kv 16k
--- config
    location /t {
        access_by_lua_block {
            local shm = require "resty.wasmx.shm"

            local _, perr = pcall(shm.kv.set, {}, false)
            ngx.say(perr)

            _, perr = pcall(shm.kv.set, {}, "k1", false)
            ngx.say(perr)

            _, perr = pcall(shm.kv.set, {}, "k1", "v1", false)
            ngx.say(perr)
        }
    }
--- response_body
key must be a string
value must be a string
cas must be a number
--- no_error_log
[crit]
[emerg]



=== TEST 4: shm_kv - set() on locked shm
--- shm_kv: kv 16k
--- config
    location /t {
        access_by_lua_block {
            local shm = require "resty.wasmx.shm"

            shm.kv:lock()

            local written, err = shm.kv:set("kv/k1", "v1")

            shm.kv:unlock()

            assert(written == nil)

            ngx.say(err)
        }
    }
--- response_body
locked
--- no_error_log
[error]
[crit]
