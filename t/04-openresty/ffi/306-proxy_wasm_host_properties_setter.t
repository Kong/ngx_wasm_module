# vim:set ft= ts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasmX::Lua;

skip_no_openresty();

plan_tests(5);
run_tests();

__DATA__

=== TEST 1: host properties setter - sanity in rewrite_by_lua
Also testing that setting the getter in-between attach() and start() works.
--- wasm_modules: hostcalls
--- http_config
    init_worker_by_lua_block {
        local proxy_wasm = require "resty.wasmx.proxy_wasm"
        local filters = {
            { name = "hostcalls", config="on=response_body " ..
                                         "test=/t/log/property " ..
                                         "name=wasmx.my_property" },
        }

        _G.c_plan = assert(proxy_wasm.new(filters))
        assert(proxy_wasm.load(_G.c_plan))
    }
--- config
    location /t {
        rewrite_by_lua_block {
            local proxy_wasm = require "resty.wasmx.proxy_wasm"

            local tbl = {}

            local function setter(key, value)
                local new_value = value:upper()
                tbl[key] = new_value
                return true, new_value
            end

            assert(proxy_wasm.attach(_G.c_plan))
            assert(proxy_wasm.set_host_properties_handlers(nil, setter))
            assert(proxy_wasm.set_property("wasmx.my_property", "my_value"))
        }

        content_by_lua_block {
            ngx.say("ok")
        }
    }
--- response_body
ok
--- grep_error_log eval: qr/wasmx.\w+_property: \w+/
--- grep_error_log_out
wasmx.my_property: MY_VALUE
wasmx.my_property: MY_VALUE
--- no_error_log
[error]
[crit]



=== TEST 2: host properties setter - sanity in access_by_lua
--- wasm_modules: hostcalls
--- http_config
    init_worker_by_lua_block {
        local proxy_wasm = require "resty.wasmx.proxy_wasm"
        local filters = {
            { name = "hostcalls", config="on=response_body " ..
                                         "test=/t/log/property " ..
                                         "name=wasmx.my_property" },
        }

        _G.c_plan = assert(proxy_wasm.new(filters))
        assert(proxy_wasm.load(_G.c_plan))
    }
--- config
    location /t {
        access_by_lua_block {
            local proxy_wasm = require "resty.wasmx.proxy_wasm"

            local tbl = {}

            local function setter(key, value)
                local new_value = value:upper()
                tbl[key] = new_value
                return true, new_value
            end

            assert(proxy_wasm.attach(_G.c_plan))
            assert(proxy_wasm.set_host_properties_handlers(nil, setter))
            assert(proxy_wasm.set_property("wasmx.my_property", "my_value"))
        }

        content_by_lua_block {
            ngx.say("ok")
        }
    }
--- response_body
ok
--- grep_error_log eval: qr/wasmx.\w+_property: \w+/
--- grep_error_log_out
wasmx.my_property: MY_VALUE
wasmx.my_property: MY_VALUE
--- no_error_log
[error]
[crit]



=== TEST 3: host properties setter - sanity in header_filter_by_lua
Also testing that setting the getter after attach() and start() works.
--- wasm_modules: hostcalls
--- http_config
    init_worker_by_lua_block {
        local proxy_wasm = require "resty.wasmx.proxy_wasm"
        local filters = {
            { name = "hostcalls", config="on=response_body " ..
                                         "test=/t/log/property " ..
                                         "name=wasmx.my_property" },
        }

        _G.c_plan = assert(proxy_wasm.new(filters))
        assert(proxy_wasm.load(_G.c_plan))
    }
--- config
    location /t {
        rewrite_by_lua_block {
            local proxy_wasm = require "resty.wasmx.proxy_wasm"
            assert(proxy_wasm.attach(_G.c_plan))
        }

        content_by_lua_block {
            ngx.say("ok")
        }

        header_filter_by_lua_block {
            local proxy_wasm = require "resty.wasmx.proxy_wasm"

            local tbl = {}

            local function setter(key, value)
                local new_value = value:upper()
                tbl[key] = new_value
                return true, new_value
            end

            assert(proxy_wasm.set_host_properties_handlers(nil, setter))
            assert(proxy_wasm.set_property("wasmx.my_property", "my_value"))
        }
    }
