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

=== TEST 1: shm - setup_zones() sanity
setup_zones() is silently called when resty.wasmx.shm module is loaded.
--- valgrind
--- main_config
    wasm {
        shm_kv kv1 16k;
        shm_queue q1 16k;
    }
--- config
    location /t {
        access_by_lua_block {
            local shm = require "resty.wasmx.shm"

            assert(shm.kv1)
            assert(shm.q1)
            assert(shm.metrics)

            ngx.say("ok")
        }
    }
--- response_body
ok
--- no_error_log
[error]
[crit]



=== TEST 2: shm - setup_zones() no zones
--- skip_no_debug
--- main_config
--- valgrind
--- config
    location /t {
        access_by_lua_block {
            local shm = require "resty.wasmx.shm"

            ngx.say("ok")
        }
    }
--- response_body
ok
--- error_log
no shm zones found for resty.wasmx.shm interface
--- no_error_log
[error]



=== TEST 3: shm - setup_zones() bad zone indexing
--- config
    location /t {
        access_by_lua_block {
            local shm = require "resty.wasmx.shm"

            print(shm.bad)
        }
    }
--- error_code: 500
--- response_body_like: 500 Internal Server Error
--- error_log eval
qr/runtime error: access_by_lua\(nginx\.conf:\d+\):4: resty\.wasmx\.shm: no "bad" shm configured/
--- no_error_log
[crit]



=== TEST 4: shm - calling functions on bad shm types
--- config
    location /t {
        access_by_lua_block {
            local shm = require "resty.wasmx.shm"

            local _, perr = pcall(function()
                shm.q:get()
            end)
            ngx.say(perr)

            _, perr = pcall(function()
                shm.q:set()
            end)
            ngx.say(perr)

            _, perr = pcall(function()
                shm.q:iterate_keys()
            end)
            ngx.say(perr)

            _, perr = pcall(function()
                shm.q:get_keys()
            end)
            ngx.say(perr)

            _, perr = pcall(function()
                shm.metrics:set()
            end)
            ngx.say(perr)

            _, perr = pcall(function()
                shm.kv:define()
            end)
            ngx.say(perr)
        }
    }
--- response_body_like
.*? attempt to call method 'get' \(a nil value\)
.*? attempt to call method 'set' \(a nil value\)
.*? attempt to call method 'iterate_keys' \(a nil value\)
.*? attempt to call method 'get_keys' \(a nil value\)
.*? attempt to call method 'set' \(a nil value\)
.*? attempt to call method 'define' \(a nil value\)
--- no_error_log
[crit]
[emerg]
