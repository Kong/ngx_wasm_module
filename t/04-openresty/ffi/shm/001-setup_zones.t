# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasmX;
use t::TestWasmX::Lua;

skip_no_openresty();

plan_tests(7);
run_tests();

__DATA__

=== TEST 1: shm - setup_zones()
setup_zones() is silently called when resty.wasmx.shm module is loaded.

--- valgrind
--- main_config
    wasm {
        shm_kv kv1 16k;
        shm_queue q1 16k;
    }

--- config
    location /t {
        access_by_lua_block {
            local shm = require "resty.wasmx.shm"

            assert(shm.kv1)
            assert(shm.q1)
            assert(shm.metrics)

            assert(shm.inexistent_zone == nil)

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
[stub]



=== TEST 2: shm - setup_zones(), no zones
--- valgrind
--- config
    location /t {
        access_by_lua_block {
            local shm = require "resty.wasmx.shm"

            assert(shm.metrics == nil)

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
[stub]
