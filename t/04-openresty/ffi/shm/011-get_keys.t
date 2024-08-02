# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasmX::Lua;

skip_no_openresty();

add_block_preprocessor(sub {
    my $block = shift;
    if (!defined $block->main_config) {
        $block->set_value("main_config", <<_EOC_
            wasm {
                shm_kv kv 16k;
                shm_queue q 16k;
            }
_EOC_
        );
    }
});

plan_tests(4);
run_tests();

__DATA__

=== TEST 1: shm - get_keys() default limit
--- valgrind
--- config
    location /t {
        access_by_lua_block {
            local shm = require "resty.wasmx.shm"

            shm.kv:set("k1", "value")
            shm.kv:set("k2", "value")

            for _, k in ipairs(shm.kv:get_keys()) do
                ngx.say(k)
            end
        }
    }
--- response_body
k1
k2
--- no_error_log
[error]
[crit]



=== TEST 2: shm - get_keys() with limit
--- config
    location /t {
        access_by_lua_block {
            local shm = require "resty.wasmx.shm"

            shm.kv:set("k1", "value")
            shm.kv:set("k2", "value")

            local limit = 1

            for _, k in ipairs(shm.kv:get_keys(limit)) do
                ngx.say(k)
            end
        }
    }
--- response_body
k1
--- no_error_log
[error]
[crit]



=== TEST 3: shm - get_keys() on empty shm
--- config
    location /t {
        access_by_lua_block {
            local shm = require "resty.wasmx.shm"

            ngx.say(#shm.kv:get_keys())
            ngx.say(#shm.kv:get_keys(0))
            ngx.say(#shm.kv:get_keys(1))
        }
    }
--- response_body
0
0
0
--- no_error_log
[error]
[crit]



=== TEST 4: shm - get_keys() bad args
--- config
    location /t {
        access_by_lua_block {
            local shm = require "resty.wasmx.shm"

            local _, perr = pcall(shm.kv.get_keys, {}, false)
            ngx.say(perr)

            _, perr = pcall(shm.kv.get_keys, {}, -1)
            ngx.say(perr)
        }
    }
--- response_body
max_count must be a number
max_count must be >= 0
--- no_error_log
[crit]
[emerg]
