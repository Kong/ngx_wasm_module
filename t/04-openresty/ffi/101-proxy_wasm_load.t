# vim:set ft= ts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasmX::Lua;

skip_no_openresty();

plan_tests(4 + 3);
run_tests();

__DATA__

=== TEST 1: load() - bad argument
--- http_config
    init_by_lua_block {
        local proxy_wasm = require "resty.wasmx.proxy_wasm"
        proxy_wasm.load({})
    }
--- error_log eval
qr/\[error\] .*? plan should be a cdata object/
--- no_error_log
[crit]
[emerg]
--- must_die



=== TEST 2: load() - bad filter in chain
--- wasm_modules: ngx_rust_tests
--- http_config
    init_worker_by_lua_block {
        local proxy_wasm = require "resty.wasmx.proxy_wasm"
        local filters = {
            { name = "ngx_rust_tests" },
        }

        local c_plan, err = proxy_wasm.new(filters)
        if not c_plan then
            ngx.log(ngx.ERR, err)
            return
        end

        local ok, err = proxy_wasm.load(c_plan)
        if not ok then
            ngx.log(ngx.ERR, err)
        end
    }
--- error_log eval
[
    qr/\[emerg\] .*? failed linking "ngx_rust_tests" module with "ngx_proxy_wasm" host interface: incompatible host interface/,
    qr/\[error\] .*? failed loading plan/
]
--- no_error_log
[crit]



=== TEST 3: load() - cannot be called in init
ngx_lua_module runs init on postconfig, meaning upon parsing the http{} block,
while our earliest handler is currently on module initialization, which comes
after config parsing.

To support this we'd need to start and load ngx_wavm the in master process, in
the wasm{} block postconfig handler. It feels unnecessary at the moment.

--- wasm_modules: on_tick
--- http_config
    init_by_lua_block {
        local proxy_wasm = require "resty.wasmx.proxy_wasm"
        local filters = {
            { name = "on_tick" },
        }

        local c_plan = assert(proxy_wasm.new(filters))
        proxy_wasm.load(c_plan)
    }
--- error_log eval
qr/\[error\] .*? load cannot be called from 'init' phase/
--- no_error_log
[crit]
[emerg]
--- must_die



=== TEST 4: load() - sanity in init_worker
--- wasm_modules: on_phases
--- http_config
    init_worker_by_lua_block {
        local proxy_wasm = require "resty.wasmx.proxy_wasm"
        local filters = {
            { name = "on_phases" },
        }

        local c_plan, err = proxy_wasm.new(filters)
        if not c_plan then
            ngx.log(ngx.ERR, err)
            return
        end

        local ok, err = proxy_wasm.load(c_plan)
        if not ok then
            ngx.log(ngx.ERR, err)
            return
        end
    }
--- grep_error_log eval: qr/#\d+ on_configure.*/
--- grep_error_log_out eval
qr/#0 on_configure, config_size: 0.*/
--- no_error_log
[error]
[crit]



=== TEST 5: load() - with a ticking filter
--- wasm_modules: on_tick
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

        local ok, err = proxy_wasm.load(c_plan)
        if not ok then
            ngx.log(ngx.ERR, err)
            return
        end
    }
--- error_log eval
[
    qr/\[info\] .*? ticking/,
    qr/\[info\] .*? ticking/,
]
--- no_error_log
[error]



=== TEST 6: load() - on_configure failures
--- ONLY
--- wasm_modules: hostcalls
--- http_config
    init_worker_by_lua_block {
        local proxy_wasm = require "resty.wasmx.proxy_wasm"

        local function load_filters(filters)
            local c_plan, err = proxy_wasm.new(filters)
            if not c_plan then
                return nil, err
            end

            local ok, err = proxy_wasm.load(c_plan)
            if not ok then
                return nil, err
            end

            return c_plan
        end

        local c_plan, err = load_filters({
            { name = "hostcalls", config = "on_configure=do_return_false" },
        })

        if c_plan ~= nil then
            ngx.log(ngx.ERR, "expected failure when 'on_configure' returns false")
            return

        elseif err ~= "failed loading plan" then
            ngx.log(ngx.ERR, "expected 'failed loading plan' error, got: ", err)
        end

        c_plan, err = load_filters({
            { name = "hostcalls", config = "on=request_headers test=/t/set_response_header value=X-Test-Case:1234", }
        })

        if c_plan then
            _G.c_plan = c_plan
            ngx.log(ngx.INFO, "successs")
        else
            ngx.log(ngx.ERR, "expected valid filters to be loaded: ", err)
        end
    }
--- config
    location /t {
        rewrite_by_lua_block {
            local proxy_wasm = require "resty.wasmx.proxy_wasm"

            if not _G.c_plan then
                return ngx.say("no c_plan found")
            end

            local ok, err = proxy_wasm.attach(_G.c_plan)
            if not ok then
                return ngx.say(err)
            end

            ok, err = proxy_wasm.start()
            if not ok then
                return ngx.say(err)
            end

            local ok, err = proxy_wasm.start()
            if not ok then
                return ngx.say(err)
            end

            ngx.say("SUCCESS")
        }
    }
--- request
GET /t
--- response_body
SUCCESS
--- response_headers
X-Test-Case: 1234
--- error_log eval
[
    qr/failed initializing "hostcalls" filter \(on_configure failure\)/,
    qr/\[info\] .*? success/,
]
--- no_error_log
[error]
