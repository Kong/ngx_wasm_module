# vim:set ft= ts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm::Lua;

skip_no_openresty();

plan_tests(5);
run_tests();

__DATA__

=== TEST 1: host properties getter - sanity in rewrite_by_lua
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

            local function getter(key)
                return true, "my_value"
            end

            assert(proxy_wasm.attach(_G.c_plan))
            assert(proxy_wasm.set_host_properties_handlers(getter, nil))
            assert(proxy_wasm.start())
            ngx.say("ok")
        }
    }
--- response_body
ok
--- grep_error_log eval: qr/wasmx.\w+_property: \w+/
--- grep_error_log_out
wasmx.my_property: my_value
wasmx.my_property: my_value
--- no_error_log
[error]
[crit]



=== TEST 2: host properties getter - sanity in access_by_lua
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

            local function getter(key)
                return true, "my_value"
            end

            assert(proxy_wasm.attach(_G.c_plan))
            assert(proxy_wasm.set_host_properties_handlers(getter, nil))
            assert(proxy_wasm.start())
            ngx.say("ok")
        }
    }
--- response_body
ok
--- grep_error_log eval: qr/wasmx.\w+_property: \w+/
--- grep_error_log_out
wasmx.my_property: my_value
wasmx.my_property: my_value
--- no_error_log
[error]
[crit]



=== TEST 3: host properties getter - sanity in header_filter_by_lua
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
            assert(proxy_wasm.start())
            ngx.say("ok")
        }

        header_filter_by_lua_block {
            local proxy_wasm = require "resty.wasmx.proxy_wasm"

            local function getter(key)
                return true, "my_value"
            end

            assert(proxy_wasm.set_host_properties_handlers(getter, nil))
        }
    }
--- response_body
ok
--- grep_error_log eval: qr/wasmx.\w+_property: \w+/
--- grep_error_log_out
wasmx.my_property: my_value
wasmx.my_property: my_value
--- no_error_log
[error]
[crit]



=== TEST 4: host properties getter - sanity in body_filter_by_lua
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
            assert(proxy_wasm.start())
            ngx.say("ok")
        }

        body_filter_by_lua_block {
            local proxy_wasm = require "resty.wasmx.proxy_wasm"

            local function getter(key)
                return true, "my_value"
            end

            if not _G.set then
                assert(proxy_wasm.set_host_properties_handlers(getter, nil))
                _G.set = true
            end
        }
    }
--- response_body
ok
--- grep_error_log eval: qr/wasmx.\w+_property: \w+/
--- grep_error_log_out
wasmx.my_property: my_value
wasmx.my_property: my_value
--- no_error_log
[error]
[crit]



=== TEST 5: host properties getter - sanity in log_by_lua
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
            assert(proxy_wasm.start())
            ngx.say("ok")
        }

        log_by_lua_block {
            local proxy_wasm = require "resty.wasmx.proxy_wasm"

            local function getter(key)
                return true, "my_value"
            end

            assert(proxy_wasm.set_host_properties_handlers(getter, nil))
        }
    }
--- response_body
ok
--- grep_error_log eval: qr/wasmx.\w+_property: \w+/
--- grep_error_log_out
wasmx.my_property: my_value
--- no_error_log
[error]
[crit]



=== TEST 6: host properties getter - getter returning false and an error
--- wasm_modules: hostcalls
--- http_config
    init_worker_by_lua_block {
        local proxy_wasm = require "resty.wasmx.proxy_wasm"
        local filters = {
            { name = "hostcalls", config="on=response_body " ..
                                         "test=/t/log/property " ..
                                         "name=wasmx.some_property" },
        }

        _G.c_plan = assert(proxy_wasm.new(filters))
        assert(proxy_wasm.load(_G.c_plan))
    }
--- config
    location /t {
        access_by_lua_block {
            local proxy_wasm = require "resty.wasmx.proxy_wasm"

            local function getter(key)
                return false, "some error"
            end

            assert(proxy_wasm.attach(_G.c_plan))
            assert(proxy_wasm.set_host_properties_handlers(getter, nil))
            assert(proxy_wasm.start())
            ngx.say("ok")
        }
    }
