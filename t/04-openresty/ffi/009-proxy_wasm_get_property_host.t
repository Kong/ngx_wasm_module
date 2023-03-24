# vim:set ft= ts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm::Lua;

skip_no_openresty();

plan tests => repeat_each() * (blocks() * 4);

run_tests();

__DATA__

=== TEST 1: get_property() - getting host properties fails on init_worker_by_lua
--- wasm_modules: hostcalls
--- http_config
    init_worker_by_lua_block {
        local proxy_wasm = require "resty.http.proxy_wasm"
        local filters = {
            { name = "hostcalls" },
        }

        local c_plan = assert(proxy_wasm.new(filters))
        assert(proxy_wasm.load(c_plan))

        local ok, err = proxy_wasm.get_property("wasmx.my_prop")
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
    qr/\[error\] .*? cannot get host properties outside of a request/,
    qr/\[error\] .*? init_worker_by_lua:\d+: unknown error/,
]



=== TEST 2: get_property() - getting host properties works on rewrite_by_lua, before start
--- wasm_modules: hostcalls
--- http_config
    init_worker_by_lua_block {
        local proxy_wasm = require "resty.http.proxy_wasm"
        local filters = {
            { name = "hostcalls" },
        }

        _G.c_plan = assert(proxy_wasm.new(filters))
        assert(proxy_wasm.load(_G.c_plan))
    }
--- config
    location /t {
        rewrite_by_lua_block {
            local proxy_wasm = require "resty.http.proxy_wasm"
            assert(proxy_wasm.attach(_G.c_plan))
            assert(proxy_wasm.set_property("wasmx.my_prop", "my_value"))

            local prop, err = proxy_wasm.get_property("wasmx.my_prop")
            if prop then
                ngx.log(ngx.INFO, "wasmx.my_prop is ", prop)
            end

            assert(proxy_wasm.start())

            ngx.say("ok")
        }
    }
--- response_body
ok
--- error_log eval
qr/\[info\] .*? wasmx.my_prop is my_value/
--- no_error_log
[crit]



=== TEST 3: get_property() - getting host properties works on access_by_lua, before start
--- wasm_modules: hostcalls
--- http_config
    init_worker_by_lua_block {
        local proxy_wasm = require "resty.http.proxy_wasm"
        local filters = {
            { name = "hostcalls" },
        }

        _G.c_plan = assert(proxy_wasm.new(filters))
        assert(proxy_wasm.load(_G.c_plan))
    }
--- config
    location /t {
        proxy_wasm_request_headers_in_access on;

        access_by_lua_block {
            local proxy_wasm = require "resty.http.proxy_wasm"
            assert(proxy_wasm.attach(_G.c_plan))
            assert(proxy_wasm.set_property("wasmx.my_prop", "my_value"))

            local prop, err = proxy_wasm.get_property("wasmx.my_prop")
            if prop then
                ngx.log(ngx.INFO, "wasmx.my_prop is ", prop)
            end

            assert(proxy_wasm.start())

            ngx.say("ok")
        }
    }
--- response_body
ok
--- error_log eval
qr/\[info\] .*? wasmx.my_prop is my_value/
--- no_error_log
[crit]



=== TEST 4: get_property() - getting host properties works on header_filter_by_lua
--- wasm_modules: hostcalls
--- http_config
    init_worker_by_lua_block {
        local proxy_wasm = require "resty.http.proxy_wasm"
        local filters = {
            { name = "hostcalls" },
        }

        _G.c_plan = assert(proxy_wasm.new(filters))
        assert(proxy_wasm.load(_G.c_plan))
    }
--- config
    location /t {
        proxy_wasm_request_headers_in_access on;

        access_by_lua_block {
            local proxy_wasm = require "resty.http.proxy_wasm"
            assert(proxy_wasm.attach(_G.c_plan))
            assert(proxy_wasm.set_property("wasmx.my_prop", "my_value"))
            assert(proxy_wasm.start())
            ngx.say("ok")
        }

        header_filter_by_lua_block {
            local proxy_wasm = require "resty.http.proxy_wasm"

            local prop, err = proxy_wasm.get_property("wasmx.my_prop")
            if prop then
                ngx.log(ngx.INFO, "wasmx.my_prop is ", prop)
            end
        }
    }
--- response_body
ok
--- error_log eval
qr/\[info\] .*? wasmx.my_prop is my_value/
--- no_error_log
[crit]



