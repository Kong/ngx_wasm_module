# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasmX;
use t::TestWasmX::Lua;

skip_no_openresty();

plan_tests(6);
run_tests();

__DATA__

=== TEST 1: shm_metrics - increment()
--- valgrind
--- main_config
    wasm {}
--- config
    location /t {
        access_by_lua_block {
            local pretty = require "pl.pretty"
            local shm = require "resty.wasmx.shm"

            local c1 = shm.metrics:define("c1", shm.metrics.COUNTER)

            shm.metrics:increment(c1)
            shm.metrics:increment(c1, 10)

            ngx.say("c1: " .. pretty.write(shm.metrics:get(c1), ""))
        }
    }
--- response_body
c1: {type="counter",value=11}
--- no_error_log
[error]
[crit]
[emerg]
[alert]



=== TEST 2: shm_metrics - increment(), inexistent metric
--- valgrind
--- main_config
    wasm {}
--- config
    location /t {
        access_by_lua_block {
            local shm = require "resty.wasmx.shm"

            local ok, err = shm.metrics:increment(1, 10)

            if not ok then
                ngx.say(err)
                return
            end

            ngx.say("fail")
        }
    }
--- response_body
metric not found
--- no_error_log
[error]
[crit]
[emerg]
[alert]