--- response_body
ok
--- error_log eval
qr/host trap \(internal error\): could not get \"wasmx.some_property\": some error/
--- no_error_log
[crit]
[emerg]]



=== TEST 7: host properties getter - getter returning false produces "property not found"
--- wasm_modules: hostcalls
--- http_config
    init_worker_by_lua_block {
        local proxy_wasm = require "resty.wasmx.proxy_wasm"
        local filters = {
            { name = "hostcalls", config="on=response_body " ..
                                         "test=/t/log/property " ..
                                         "name=wasmx.missing_property" },
        }

        _G.c_plan = assert(proxy_wasm.new(filters))
        assert(proxy_wasm.load(_G.c_plan))
    }
--- config
    location /t {
        access_by_lua_block {
            local proxy_wasm = require "resty.wasmx.proxy_wasm"

            local function getter(key)
                if key == "wasmx.my_property" then
                    return true, "my_value"
                end

                return false
            end

            assert(proxy_wasm.attach(_G.c_plan))
            assert(proxy_wasm.set_host_properties_handlers(getter, nil))
            assert(proxy_wasm.start())
            ngx.say("ok")
        }
    }
--- response_body
ok
--- grep_error_log eval: qr/.*?wasmx.\w+_property/
--- grep_error_log_out eval
qr/property not found: wasmx.missing_property/
--- no_error_log
[error]
[crit]



=== TEST 8: host properties getter - getter returning nil produces "property not found"
--- wasm_modules: hostcalls
--- http_config
    init_worker_by_lua_block {
        local proxy_wasm = require "resty.wasmx.proxy_wasm"
        local filters = {
            { name = "hostcalls", config="on=response_body " ..
                                         "test=/t/log/property " ..
                                         "name=wasmx.missing_property" },
        }

        _G.c_plan = assert(proxy_wasm.new(filters))
        assert(proxy_wasm.load(_G.c_plan))
    }
--- config
    location /t {
        access_by_lua_block {
            local proxy_wasm = require "resty.wasmx.proxy_wasm"

            local function getter(key)
                return nil
            end

            assert(proxy_wasm.attach(_G.c_plan))
            assert(proxy_wasm.set_host_properties_handlers(getter, nil))
            assert(proxy_wasm.start())
            ngx.say("ok")
        }
    }
--- response_body
ok
--- grep_error_log eval: qr/.*?wasmx.\w+_property/
--- grep_error_log_out eval
qr/property not found: wasmx.missing_property/
--- no_error_log
[error]
[crit]



=== TEST 9: host properties getter - getter returning true
With and without 3rd return value (caches result)
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

            local counter = 0

            local function getter(key)
                counter = counter + 1

                print("getting ", key, " counter: ", counter)

                if key == "wasmx.dyn_property" then
                    return true, tostring(counter)

                elseif key == "wasmx.const_property" then
                    return true, tostring(counter), true

                elseif key == "wasmx.nil_dyn_property" then
                    return true, nil

                elseif key == "wasmx.nil_const_property" then
                    return true, nil, true

                elseif key == "wasmx.empty_dyn_property" then
                    return true, ""

                elseif key == "wasmx.empty_const_property" then
                    return true, "", true
                end
            end

            assert(proxy_wasm.attach(_G.c_plan))
            assert(proxy_wasm.set_host_properties_handlers(getter, nil))
            assert(proxy_wasm.start())
            ngx.say("ok")
        }

        log_by_lua_block {
            local proxy_wasm = require "resty.wasmx.proxy_wasm"

            local function log_property(k)
                local v = proxy_wasm.get_property(k)
                print(k, ": ", type(v), " \"", v, "\"")
            end

            -- four get_property operations,
            -- but it will hit the getter three times
            -- because the second call to get wasmx.const_property
            -- hits the cache
            log_property("wasmx.const_property") -- returns 1
            log_property("wasmx.dyn_property") -- returns 2
            log_property("wasmx.const_property") -- returns 1
            log_property("wasmx.dyn_property") -- returns 3

            log_property("wasmx.nil_const_property") -- returns nil
            log_property("wasmx.nil_dyn_property") -- returns nil
            log_property("wasmx.nil_const_property") -- returns nil
            log_property("wasmx.nil_dyn_property") -- returns nil

            log_property("wasmx.empty_const_property") -- returns ""
            log_property("wasmx.empty_dyn_property") -- returns ""
            log_property("wasmx.empty_const_property") -- returns ""
            log_property("wasmx.empty_dyn_property") -- returns ""
        }
    }
