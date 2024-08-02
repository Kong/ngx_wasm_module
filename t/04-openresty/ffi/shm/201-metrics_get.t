# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasmX;
use t::TestWasmX::Lua;

skip_no_openresty();

plan_tests(6);
run_tests();

__DATA__

=== TEST 1: shm_metrics - get, by name
--- valgrind
--- main_config
    wasm {}
--- config
    location /t {
        access_by_lua_block {
            local pretty = require "pl.pretty"
            local shm = require "resty.wasmx.shm"

            local metrics = {
                c1 = shm.metrics:define("c1", shm.metrics.COUNTER),
                g1 = shm.metrics:define("g1", shm.metrics.GAUGE),
                h1 = shm.metrics:define("h1", shm.metrics.HISTOGRAM),
            }
            local values = {}

            shm.metrics:increment(metrics.c1)
            shm.metrics:record(metrics.g1, 10)
            shm.metrics:record(metrics.h1, 100)

            for name, _ in pairs(metrics) do
                values[name] = shm.metrics:get_by_name(name)
            end

            ngx.say("c1: " .. pretty.write(values.c1, ""))
            ngx.say("g1: " .. pretty.write(values.g1, ""))
            ngx.say("h1: " .. pretty.write(values.h1, ""))
        }
    }
--- response_body
c1: {type="counter",value=1}
g1: {type="gauge",value=10}
h1: {type="histogram",value={[4294967295]=0,[128]=1}}
--- no_error_log
[error]
[crit]
[emerg]
[alert]



=== TEST 2: shm_metrics - get, by name (prefix = false)
--- valgrind
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'on_configure=define_metrics \
                              on=request_headers \
                              n_increments=13 \
                              test=/t/metrics/increment_counters \
                              metrics=c1';

        proxy_wasm hostcalls 'on_configure=define_metrics \
                              on=request_headers \
                              test=/t/metrics/toggle_gauges \
                              metrics=g1';

        proxy_wasm hostcalls 'on_configure=define_metrics \
                              on=request_headers \
                              test=/t/metrics/record_histograms \
                              metrics=h1 \
                              value=10';

        proxy_wasm hostcalls 'on_configure=define_metrics \
                              on=request_headers \
                              test=/t/metrics/record_histograms \
                              metrics=h1 \
                              value=100';

        access_by_lua_block {
            local pretty = require "pl.pretty"
            local shm = require "resty.wasmx.shm"

            shm.metrics:lock()

            for name in shm.metrics:iterate_keys() do
                local m, err = shm.metrics:get_by_name(name, { prefix = false })

                assert(err == nil)
                ngx.say(name .. ": " .. pretty.write(m, ""))
            end

            shm.metrics:unlock()
        }
    }
--- response_body
pw.hostcalls.c1: {type="counter",value=13}
pw.hostcalls.g1: {type="gauge",value=1}
pw.hostcalls.h1: {type="histogram",value={[4294967295]=0,[128]=1,[16]=1}}
--- no_error_log
[error]
[crit]
[emerg]
[alert]



=== TEST 3: shm_metrics - get, by name (invalid name)
--- valgrind
--- main_config
    wasm {}
--- config
    location /t {
        access_by_lua_block {
            local pretty = require "pl.pretty"
            local shm = require "resty.wasmx.shm"

            local m, err = shm.metrics:get_by_name("invalid_name")

            assert(m == nil)
            assert(err == "metric not found")

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
