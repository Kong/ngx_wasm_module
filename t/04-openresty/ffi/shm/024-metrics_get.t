# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasmX::Lua;

skip_no_openresty();

plan_tests(4);
run_tests();

__DATA__

=== TEST 1: shm_metrics - get() sanity
--- valgrind
--- metrics: 16k
--- config
    location /t {
        access_by_lua_block {
            local shm = require "resty.wasmx.shm"
            local pretty = require "pl.pretty"

            local bins = { 1, 3, 5 }

            local c1 = shm.metrics:define("c1", shm.metrics.COUNTER)
            local g1 = shm.metrics:define("g1", shm.metrics.GAUGE)
            local h1 = shm.metrics:define("h1", shm.metrics.HISTOGRAM)
            local ch1 = shm.metrics:define("ch1", shm.metrics.HISTOGRAM, { bins = bins })

            shm.metrics:increment(c1)
            shm.metrics:record(g1, 10)
            shm.metrics:record(h1, 100)
            shm.metrics:record(ch1, 2)

            ngx.say("c1: ", pretty.write(shm.metrics:get(c1), ""))
            ngx.say("g1: ", pretty.write(shm.metrics:get(g1), ""))
            ngx.say("h1: ", pretty.write(shm.metrics:get(h1), ""))
            ngx.say("ch1: ", pretty.write(shm.metrics:get(ch1), ""))
        }
    }
--- response_body
c1: {type="counter",value=1}
g1: {type="gauge",value=10}
h1: {sum=100,type="histogram",value={{count=1,ub=128},{count=0,ub=4294967295}}}
ch1: {sum=2,type="histogram",value={{count=0,ub=1},{count=1,ub=3},{count=0,ub=5},{count=0,ub=4294967295}}}
--- no_error_log
[error]
[crit]



=== TEST 2: shm_metrics - get() metric not found
--- metrics: 16k
--- config
    location /t {
        access_by_lua_block {
            local shm = require "resty.wasmx.shm"

            local m = shm.metrics:get(99)
            assert(m == nil)

            ngx.say("ok")
        }
    }
--- response_body
ok
--- no_error_log
[error]
[crit]



=== TEST 3: shm_metrics - get() bad args
--- metrics: 16k
--- config
    location /t {
        access_by_lua_block {
            local shm = require "resty.wasmx.shm"

            local _, perr = pcall(shm.metrics.get, {}, false)
            ngx.say(perr)
        }
    }
--- response_body
metric_id must be a number
--- no_error_log
[error]
[crit]
