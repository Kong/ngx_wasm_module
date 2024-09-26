# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasmX::Lua;

skip_no_openresty();

plan_tests(4);
run_tests();

__DATA__

=== TEST 1: shm_metrics - record() sanity
--- valgrind
--- metrics: 16k
--- config
    location /t {
        access_by_lua_block {
            local shm = require "resty.wasmx.shm"
            local pretty = require "pl.pretty"

            local g1 = shm.metrics:define("g1", shm.metrics.GAUGE)
            local h1 = shm.metrics:define("h1", shm.metrics.HISTOGRAM)

            assert(shm.metrics:record(g1, 10))
            assert(shm.metrics:record(h1, 100))

            ngx.say("g1: ", pretty.write(shm.metrics:get(g1), ""))
            ngx.say("h1: ", pretty.write(shm.metrics:get(h1), ""))
        }
    }
--- response_body
g1: {type="gauge",value=10}
h1: {sum=100,type="histogram",value={{count=1,ub=128},{count=0,ub=4294967295}}}
--- no_error_log
[error]
[crit]



=== TEST 2: shm_metrics - record() metric not found
--- metrics: 16k
--- config
    location /t {
        access_by_lua_block {
            local shm = require "resty.wasmx.shm"

            local ok, err = shm.metrics:record(1, 10)
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



=== TEST 3: shm_metrics - record() bad args
--- metrics: 16k
--- config
    location /t {
        access_by_lua_block {
            local shm = require "resty.wasmx.shm"

            local _, perr = pcall(shm.metrics.record, {}, false)
            ngx.say(perr)

            _, perr = pcall(shm.metrics.record, {}, 1, false)
            ngx.say(perr)
        }
    }
--- response_body
metric_id must be a number
value must be a number
--- no_error_log
[error]
[crit]