--- response_body
ok
--- grep_error_log eval: qr/wasmx.\w+_property: \w+/
--- grep_error_log_out
wasmx.my_property: MY_VALUE
wasmx.my_property: MY_VALUE
--- no_error_log
[error]
[crit]



=== TEST 4: host properties setter - sanity in body_filter_by_lua
--- wasm_modules: hostcalls
--- http_config
    init_worker_by_lua_block {
        local proxy_wasm = require "resty.wasmx.proxy_wasm"
        local filters = {
            { name = "hostcalls", config="on=response_body " ..
                                         "test=/t/log/property " ..
                                         "name=wasmx.my_property" },
        }

        _G.c_plan = assert(proxy_wasm.new(filters))
        assert(proxy_wasm.load(_G.c_plan))
    }
--- config
    location /t {
        rewrite_by_lua_block {
            local proxy_wasm = require "resty.wasmx.proxy_wasm"
            assert(proxy_wasm.attach(_G.c_plan))
        }

        content_by_lua_block {
            ngx.say("ok")
        }

        body_filter_by_lua_block {
            local proxy_wasm = require "resty.wasmx.proxy_wasm"

            local tbl = {}

            local function setter(key, value)
                local new_value = value:upper()
                tbl[key] = new_value
                return true, new_value
            end

            if not _G.set then
                assert(proxy_wasm.set_host_properties_handlers(nil, setter))
                _G.set = true
            end

            assert(proxy_wasm.set_property("wasmx.my_property", "my_value"))
        }
    }
--- response_body
ok
--- grep_error_log eval: qr/wasmx.\w+_property: \w+/
--- grep_error_log_out
wasmx.my_property: MY_VALUE
wasmx.my_property: MY_VALUE
--- no_error_log
[error]
[crit]



=== TEST 5: host properties setter - sanity in log_by_lua
--- wasm_modules: hostcalls
--- http_config
    init_worker_by_lua_block {
        local proxy_wasm = require "resty.wasmx.proxy_wasm"
        local filters = {
            { name = "hostcalls", config="on=log " ..
                                         "test=/t/log/property " ..
                                         "name=wasmx.my_property" },
        }

        _G.c_plan = assert(proxy_wasm.new(filters))
        assert(proxy_wasm.load(_G.c_plan))
    }
--- config
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

            local tbl = {}

            local function setter(key, value)
                local new_value = value:upper()
                tbl[key] = new_value
                return true, new_value
            end

            assert(proxy_wasm.set_host_properties_handlers(nil, setter))
            assert(proxy_wasm.set_property("wasmx.my_property", "my_value"))
        }
    }
--- response_body
ok
--- grep_error_log eval: qr/wasmx.\w+_property: \w+/
--- grep_error_log_out
wasmx.my_property: MY_VALUE
--- no_error_log
[error]
[crit]



=== TEST 6: host properties setter - setter returning falsy and an optional error
With and without 2nd return value (err)
--- wasm_modules: hostcalls
--- http_config
    init_worker_by_lua_block {
        local proxy_wasm = require "resty.wasmx.proxy_wasm"
        local filters = {
            { name = "hostcalls", config="on=log " ..
                                         "test=/t/log/property " ..
                                         "name=wasmx.fail1_property" },
        }

        _G.c_plan = assert(proxy_wasm.new(filters))
        assert(proxy_wasm.load(_G.c_plan))
    }
--- config
    location /t {
        access_by_lua_block {
            local proxy_wasm = require "resty.wasmx.proxy_wasm"

            local function setter(key, value)
                if key == "wasmx.fail1_property" then
                    return nil, "first wasmx property custom error"

                elseif key == "wasmx.fail2_property" then
                    return false, "second wasmx property custom error"

                elseif key == "wasmx.fail3_property" then
                    return nil

                elseif key == "wasmx.fail4_property" then
                    return false
                end
            end

            assert(proxy_wasm.attach(_G.c_plan))
            assert(proxy_wasm.set_host_properties_handlers(nil, setter))

            for i = 1, 4 do
                local key = "wasmx.fail" .. i .. "_property"
                local ok, err = proxy_wasm.set_property(key, "my_value")
                if not ok then
                    ngx.log(ngx.ERR, key, ": ", err)
                end
            end
        }

        content_by_lua_block {
            ngx.say("ok")
        }
    }
