# vim:set ft= ts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm::Lua;

our $nginxV = $t::TestWasm::nginxV;

skip_no_openresty();

plan tests => repeat_each() * (blocks() * 4);

run_tests();

__DATA__

=== TEST 1: new() - no VM
--- http_config
    init_by_lua_block {
        local proxy_wasm = require "resty.wasmx.proxy_wasm"
        proxy_wasm.new({})
    }
--- error_log
no wasm vm
--- no_error_log
[crit]
[emerg]
--- must_die



=== TEST 2: new() - bad argument
--- http_config
    init_by_lua_block {
        local proxy_wasm = require "resty.wasmx.proxy_wasm"
        proxy_wasm.new()
    }
--- error_log
filters must be a table
--- no_error_log
[crit]
[emerg]
--- must_die



=== TEST 3: new() - bad module
--- wasm_modules: on_phases
--- http_config
    init_worker_by_lua_block {
        local proxy_wasm = require "resty.wasmx.proxy_wasm"
        local filters = {
            { name = "on_tick" },
        }

        local c_plan, err = proxy_wasm.new(filters)
        if not c_plan then
            ngx.log(ngx.ERR, err)
            return
        end
    }
--- error_log eval
qr/\[error\] .*? no "on_tick" module defined/
--- no_error_log
[crit]
[emerg]



=== TEST 4: new() - sanity
--- wasm_modules: on_phases hostcalls
--- config
    location /t {
        access_by_lua_block {
            local proxy_wasm = require "resty.wasmx.proxy_wasm"
            local filters = {
                { name = "on_phases", config = "a" },
                { name = "hostcalls", config = "b" },
            }

            local c_plan, err = proxy_wasm.new(filters)
            if not c_plan then
                return ngx.say(err)
            end

            ngx.say("plan: ", tostring(c_plan))
        }
    }
--- response_body_like
plan: cdata<struct ngx_wasm_ops_plan_t \*>: 0x[0-9a-f]+
--- no_error_log
[error]
[crit]



=== TEST 5: new() - plan is garbage collected
--- skip_eval: 4: $::nginxV !~ m/debug/
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: on_phases hostcalls
--- config
    location /t {
        access_by_lua_block {
            local proxy_wasm = require "resty.wasmx.proxy_wasm"
            local filters = {
                { name = "on_phases", config = "a" },
                { name = "hostcalls", config = "b" },
            }

            local c_plan, err = proxy_wasm.new(filters)
            if not c_plan then
                return ngx.say(err)
            end

            c_plan = nil

            collectgarbage()
            collectgarbage()
        }

        echo -n;
    }
--- error_log eval
qr/\[debug\].*?freeing plan.*/
--- no_error_log
[error]
[crit]
