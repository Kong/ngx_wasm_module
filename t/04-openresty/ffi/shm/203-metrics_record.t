# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasmX;
use t::TestWasmX::Lua;

skip_no_openresty();

plan_tests(6);
run_tests();

__DATA__

=== TEST 1: shm_metrics - record()
--- valgrind
--- main_config
    wasm {}
--- config
    location /t {
        access_by_lua_block {
            local pretty = require "pl.pretty"
            local shm = require "resty.wasmx.shm"

            local g1 = shm.metrics:define("g1", shm.metrics.GAUGE)
            local h1 = shm.metrics:define("h1", shm.metrics.HISTOGRAM)

            shm.metrics:record(g1, 10)
            shm.metrics:record(h1, 100)

            ngx.say("g1: " .. pretty.write(shm.metrics:get(g1), ""))
            ngx.say("h1: " .. pretty.write(shm.metrics:get(h1), ""))
        }
    }
--- response_body
g1: {type="gauge",value=10}
h1: {type="histogram",value={[4294967295]=0,[128]=1}}
--- no_error_log
[error]
[crit]
[emerg]
[alert]



=== TEST 2: shm_metrics - record(), inexistent metric
--- valgrind
--- main_config
    wasm {}
--- config
    location /t {
        access_by_lua_block {
            local shm = require "resty.wasmx.shm"

            local ok, err = shm.metrics:record(1, 10)

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