--- response_body
ok
--- grep_error_log eval: qr/(property not found:\s)?wasmx\.\w+_property.*?(?=(,|(\s+while)))/
--- grep_error_log_out
wasmx.fail1_property: first wasmx property custom error
wasmx.fail2_property: second wasmx property custom error
wasmx.fail3_property: unknown error
wasmx.fail4_property: unknown error
property not found: wasmx.fail1_property
--- no_error_log
[crit]
[alert]



=== TEST 7: host properties setter - setter returning true
With and without 3rd return value (caches result)
--- valgrind
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
        access_by_lua_block {
            local proxy_wasm = require "resty.wasmx.proxy_wasm"

            local tbl = {}
            local counter = 0

            local function setter(key, value)
                counter = counter + 1

                if value and #value > 0 then
                    value = value:upper() .. " " .. tostring(counter)
                end

                tbl[key] = value

                print("setting ", key, " to ", value, ", counter: ", counter)

                return true, value, (key:match("const") and true or false)
            end

            local function getter(key)
                counter = counter + 1

                print("getting ", key, " via getter, counter: ", counter)

                local value = tbl[key]
                if value and #value > 0 then
                    value = value .. " " .. tostring(counter)
                end

                return true, value
            end

            assert(proxy_wasm.attach(_G.c_plan))
            assert(proxy_wasm.set_host_properties_handlers(getter, setter))
        }

        content_by_lua_block {
            ngx.say("ok")
        }

        log_by_lua_block {
            local proxy_wasm = require "resty.wasmx.proxy_wasm"

            local function log_property(k)
                local v = proxy_wasm.get_property(k)
                print(k, ": ", type(v), " \"", v, "\"")
            end

            -- two set_property operations, each will hit the setter.
            -- four get_property operations,
            -- but only wasmx.dyn_property will hit the getter
            -- because the setter to wasmx.const_property cached the result.
            proxy_wasm.set_property("wasmx.const_property", "hello") -- sets "HELLO 1"
            log_property("wasmx.const_property") -- returns "HELLO 1"
            proxy_wasm.set_property("wasmx.dyn_property", "hi") -- sets "HI 2"
            log_property("wasmx.dyn_property") -- returns "HI 2 3"
            log_property("wasmx.const_property") -- returns "HELLO 1"
            log_property("wasmx.dyn_property") -- returns "HI 2 4"

            proxy_wasm.set_property("wasmx.nil_const_property", nil) -- sets nil
            log_property("wasmx.nil_const_property") -- returns nil
            proxy_wasm.set_property("wasmx.nil_dyn_property", nil) -- sets nil
            log_property("wasmx.nil_dyn_property") -- returns nil
            log_property("wasmx.nil_const_property") -- returns nil
            log_property("wasmx.nil_dyn_property") -- returns nil

            proxy_wasm.set_property("wasmx.empty_const_property", "") -- sets ""
            log_property("wasmx.empty_const_property") -- returns ""
            proxy_wasm.set_property("wasmx.empty_dyn_property", "") -- sets ""
            log_property("wasmx.empty_dyn_property") -- returns ""
            log_property("wasmx.empty_const_property") -- returns ""
            log_property("wasmx.empty_dyn_property") -- returns ""
        }
    }
--- response_body
ok
--- grep_error_log eval: qr/((g|s)etting )?wasmx\.\w+_property.*?(?= while)/
--- grep_error_log_out
setting wasmx.const_property to HELLO 1, counter: 1
wasmx.const_property: string "HELLO 1"
setting wasmx.dyn_property to HI 2, counter: 2
getting wasmx.dyn_property via getter, counter: 3
wasmx.dyn_property: string "HI 2 3"
wasmx.const_property: string "HELLO 1"
getting wasmx.dyn_property via getter, counter: 4
wasmx.dyn_property: string "HI 2 4"
setting wasmx.nil_const_property to nil, counter: 5
wasmx.nil_const_property: nil "nil"
setting wasmx.nil_dyn_property to nil, counter: 6
getting wasmx.nil_dyn_property via getter, counter: 7
wasmx.nil_dyn_property: nil "nil"
wasmx.nil_const_property: nil "nil"
getting wasmx.nil_dyn_property via getter, counter: 8
wasmx.nil_dyn_property: nil "nil"
setting wasmx.empty_const_property to , counter: 9
wasmx.empty_const_property: string ""
setting wasmx.empty_dyn_property to , counter: 10
getting wasmx.empty_dyn_property via getter, counter: 11
wasmx.empty_dyn_property: string ""
wasmx.empty_const_property: string ""
getting wasmx.empty_dyn_property via getter, counter: 12
wasmx.empty_dyn_property: string ""
--- no_error_log
[error]
[crit]



