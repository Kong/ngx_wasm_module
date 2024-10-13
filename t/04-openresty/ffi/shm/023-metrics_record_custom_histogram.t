# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasmX::Lua;

skip_no_openresty();

workers(2);
master_on();

plan_tests(4);
run_tests();

__DATA__

=== TEST 1: shm_metrics - record() custom histogram, multiple workers
Metric creation and update by multiple workers is covered in Proxy-Wasm tests;
custom histogram creation and update by multiple workers is covered here because
custom histograms can't be created from Proxy-Wasm filters yet.

--- valgrind
--- metrics: 16k
--- http_config
    init_worker_by_lua_block {
        local shm = require "resty.wasmx.shm"

        local bins = { 1, 3, 5 }
        ch1 = shm.metrics:define("ch1", shm.metrics.HISTOGRAM, { bins = bins })

        for i in ipairs({ 1, 2, 3, 4, 5, 6 }) do
            assert(shm.metrics:record(ch1, i))
        end
    }
--- config
    location /t {
        access_by_lua_block {
            local shm = require "resty.wasmx.shm"
            local pretty = require "pl.pretty"

            ngx.say("ch1: ", pretty.write(shm.metrics:get(ch1), ""))
        }
    }
--- response_body
ch1: {sum=42,type="histogram",value={{count=2,ub=1},{count=4,ub=3},{count=4,ub=5},{count=2,ub=4294967295}}}
--- no_error_log
[error]
[crit]
