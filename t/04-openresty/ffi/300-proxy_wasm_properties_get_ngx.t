# vim:set ft= ts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasmX::Lua;

skip_no_openresty();

plan_tests(5);
run_tests();

__DATA__

=== TEST 1: get_property() - getting ngx properties fails on init_worker_by_lua
--- wasm_modules: hostcalls
--- http_config
    init_worker_by_lua_block {
        local proxy_wasm = require "resty.wasmx.proxy_wasm"
        local filters = {
            { name = "hostcalls" },
        }

        local c_plan = assert(proxy_wasm.new(filters))
        assert(proxy_wasm.load(c_plan))

        local ok, err = proxy_wasm.get_property("ngx.hostname")
        if not ok then
            ngx.log(ngx.ERR, err)
            return
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
    qr/\[error\] .*? cannot get ngx properties outside of a request/,
    qr/\[error\] .*? init_worker_by_lua(\(nginx\.conf:\d+\))?:\d+: unknown error/,
]
--- no_error_log
[crit]



=== TEST 2: get_property() - getting ngx properties works on rewrite_by_lua, before start
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
    set $my_var 456;

    location /t {
        rewrite_by_lua_block {
            local proxy_wasm = require "resty.wasmx.proxy_wasm"
            assert(proxy_wasm.attach(_G.c_plan))

            local prop, err = proxy_wasm.get_property("ngx.my_var")
            if prop then
                ngx.log(ngx.INFO, "ngx.my_var is ", prop)
            end
        }

        content_by_lua_block {
            ngx.say("ok")
        }
    }
--- response_body
ok
--- error_log eval
qr/\[info\] .*? ngx.my_var is 456/
--- no_error_log
[error]
[crit]



=== TEST 3: get_property() - getting ngx properties works on access_by_lua, before start
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
    set $my_var 456;
    location /t {
        proxy_wasm_request_headers_in_access on;

        access_by_lua_block {
            local proxy_wasm = require "resty.wasmx.proxy_wasm"
            assert(proxy_wasm.attach(_G.c_plan))

            local prop, err = proxy_wasm.get_property("ngx.my_var")
            if prop then
                ngx.log(ngx.INFO, "ngx.my_var is ", prop)
            end
        }

        content_by_lua_block {
            ngx.say("ok")
        }
    }
--- response_body
ok
--- error_log eval
qr/\[info\] .*? ngx.my_var is 456/
--- no_error_log
[error]
[crit]



=== TEST 4: get_property() - getting ngx properties works on header_filter_by_lua
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
    set $my_var 456;

    location /t {
        proxy_wasm_request_headers_in_access on;

        access_by_lua_block {
            local proxy_wasm = require "resty.wasmx.proxy_wasm"
            assert(proxy_wasm.attach(_G.c_plan))
        }

        content_by_lua_block {
            ngx.say("ok")
        }

        header_filter_by_lua_block {
            local proxy_wasm = require "resty.wasmx.proxy_wasm"

            local prop, err = proxy_wasm.get_property("ngx.my_var")
            if prop then
                ngx.log(ngx.INFO, "ngx.my_var is ", prop)
            end
        }
    }
--- response_body
ok
--- error_log eval
qr/\[info\] .*? ngx.my_var is 456/
--- no_error_log
[error]
[crit]



=== TEST 5: get_property() - getting ngx properties works on body_filter_by_lua
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
    set $my_var 456;

    location /t {
        proxy_wasm_request_headers_in_access on;

        access_by_lua_block {
            local proxy_wasm = require "resty.wasmx.proxy_wasm"
            assert(proxy_wasm.attach(_G.c_plan))
        }

        content_by_lua_block {
            ngx.say("ok")
        }

        body_filter_by_lua_block {
            local proxy_wasm = require "resty.wasmx.proxy_wasm"

            local prop, err = proxy_wasm.get_property("ngx.my_var")
            if prop then
                ngx.log(ngx.INFO, "ngx.my_var is ", prop)
            end
       }
    }
--- response_body
ok
--- error_log eval
qr/\[info\] .*? ngx.my_var is 456/
--- no_error_log
[error]
[crit]



=== TEST 6: get_property() - getting ngx properties works on log_by_lua
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
    set $my_var 456;

    location /t {
        proxy_wasm_request_headers_in_access on;

        access_by_lua_block {
            local proxy_wasm = require "resty.wasmx.proxy_wasm"
            assert(proxy_wasm.attach(_G.c_plan))
        }

        content_by_lua_block {
            ngx.say("ok")
        }

        log_by_lua_block {
            local proxy_wasm = require "resty.wasmx.proxy_wasm"

            local prop, err = proxy_wasm.get_property("ngx.my_var")
            if prop then
                ngx.log(ngx.INFO, "ngx.my_var is ", prop)
            end
        }
    }
--- response_body
ok
--- error_log eval
qr/\[info\] .*? ngx.my_var is 456/
--- no_error_log
[error]
[crit]



=== TEST 7: get_property() - without loading a plan
--- config
    location /t {
        access_by_lua_block {
            local proxy_wasm = require "resty.wasmx.proxy_wasm"

            local prop, err = proxy_wasm.get_property("ngx.my_var")
            if not prop then
                ngx.log(ngx.ERR, err)
            end

            ngx.say("ok")
        }
    }
--- response_body
ok
--- error_log eval
qr/\[error\] .*? access_by_lua.*?: unknown error/
--- no_error_log
[crit]
[emerg]