=== TEST 8: host properties setter - errors are caught when accessed through filters
Produces a wasm host trap.
--- wasm_modules: hostcalls
--- http_config
    init_worker_by_lua_block {
        local proxy_wasm = require "resty.wasmx.proxy_wasm"
        local filters = {
            { name = "hostcalls", config="on=response_body " ..
                                         "test=/t/set_property " ..
                                         "name=wasmx.my_property set=my_value" },
        }

        _G.c_plan = assert(proxy_wasm.new(filters))
        assert(proxy_wasm.load(_G.c_plan))
    }
--- config
    location /t {
        rewrite_by_lua_block {
            local proxy_wasm = require "resty.wasmx.proxy_wasm"

            local function setter(key, value)
                error("crash!")
            end

            assert(proxy_wasm.attach(_G.c_plan))
            assert(proxy_wasm.set_host_properties_handlers(nil, setter))
        }

        content_by_lua_block {
            ngx.say("ok")
        }
    }
--- response_body
ok
--- error_log eval
qr/host trap \(internal error\): could not set \"wasmx\.my_property\": error in property setter: rewrite_by_lua.*?: crash\!/
--- no_error_log
[crit]
[emerg]



=== TEST 9: host properties setter - errors are caught when accessed through proxy_wasm.set_property()
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

            local function setter(key, value)
                error("crash!")
            end

            assert(proxy_wasm.attach(_G.c_plan))
            assert(proxy_wasm.set_host_properties_handlers(nil, setter))

            local ok, err = proxy_wasm.set_property("wasmx.my_property", "my_value")
            if not ok then
                ngx.log(ngx.ERR, err)
            end
        }

        content_by_lua_block {
            ngx.say("ok")
        }
    }
--- response_body
ok
--- error_log eval
qr/\[error\] .*? error in property setter: rewrite_by_lua.*?: crash\!/
--- no_error_log
[crit]
[emerg]



=== TEST 10: host properties setter - setter can ngx.log()
--- wasm_modules: hostcalls
--- http_config
    init_worker_by_lua_block {
        local proxy_wasm = require "resty.wasmx.proxy_wasm"
        local filters = {
            { name = "hostcalls", config="on=response_body " ..
                                         "test=/t/set_property " ..
                                         "name=wasmx.my_property set=my_value" },
        }

        _G.c_plan = assert(proxy_wasm.new(filters))
        assert(proxy_wasm.load(_G.c_plan))
    }
--- config
    location /t {
        rewrite_by_lua_block {
            local proxy_wasm = require "resty.wasmx.proxy_wasm"

            local function setter(key, value)
                print("in setter")
                return true, "my_value"
            end

            assert(proxy_wasm.attach(_G.c_plan))
            assert(proxy_wasm.set_host_properties_handlers(nil, setter))
        }

        content_by_lua_block {
            ngx.say("ok")
        }
    }
--- response_body
ok
--- error_log eval
qr/rewrite_by_lua.*?: in setter/
--- no_error_log
[error]
[crit]



=== TEST 11: host properties setter - setter cannot yield
Produces a wasm host trap.
--- wasm_modules: hostcalls
--- http_config
    init_worker_by_lua_block {
        local proxy_wasm = require "resty.wasmx.proxy_wasm"
        local filters = {
            { name = "hostcalls", config="on=response_body " ..
                                         "test=/t/set_property " ..
                                         "name=wasmx.my_property set=my_value" },
        }

        _G.c_plan = assert(proxy_wasm.new(filters))
        assert(proxy_wasm.load(_G.c_plan))
    }
--- config
    location /t {
        rewrite_by_lua_block {
            local proxy_wasm = require "resty.wasmx.proxy_wasm"

            local function setter(key, value)
                ngx.sleep(1000)
                return true, "my_value"
            end

            assert(proxy_wasm.attach(_G.c_plan))
            assert(proxy_wasm.set_host_properties_handlers(nil, setter))
        }

        content_by_lua_block {
            ngx.say("ok")
        }
    }
--- response_body
ok
--- error_log eval
qr/host trap \(internal error\): could not set \"wasmx\.my_property\": error in property setter: attempt to yield across C-call boundary/
--- no_error_log
[crit]
[emerg]