--- response_body
ok
--- grep_error_log eval: qr/(getting )?wasmx\.\w+_property.*?(?= while)/
--- grep_error_log_out
getting wasmx.const_property counter: 1
wasmx.const_property: string "1"
getting wasmx.dyn_property counter: 2
wasmx.dyn_property: string "2"
wasmx.const_property: string "1"
getting wasmx.dyn_property counter: 3
wasmx.dyn_property: string "3"
getting wasmx.nil_const_property counter: 4
wasmx.nil_const_property: nil "nil"
getting wasmx.nil_dyn_property counter: 5
wasmx.nil_dyn_property: nil "nil"
wasmx.nil_const_property: nil "nil"
getting wasmx.nil_dyn_property counter: 6
wasmx.nil_dyn_property: nil "nil"
getting wasmx.empty_const_property counter: 7
wasmx.empty_const_property: string ""
getting wasmx.empty_dyn_property counter: 8
wasmx.empty_dyn_property: string ""
wasmx.empty_const_property: string ""
getting wasmx.empty_dyn_property counter: 9
wasmx.empty_dyn_property: string ""
--- no_error_log
[error]
[crit]



=== TEST 10: host properties getter - errors are caught when accessed through filters
Produces a wasm host trap.
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

            local function getter(key)
                error("crash!")
            end

            assert(proxy_wasm.attach(_G.c_plan))
            assert(proxy_wasm.set_host_properties_handlers(getter, nil))
            assert(proxy_wasm.start())
            ngx.say("ok")
        }
    }
--- response_body
ok
--- error_log eval
qr/host trap \(internal error\): could not get \"wasmx\.my_property\": error in property getter: rewrite_by_lua.*?: crash\!/
--- no_error_log
[crit]
[emerg]



=== TEST 11: host properties getter - errors are caught when accessed through proxy_wasm.get_property()
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

            local function getter(key, value)
                error("crash!")
            end

            assert(proxy_wasm.attach(_G.c_plan))
            assert(proxy_wasm.set_host_properties_handlers(getter, nil))

            local ok, err = proxy_wasm.get_property("wasmx.my_property")
            if not ok then
                ngx.log(ngx.ERR, err)
            end

            assert(proxy_wasm.start())
            ngx.say("ok")
        }
    }
--- response_body
ok
--- error_log eval
qr/\[error\] .*? error in property getter: rewrite_by_lua.*?: crash\!/
--- no_error_log
[crit]
[emerg]



=== TEST 12: host properties getter - getter can ngx.log()
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

            local function getter(key)
                print("in getter")
                return true, "foo"
            end

            assert(proxy_wasm.attach(_G.c_plan))
            assert(proxy_wasm.set_host_properties_handlers(getter, nil))
            assert(proxy_wasm.start())
            ngx.say("ok")
        }
    }
--- response_body
ok
--- error_log eval
qr/rewrite_by_lua.*?: in getter/
--- no_error_log
[error]
[crit]



=== TEST 13: host properties getter - getter cannot yield
Produces a wasm host trap.
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

            local function getter(key)
                ngx.sleep(1000)
                return true, "foo"
            end

            assert(proxy_wasm.attach(_G.c_plan))
            assert(proxy_wasm.set_host_properties_handlers(getter, nil))
            assert(proxy_wasm.start())
            ngx.say("ok")
        }
    }
--- response_body
ok
--- error_log eval
qr/host trap \(internal error\): could not get \"wasmx\.my_property\": error in property getter: attempt to yield across C-call boundary/
--- no_error_log
[crit]
[emerg]
