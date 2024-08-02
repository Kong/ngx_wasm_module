# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasmX;
use t::TestWasmX::Lua;

skip_no_openresty();

plan_tests(9);
run_tests();

__DATA__

=== TEST 1: shm_metrics - define()
--- valgrind
--- main_config
    wasm {}
--- config
    location /t {
        access_by_lua_block {
            local shm = require "resty.wasmx.shm"

            local metrics = {
                c1 = shm.metrics:define("c1", shm.metrics.COUNTER),
                g1 = shm.metrics:define("g1", shm.metrics.GAUGE),
                h1 = shm.metrics:define("h1", shm.metrics.HISTOGRAM),
            }

            for name, id in pairs(metrics) do
                assert(type(id) == "number" and id > 0)
            end

            ngx.say("ok")
        }
    }
--- response_body
ok
--- error_log eval
[
    qr/.*? \[info\] .*? defined counter "lua.c1" with id \d+/,
    qr/.*? \[info\] .*? defined gauge "lua.g1" with id \d+/,
    qr/.*? \[info\] .*? defined histogram "lua.h1" with id \d+/,
]
--- no_error_log
[error]
[crit]
[emerg]
[alert]



=== TEST 2: shm_metrics - define(), name too long
--- valgrind
--- main_config
    wasm {}
--- config
    location /t {
        access_by_lua_block {
            local shm = require "resty.wasmx.shm"

            local name_too_long = string.rep("x", 500)
            local mid, err = shm.metrics:define(name_too_long, shm.metrics.HISTOGRAM)

            assert(mid == nil)
            assert(err == "failed defining metric, name too long")

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
defined histogram
[stub]
[stub]



=== TEST 3: shm_metrics - define(), invalid metric type
--- valgrind
--- main_config
    wasm {}
--- config
    location /t {
        access_by_lua_block {
            local shm = require "resty.wasmx.shm"

            shm.metrics:define("c1", 10)
        }
    }
--- error_code: 500
--- error_log eval
qr/\[error\] .*? metric_type must be either resty.wasmx.shm.metrics.COUNTER, resty.wasmx.shm.metrics.GAUGE, or resty.wasmx.shm.metrics.HISTOGRAM/
--- no_error_log
[crit]
[emerg]
[alert]
defined counter
[stub]
[stub]
[stub]
