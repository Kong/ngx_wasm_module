# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasmX::Lua;

skip_no_openresty();

plan_tests(7);
run_tests();

__DATA__

=== TEST 1: shm_metrics - define() sanity
--- skip_no_debug
--- valgrind
--- metrics: 16k
--- config
    location /t {
        access_by_lua_block {
            local shm = require "resty.wasmx.shm"

            local bins = { 1, 3, 5 }

            local metrics = {
                c1 = shm.metrics:define("c1", shm.metrics.COUNTER),
                g1 = shm.metrics:define("g1", shm.metrics.GAUGE),
                h1 = shm.metrics:define("h1", shm.metrics.HISTOGRAM),
                ch1 = shm.metrics:define("ch1", shm.metrics.HISTOGRAM, { bins = bins }),
            }

            for _, id in pairs(metrics) do
                assert(id > 0)
            end

            ngx.say("ok")
        }
    }
--- response_body
ok
--- error_log eval
[
    qr/.*? \[debug\] .*? defined counter "lua.c1" with id \d+/,
    qr/.*? \[debug\] .*? defined gauge "lua.g1" with id \d+/,
    qr/.*? \[debug\] .*? defined histogram "lua.h1" with id \d+/,
    qr/.*? \[debug\] .*? defined histogram "lua.ch1" with id \d+/,
]
--- no_error_log
[error]



=== TEST 2: shm_metrics - define() name too long
--- metrics: 16k
--- config
    location /t {
        access_by_lua_block {
            local shm = require "resty.wasmx.shm"

            local mid, err = shm.metrics:define(string.rep(".", 500), shm.metrics.HISTOGRAM)
            ngx.say("mid: ", mid)
            ngx.say("err: ", err)
        }
    }
--- response_body
mid: nil
err: name too long
--- no_error_log
[error]
[crit]
[emerg]
[alert]
[stub]



=== TEST 3: shm_metrics - define() no memory
--- metrics: 16k
--- config
    location /t {
        access_by_lua_block {
            local shm = require "resty.wasmx.shm"

            for i = 1, 100 do
                local _, err = shm.metrics:define(string.rep(i, 100), shm.metrics.HISTOGRAM)
                if err then
                    ngx.log(ngx.ERR, err)
                    break
                end
            end

            ngx.say("ok")
        }
    }
--- response_body
ok
--- error_log eval
[
    qr/\[crit\] .*? "metrics" shm store: no memory; cannot allocate pair with key size 204 and value size 24/,
    qr/\[error\] .*? access_by_lua\(.*?\):\d+: no memory/,
]
--- no_error_log
[emerg]
[alert]
[stub]



=== TEST 4: shm_metrics - define() bad args
--- metrics: 16k
--- config
    location /t {
        access_by_lua_block {
            local shm = require "resty.wasmx.shm"

            local _, perr = pcall(shm.metrics.define, {}, false, 10)
            ngx.say(perr)

            _, perr = pcall(shm.metrics.define, {}, "", 10)
            ngx.say(perr)

            _, perr = pcall(shm.metrics.define, {}, "c1", 10)
            ngx.say(perr)

            _, perr = pcall(shm.metrics.define, {}, "ch1", shm.metrics.HISTOGRAM, { bins = 10 })
            ngx.say(perr)

            local bins = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18 }
            _, perr = pcall(shm.metrics.define, {}, "ch1", shm.metrics.HISTOGRAM, { bins = bins })
            ngx.say(perr)

            bins = { 1, 3, -5 }
            _, perr = pcall(shm.metrics.define, {}, "ch1", shm.metrics.HISTOGRAM, { bins = bins })
            ngx.say(perr)

            bins = { 1, 3, 5.5 }
            _, perr = pcall(shm.metrics.define, {}, "ch1", shm.metrics.HISTOGRAM, { bins = bins })
            ngx.say(perr)

            bins = { 1, 5, 3 }
            _, perr = pcall(shm.metrics.define, {}, "ch1", shm.metrics.HISTOGRAM, { bins = bins })
            ngx.say(perr)

            -- the optional 'bins' arg is ignored if not defining a histogram
            shm.metrics:define("c1", shm.metrics.COUNTER, { bins = { 3, -1, 1.5 } })
        }
    }
--- response_body
name must be a non-empty string
name must be a non-empty string
metric_type must be one of resty.wasmx.shm.metrics.COUNTER, resty.wasmx.shm.metrics.GAUGE, or resty.wasmx.shm.metrics.HISTOGRAM
opts.bins must be a table
opts.bins cannot have more than 17 numbers
opts.bins must be an ascending list of positive integers
opts.bins must be an ascending list of positive integers
opts.bins must be an ascending list of positive integers
--- no_error_log
[error]
[crit]
[emerg]
[alert]
[stub]
