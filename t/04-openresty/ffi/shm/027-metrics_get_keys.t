# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasmX::Lua;

skip_no_openresty();

plan_tests(4);
run_tests();

__DATA__

=== TEST 1: shm_metrics - get_keys() sanity
--- metrics: 16k
--- config
    location /t {
        access_by_lua_block {
            local shm = require "resty.wasmx.shm"

            local c1 = shm.metrics:define("c1", shm.metrics.COUNTER)
            local g1 = shm.metrics:define("g1", shm.metrics.GAUGE)
            local h1 = shm.metrics:define("h1", shm.metrics.HISTOGRAM)

            local keys = assert(shm.metrics:get_keys())

            for _, key in ipairs(keys) do
                ngx.say(key)
            end
        }
    }
--- response_body
lua:c1
lua:g1
lua:h1
--- no_error_log
[error]
[crit]