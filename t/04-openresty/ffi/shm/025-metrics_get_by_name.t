# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasmX::Lua;

skip_no_openresty();

plan_tests(4);
run_tests();

__DATA__

=== TEST 1: shm_metrics - get_by_name() sanity FFI-defined metrics
colon-separated prefix: "lua:*"
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

            ngx.say("c1: ", pretty.write(shm.metrics:get_by_name("c1"), ""))
            ngx.say("g1: ", pretty.write(shm.metrics:get_by_name("g1"), ""))
            ngx.say("h1: ", pretty.write(shm.metrics:get_by_name("h1"), ""))
            ngx.say("ch1: ", pretty.write(shm.metrics:get_by_name("ch1"), ""))
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



=== TEST 2: shm_metrics - get_by_name() sanity non-FFI-defined metrics
colon-separated prefix: "pw:hostcalls:*"
--- wasm_modules: hostcalls
--- config eval
my $record_histograms;

foreach my $exp (0 .. 17) {
    my $v = 2 ** $exp;
    $record_histograms .= "
        proxy_wasm hostcalls 'on_configure=define_metrics \
                              test=/t/metrics/record_histograms \
                              metrics=h1 \
                              value=$v';";
}

qq{
    location /t {
        proxy_wasm hostcalls 'on_configure=define_metrics \
                              on=request_headers \
                              test=/t/metrics/increment_counters \
                              metrics=c1 \
                              n_increments=13';

        proxy_wasm hostcalls 'on_configure=define_metrics \
                              on=request_headers \
                              test=/t/metrics/toggle_gauges \
                              metrics=g1';

        $record_histograms

        content_by_lua_block {
            local shm = require "resty.wasmx.shm"
            local pretty = require "pl.pretty"

            ngx.say("c1: ", pretty.write(shm.metrics:get_by_name("pw:hostcalls:c1", { prefix = false }), ""))
            ngx.say("g1: ", pretty.write(shm.metrics:get_by_name("pw:hostcalls:g1", { prefix = false }), ""))
            ngx.say("h1: ", pretty.write(shm.metrics:get_by_name("pw:hostcalls:h1", { prefix = false }), ""))
        }
    }
}
--- response_body
c1: {type="counter",value=13}
g1: {type="gauge",value=1}
h1: {sum=262143,type="histogram",value={{count=1,ub=1},{count=1,ub=2},{count=1,ub=4},{count=1,ub=8},{count=1,ub=16},{count=1,ub=32},{count=1,ub=64},{count=1,ub=128},{count=1,ub=256},{count=1,ub=512},{count=1,ub=1024},{count=1,ub=2048},{count=1,ub=4096},{count=1,ub=8192},{count=1,ub=16384},{count=1,ub=32768},{count=1,ub=65536},{count=1,ub=4294967295}}}
--- no_error_log
[error]
[crit]



=== TEST 3: shm_metrics - get_by_name() metric not found
--- metrics: 16k
--- config
    location /t {
        access_by_lua_block {
            local shm = require "resty.wasmx.shm"

            local m = shm.metrics:get_by_name("no_such_metric")
            assert(m == nil)

            ngx.say("ok")
        }
    }
--- response_body
ok
--- no_error_log
[error]
[crit]



=== TEST 4: shm_metrics - get_by_name() bad args
--- metrics: 16k
--- config
    location /t {
        access_by_lua_block {
            local shm = require "resty.wasmx.shm"

            local _, perr = pcall(shm.metrics.get_by_name, {}, false)
            ngx.say(perr)

            _, perr = pcall(shm.metrics.get_by_name, {}, "")
            ngx.say(perr)

            _, perr = pcall(shm.metrics.get_by_name, {}, "m", false)
            ngx.say(perr)

            _, perr = pcall(shm.metrics.get_by_name, {}, "m", { prefix = 1 })
            ngx.say(perr)
        }
    }
--- response_body
name must be a non-empty string
name must be a non-empty string
opts must be a table
opts.prefix must be a boolean
--- no_error_log
[error]
[crit]
