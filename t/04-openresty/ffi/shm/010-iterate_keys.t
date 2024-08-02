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

=== TEST 1: shm - iterate_keys() with page_size > nelts
--- skip_no_debug
--- valgrind
--- config
    location /t {
        access_by_lua_block {
            local shm = require "resty.wasmx.shm"

            shm.kv:set("k1", "value")
            shm.kv:set("k2", "value")
            shm.kv:set("k3", "value")

            shm.kv:lock()

            for k in shm.kv:iterate_keys() do
                ngx.say(k)
            end

            shm.kv:unlock()
        }
    }
--- response_body
k3
k2
k1
--- grep_error_log: iterate_keys fetched a new page
--- grep_error_log_out
iterate_keys fetched a new page
--- no_error_log
[error]



=== TEST 2: shm - iterate_keys() with page_size < nelts
--- skip_no_debug
--- config
    location /t {
        access_by_lua_block {
            local shm = require "resty.wasmx.shm"

            shm.kv:set("k1", "value")
            shm.kv:set("k2", "value")
            shm.kv:set("k3", "value")

            shm.kv:lock()

            for k in shm.kv:iterate_keys({ page_size = 2 }) do
                ngx.say(k)
            end

            shm.kv:unlock()
        }
    }
--- response_body
k3
k2
k1
--- grep_error_log: iterate_keys fetched a new page
--- grep_error_log_out
iterate_keys fetched a new page
iterate_keys fetched a new page
--- no_error_log
[error]



=== TEST 3: shm - iterate_keys() with page_size == nelts
--- skip_no_debug
--- config
    location /t {
        access_by_lua_block {
            local shm = require "resty.wasmx.shm"

            shm.kv:set("k1", "value")
            shm.kv:set("k2", "value")
            shm.kv:set("k3", "value")

            shm.kv:lock()

            for k in shm.kv:iterate_keys({ page_size = 3 }) do
                ngx.say(k)
            end

            shm.kv:unlock()
        }
    }
--- response_body
k3
k2
k1
--- grep_error_log: iterate_keys fetched a new page
--- grep_error_log_out
iterate_keys fetched a new page
--- no_error_log
[error]



=== TEST 4: shm - iterate_keys() on empty shm
--- config
    location /t {
        access_by_lua_block {
            local shm = require "resty.wasmx.shm"

            shm.kv:lock()

            for k in shm.kv:iterate_keys() do
                ngx.say("fail")
            end

            shm.kv:unlock()

            ngx.say("ok")
        }
    }
--- response_body
ok
--- no_error_log
[error]
[crit]



=== TEST 5: shm - iterate_keys() on unlocked shm
--- config
    location /t {
        access_by_lua_block {
            local shm = require "resty.wasmx.shm"

            for k in shm.kv:iterate_keys() do

            end
        }
    }
--- error_code: 500
--- error_log eval
qr/\[error\] .*? attempt to call kv:iterate_keys\(\) but the shm zone is not locked; invoke :lock\(\) before and :unlock\(\) after./
--- no_error_log
[crit]
[emerg]



=== TEST 6: shm - iterate_keys() bad args
--- config
    location /t {
        access_by_lua_block {
            local shm = require "resty.wasmx.shm"

            local _, perr = pcall(shm.kv.iterate_keys, {}, true)
            ngx.say(perr)

            _, perr = pcall(shm.kv.iterate_keys, {}, { page_size = true })
            ngx.say(perr)

            _, perr = pcall(shm.kv.iterate_keys, {}, { page_size = 0 })
            ngx.say(perr)
        }
    }
--- response_body
opts must be a table
opts.page_size must be a number
opts.page_size must be > 0
--- no_error_log
[crit]
[emerg]



=== TEST 7: shm - iterate_keys() can use get() while iterating
get() does not wait on lock if the shm is already locked.
--- config
    location /t {
        access_by_lua_block {
            local shm = require "resty.wasmx.shm"

            shm.kv:set("k1", "v1")
            shm.kv:set("k2", "v2")
            shm.kv:set("k3", "v3")

            shm.kv:lock()

            for k in shm.kv:iterate_keys() do
                local v = shm.kv:get(k)
                ngx.say(v)
            end

            shm.kv:unlock()
        }
    }
--- response_body
v3
v2
v1
--- no_error_log
[error]
[crit]
