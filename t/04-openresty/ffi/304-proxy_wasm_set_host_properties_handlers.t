# vim:set ft= ts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm::Lua;

skip_no_openresty();

plan_tests(5);
run_tests();

__DATA__

=== TEST 1: set_host_properties_handlers() - bad arguments
--- http_config
    init_worker_by_lua_block {
        local proxy_wasm = require "resty.wasmx.proxy_wasm"

        local function f()
        end

        local pok, perr = pcall(proxy_wasm.set_host_properties_handlers, false)
        if not pok then
            ngx.log(ngx.ERR, perr)
        end

        pok, perr = pcall(proxy_wasm.set_host_properties_handlers, f, false)
        if not pok then
            ngx.log(ngx.ERR, perr)
        end
    }
--- config
    location /t {
        rewrite_by_lua_block {
            ngx.say("ok")
        }
    }
--- response_body
ok
--- error_log eval
[
    qr/\[error\] .*? init_worker_by_lua.*?: getter must be a function/,
    qr/\[error\] .*? init_worker_by_lua.*?: setter must be a function/,
]
--- no_error_log
[crit]



=== TEST 2: set_host_properties_handlers() - no getter, no setter
--- wasm_modules: hostcalls
--- config
    location /t {
        rewrite_by_lua_block {
            local proxy_wasm = require "resty.wasmx.proxy_wasm"

            local pok, perr = pcall(proxy_wasm.set_host_properties_handlers)
            if not pok then
                ngx.log(ngx.ERR, perr)
            end

            ngx.say("ok")
        }
    }
--- response_body
ok
--- error_log eval
qr/\[error\] .*? getter or setter required/
--- no_error_log
[crit]
[emerg]



=== TEST 3: set_host_properties_handlers() - handlers already set
--- wasm_modules: hostcalls
--- http_config
    init_worker_by_lua_block {
        local proxy_wasm = require "resty.wasmx.proxy_wasm"
        local filters = {
            { name = "hostcalls" },
        }

        _G.c_plan = assert(proxy_wasm.new(filters))
        assert(proxy_wasm.load(_G.c_plan))
    }
--- config
    location /t {
        rewrite_by_lua_block {
            local proxy_wasm = require "resty.wasmx.proxy_wasm"

            assert(proxy_wasm.attach(_G.c_plan))

            local ok, err = proxy_wasm.set_host_properties_handlers(function() end,
  function() end)
            if not ok then
                ngx.log(ngx.ERR, err)
            end

            local pok, perr = pcall(proxy_wasm.set_host_properties_handlers, function() end)
            if not pok then
                ngx.log(ngx.ERR, perr)
            end

            ngx.say("ok")
        }
    }
--- response_body
ok
--- grep_error_log eval: qr/\[error\].*/
--- grep_error_log_out eval
qr/\[error\] .*? host properties handlers already set/
--- no_error_log
[crit]
[emerg]



=== TEST 4: set_host_properties_handlers() - fails in init_worker_by_lua
--- http_config
    init_worker_by_lua_block {
        local proxy_wasm = require "resty.wasmx.proxy_wasm"

        local function f()
        end

        assert(proxy_wasm.set_host_properties_handlers(f, f))
    }
--- config
    location /t {
        rewrite_by_lua_block {
            ngx.say("ok")
        }
    }
--- response_body
ok
--- error_log eval
[
    qr/\[error\] .*? cannot set host properties handlers outside of a request/,
    qr/\[error\] .*? init_worker_by_lua.*?: could not set host properties handlers/,
]
--- no_error_log
[crit]
