# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasmX::Lua;

skip_no_openresty();

plan_tests(4);
run_tests();

__DATA__

=== TEST 1: shm_metrics - increment() sanity
--- valgrind
--- metrics: 16k
--- config
    location /t {
        access_by_lua_block {
            local shm = require "resty.wasmx.shm"
            local pretty = require "pl.pretty"

            local c1 = shm.metrics:define("c1", shm.metrics.COUNTER)

            shm.metrics:increment(c1) -- value defaults to 1
            shm.metrics:increment(c1, 10)

            ngx.say("c1: ", pretty.write(shm.metrics:get(c1), ""))
        }
    }
--- response_body
c1: {type="counter",value=11}
--- no_error_log
[error]
[crit]



=== TEST 2: shm_metrics - increment() metric not found
--- metrics: 16k
--- config
    location /t {
        access_by_lua_block {
            local shm = require "resty.wasmx.shm"

            local ok, err = shm.metrics:increment(1, 10)
            ngx.say("ok: ", ok)
            ngx.say("err: ", err)
        }
    }
--- response_body
ok: nil
err: metric not found
--- no_error_log
[error]
[crit]



=== TEST 3: shm_metrics - increment() bad args
--- metrics: 16k
--- config
    location /t {
        access_by_lua_block {
            local shm = require "resty.wasmx.shm"

            local _, perr = pcall(shm.metrics.increment, {}, false)
            ngx.say(perr)

            _, perr = pcall(shm.metrics.increment, {}, 1, false)
            ngx.say(perr)

            _, perr = pcall(shm.metrics.increment, {}, 1, 0)
            ngx.say(perr)
        }
    }
--- response_body
metric_id must be a number
value must be > 0
value must be > 0
--- no_error_log
[error]
[crit]