=== TEST 5: get_property() - getting host properties works on body_filter_by_lua
--- wasm_modules: hostcalls
--- http_config
    init_worker_by_lua_block {
        local proxy_wasm = require "resty.http.proxy_wasm"
        local filters = {
            { name = "hostcalls" },
        }

        _G.c_plan = assert(proxy_wasm.new(filters))
        assert(proxy_wasm.load(_G.c_plan))
    }
--- config
    location /t {
        proxy_wasm_request_headers_in_access on;

        access_by_lua_block {
            local proxy_wasm = require "resty.http.proxy_wasm"
            assert(proxy_wasm.attach(_G.c_plan))
            assert(proxy_wasm.set_property("wasmx.my_prop", "my_value"))
            assert(proxy_wasm.start())
            ngx.say("ok")
        }

        body_filter_by_lua_block {
            local proxy_wasm = require "resty.http.proxy_wasm"

            local prop, err = proxy_wasm.get_property("wasmx.my_prop")
            if prop then
                ngx.log(ngx.INFO, "wasmx.my_prop is ", prop)
            end
       }
    }
--- response_body
ok
--- error_log eval
qr/\[info\] .*? wasmx.my_prop is my_value/
--- no_error_log
[crit]



=== TEST 6: get_property() - getting host properties works on log_by_lua
--- wasm_modules: hostcalls
--- http_config
    init_worker_by_lua_block {
        local proxy_wasm = require "resty.http.proxy_wasm"
        local filters = {
            { name = "hostcalls" },
        }

        _G.c_plan = assert(proxy_wasm.new(filters))
        assert(proxy_wasm.load(_G.c_plan))
    }
--- config
    location /t {
        proxy_wasm_request_headers_in_access on;

        access_by_lua_block {
            local proxy_wasm = require "resty.http.proxy_wasm"
            assert(proxy_wasm.attach(_G.c_plan))
            assert(proxy_wasm.set_property("wasmx.my_prop", "my_value"))
            assert(proxy_wasm.start())
            ngx.say("ok")
        }

        log_by_lua_block {
            local proxy_wasm = require "resty.http.proxy_wasm"

            local prop, err = proxy_wasm.get_property("wasmx.my_prop")
            if prop then
                ngx.log(ngx.INFO, "wasmx.my_prop is ", prop)
            end
        }
    }
--- response_body
ok
--- error_log eval
qr/\[info\] .*? wasmx.my_prop is my_value/
--- no_error_log
[crit]



=== TEST 7: get_property() - getting host properties returns nil when unset
--- wasm_modules: hostcalls
--- http_config
    init_worker_by_lua_block {
        local proxy_wasm = require "resty.http.proxy_wasm"
        local filters = {
            { name = "hostcalls" },
        }

        _G.c_plan = assert(proxy_wasm.new(filters))
        assert(proxy_wasm.load(_G.c_plan))
    }
--- config
    location /t {
        proxy_wasm_request_headers_in_access on;

        access_by_lua_block {
            local proxy_wasm = require "resty.http.proxy_wasm"
            assert(proxy_wasm.attach(_G.c_plan))
            assert(proxy_wasm.start())

            local prop, err, ecode = proxy_wasm.get_property("wasmx.my_prop")
            assert(prop == nil)
            assert(err == "property \"wasmx.my_prop\" not found")
            assert(ecode == "missing")

            ngx.say("ok")
        }
    }
--- response_body
ok
--- no_error_log
[error]
[crit]



=== TEST 8: get_property() - getting host properties can get an empty string
--- wasm_modules: hostcalls
--- http_config
    init_worker_by_lua_block {
        local proxy_wasm = require "resty.http.proxy_wasm"
        local filters = {
            { name = "hostcalls" },
        }

        _G.c_plan = assert(proxy_wasm.new(filters))
        assert(proxy_wasm.load(_G.c_plan))
    }
--- config
    location /t {
        proxy_wasm_request_headers_in_access on;

        access_by_lua_block {
            local proxy_wasm = require "resty.http.proxy_wasm"
            assert(proxy_wasm.attach(_G.c_plan))
            assert(proxy_wasm.set_property("wasmx.my_prop", ""))
            assert(proxy_wasm.start())

            local prop, err = proxy_wasm.get_property("wasmx.my_prop")
            ngx.log(ngx.INFO, "wasmx.my_prop: \"", prop, "\", err: ", tostring(err))
            assert(prop == "")
            assert(err == nil)

            ngx.say("ok")
        }
    }
--- response_body
ok
--- error_log eval
qr/\[info\] .*? wasmx.my_prop: "", err: nil/
--- no_error_log
[crit]
