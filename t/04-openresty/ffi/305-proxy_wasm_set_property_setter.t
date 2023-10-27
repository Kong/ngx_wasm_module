# vim:set ft= ts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm::Lua;

skip_no_openresty();

plan tests => repeat_each() * (blocks() * 5);

run_tests();

__DATA__

=== TEST 1: set_property_setter() - setting host properties setter fails on init_worker_by_lua
--- wasm_modules: on_phases
--- http_config
    init_worker_by_lua_block {
        local proxy_wasm = require "resty.wasmx.proxy_wasm"
        local filters = {
            { name = "on_phases" },
        }

        _G.c_plan = assert(proxy_wasm.new(filters))
        assert(proxy_wasm.load(_G.c_plan))

        local tbl = {}

        local function setter(key, value)
            tbl[key] = value
            return true
        end

        assert(proxy_wasm.set_property_setter(setter))
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
    qr/\[error\] .*? cannot set host properties setter outside of a request/,
    qr/\[error\] .*? init_worker_by_lua(\(nginx\.conf:\d+\))?:\d+: could not set property setter/,
]
--- no_error_log
[crit]



=== TEST 2: set_property_setter() - setting host properties setter works on rewrite_by_lua
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
        proxy_wasm hostcalls 'on=response_body \
                              test=/t/log/property \
                              name=wasmx.my_property';

        rewrite_by_lua_block {
            local proxy_wasm = require "resty.wasmx.proxy_wasm"
            assert(proxy_wasm.attach(_G.c_plan))

            local tbl = {}

            local function setter(key, value)
                local new_value = value:upper()
                tbl[key] = new_value
                return true, new_value
            end

            assert(proxy_wasm.set_property_setter(setter))

            assert(proxy_wasm.set_property("wasmx.my_property", "my_value"))

            assert(proxy_wasm.start())
            ngx.say("ok")
        }
    }
--- response_body
ok
--- error_log eval
[
    qr/\[info\] .*? \[hostcalls\] on_response_body/,
    qr/\[info\] .*? wasmx.my_property: MY_VALUE/,
]
--- no_error_log
[error]



=== TEST 3: set_property_setter() - setting host properties setter works on access_by_lua
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
        proxy_wasm hostcalls 'on=response_body \
                              test=/t/log/property \
                              name=wasmx.my_property';

        access_by_lua_block {
            local proxy_wasm = require "resty.wasmx.proxy_wasm"
            assert(proxy_wasm.attach(_G.c_plan))

            local tbl = {}

            local function setter(key, value)
                local new_value = value:upper()
                tbl[key] = new_value
                return true, new_value
            end

            assert(proxy_wasm.set_property_setter(setter))

            assert(proxy_wasm.set_property("wasmx.my_property", "my_value"))

            assert(proxy_wasm.start())
            ngx.say("ok")
        }
    }
--- response_body
ok
--- error_log eval
[
    qr/\[info\] .*? \[hostcalls\] on_response_body/,
    qr/\[info\] .*? wasmx.my_property: MY_VALUE/,
]
--- no_error_log
[error]



=== TEST 4: set_property_setter() - setting host properties setter works on header_filter_by_lua
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
        proxy_wasm hostcalls 'on=response_body \
                              test=/t/log/property \
                              name=wasmx.my_property';

        rewrite_by_lua_block {
            local proxy_wasm = require "resty.wasmx.proxy_wasm"
            assert(proxy_wasm.attach(_G.c_plan))

            assert(proxy_wasm.start())
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

            assert(proxy_wasm.set_property_setter(setter))

            assert(proxy_wasm.set_property("wasmx.my_property", "my_value"))
        }
    }
--- response_body
ok
--- error_log eval
[
    qr/\[info\] .*? \[hostcalls\] on_response_body/,
    qr/\[info\] .*? wasmx.my_property: MY_VALUE/,
]
--- no_error_log
[error]



=== TEST 5: set_property_setter() - setting host properties setter works on body_filter_by_lua
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
        proxy_wasm hostcalls 'on=response_body \
                              test=/t/log/property \
                              name=wasmx.my_property';

        rewrite_by_lua_block {
            local proxy_wasm = require "resty.wasmx.proxy_wasm"
            assert(proxy_wasm.attach(_G.c_plan))

            assert(proxy_wasm.start())
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

            assert(proxy_wasm.set_property_setter(setter))

            assert(proxy_wasm.set_property("wasmx.my_property", "my_value"))
        }
    }
--- response_body
ok
--- error_log eval
[
    qr/\[info\] .*? \[hostcalls\] on_response_body/,
    qr/\[info\] .*? wasmx.my_property: MY_VALUE/,
]
--- no_error_log
[error]



=== TEST 6: set_property_setter() - setting host properties setter works on log_by_lua
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
        proxy_wasm_request_headers_in_access on;

        proxy_wasm hostcalls 'on=log \
                              test=/t/log/property \
                              name=wasmx.my_property';

        access_by_lua_block {
            local proxy_wasm = require "resty.wasmx.proxy_wasm"
            assert(proxy_wasm.attach(_G.c_plan))

            assert(proxy_wasm.start())
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

            assert(proxy_wasm.set_property_setter(setter))

            assert(proxy_wasm.set_property("wasmx.my_property", "my_value"))
        }
    }
--- response_body
ok
--- error_log eval
[
    qr/\[info\] .*? \[hostcalls\] on_log/,
    qr/\[info\] .*? wasmx.my_property: MY_VALUE/,
]
--- no_error_log
[error]



=== TEST 7: set_property_setter() - setting property setter at startup doesn't reset filter list
--- wasm_modules: hostcalls
--- http_config
    init_worker_by_lua_block {
        local proxy_wasm = require "resty.wasmx.proxy_wasm"
        local filters = {
            { name = "hostcalls", config = "on=log " ..
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

            local function setter(key, value)
                if key == "wasmx.my_property" then
                    return true, "foo"
                end
                return false
            end

            assert(proxy_wasm.set_property_setter(setter))

            assert(proxy_wasm.start())

            ngx.say("ok")
        }

        body_filter_by_lua_block {
            local proxy_wasm = require "resty.wasmx.proxy_wasm"

            assert(proxy_wasm.set_property("wasmx.my_property", "bar"))
        }
    }
--- response_body
ok
--- error_log eval
[
    qr/\[info\] .*? \[hostcalls\] on_log/,
    qr/\[info\] .*?hostcalls.*? wasmx.my_property: foo/,
]
--- no_error_log
[error]



=== TEST 8: set_property_setter() - setter returning falsy logs custom message or "unknown error"
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
    proxy_wasm hostcalls 'on=log \
                          test=/t/log/property \
                          name=wasmx.fail1_property';

    location /t {
        access_by_lua_block {
            local proxy_wasm = require "resty.wasmx.proxy_wasm"
            assert(proxy_wasm.attach(_G.c_plan))

            local function setter(key)
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

            assert(proxy_wasm.set_property_setter(setter))

            local msg = "result:"
            for i = 1, 4 do
                local key = "wasmx.fail" .. i .. "_property"
                local ok, err = proxy_wasm.set_property(key, "wat")
                if not ok then
                    msg = msg .. "\n" .. err
                end
            end

            assert(proxy_wasm.start())
            ngx.say(msg)
        }
    }
--- response_body
result:
unknown error
unknown error
unknown error
unknown error
--- grep_error_log eval: qr/.*property.*/
--- grep_error_log_out eval
qr/^[^\n]*?\[error\] [^\n]*? error in property handler: first wasmx property custom error[^\n]*?
[^\n]*?\[error\] [^\n]*? error in property handler: second wasmx property custom error[^\n]*?
[^\n]*?\[error\] [^\n]*? error in property handler: unknown error[^\n]*?
[^\n]*?\[error\] [^\n]*? error in property handler: unknown error[^\n]*?
[^\n]*?\[info\] [^\n]*? property not found: wasmx.fail1_property[^\n]*?$/
--- no_error_log
[crit]
[alert]



=== TEST 9: set_property_setter() - setter returning true on 3rd value caches result
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
            assert(proxy_wasm.attach(_G.c_plan))

            local counter = 0

            local tbl = {}

            local function setter(key, value)
                counter = counter + 1

                if value and #value > 0 then
                    value = value:upper() .. " " .. tostring(counter)
                end

                tbl[key] = value

                ngx.log(ngx.INFO, "setting ", key, " to ", value, ", counter: " .. counter)

                return true, value, (key:match("const") and true or false)
            end

            assert(proxy_wasm.set_property_setter(setter))

            local function getter(key)
                counter = counter + 1

                ngx.log(ngx.INFO, "getting ", key, " via getter, counter: ", counter)

                local value = tbl[key]

                if value and #value > 0 then
                    value = value .. " " .. tostring(counter)
                end

                return true, value
            end

            assert(proxy_wasm.set_property_getter(getter))

            assert(proxy_wasm.start())
            ngx.say("ok")
        }

        log_by_lua_block {
            local proxy_wasm = require "resty.wasmx.proxy_wasm"

            local function log_property(k)
                local v = proxy_wasm.get_property(k)
                ngx.log(ngx.INFO, k, ": ", type(v), " \"", v, "\"")
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
--- grep_error_log eval: qr/.*wasmx.*property.*/
--- grep_error_log_out eval
qr/^[^\n]*?\[info\] [^\n]*? setting wasmx.const_property to HELLO 1, counter: 1[^\n]*?
[^\n]*?\[info\] [^\n]*? wasmx.const_property: string "HELLO 1"[^\n]*?
[^\n]*?\[info\] [^\n]*? setting wasmx.dyn_property to HI 2, counter: 2[^\n]*?
[^\n]*?\[info\] [^\n]*? getting wasmx.dyn_property via getter, counter: 3[^\n]*?
[^\n]*?\[info\] [^\n]*? wasmx.dyn_property: string "HI 2 3"[^\n]*?
[^\n]*?\[info\] [^\n]*? wasmx.const_property: string "HELLO 1"[^\n]*?
[^\n]*?\[info\] [^\n]*? getting wasmx.dyn_property via getter, counter: 4[^\n]*?
[^\n]*?\[info\] [^\n]*? wasmx.dyn_property: string "HI 2 4"[^\n]*?
[^\n]*?\[info\] [^\n]*? setting wasmx.nil_const_property to nil, counter: 5[^\n]*?
[^\n]*?\[info\] [^\n]*? wasmx.nil_const_property: nil "nil"[^\n]*?
[^\n]*?\[info\] [^\n]*? setting wasmx.nil_dyn_property to nil, counter: 6[^\n]*?
[^\n]*?\[info\] [^\n]*? getting wasmx.nil_dyn_property via getter, counter: 7[^\n]*?
[^\n]*?\[info\] [^\n]*? wasmx.nil_dyn_property: nil "nil"[^\n]*?
[^\n]*?\[info\] [^\n]*? wasmx.nil_const_property: nil "nil"[^\n]*?
[^\n]*?\[info\] [^\n]*? getting wasmx.nil_dyn_property via getter, counter: 8[^\n]*?
[^\n]*?\[info\] [^\n]*? wasmx.nil_dyn_property: nil "nil"[^\n]*?
[^\n]*?\[info\] [^\n]*? setting wasmx.empty_const_property to , counter: 9[^\n]*?
[^\n]*?\[info\] [^\n]*? wasmx.empty_const_property: string ""[^\n]*?
[^\n]*?\[info\] [^\n]*? setting wasmx.empty_dyn_property to , counter: 10[^\n]*?
[^\n]*?\[info\] [^\n]*? getting wasmx.empty_dyn_property via getter, counter: 11[^\n]*?
[^\n]*?\[info\] [^\n]*? wasmx.empty_dyn_property: string ""[^\n]*?
[^\n]*?\[info\] [^\n]*? wasmx.empty_const_property: string ""[^\n]*?
[^\n]*?\[info\] [^\n]*? getting wasmx.empty_dyn_property via getter, counter: 12[^\n]*?
[^\n]*?\[info\] [^\n]*? wasmx.empty_dyn_property: string ""[^\n]*?$/
--- no_error_log
[error]
[crit]



=== TEST 10: set_property_setter() - errors in setter are caught by pcall in Lua library and don't crash the worker
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
        proxy_wasm hostcalls 'on=response_body \
                              test=/t/log/property \
                              name=wasmx.my_property';

        rewrite_by_lua_block {
            local proxy_wasm = require "resty.wasmx.proxy_wasm"
            assert(proxy_wasm.attach(_G.c_plan))

            local tbl = {}

            local function setter(key, value)
                error("crash!")
            end

            assert(proxy_wasm.set_property_setter(setter))

            local ok, err = proxy_wasm.set_property("wasmx.my_property", "my_value")
            assert((not ok) and err == "unknown error")

            assert(proxy_wasm.start())
            ngx.say("ok")
        }
    }
--- response_body
ok
--- error_log eval
[
    qr/\[info\] .*? \[hostcalls\] on_response_body/,
    qr/\[error\] .*? Lua error in property handler: rewrite_by_lua\(nginx.conf:[0-9]+\):[0-9]+: crash\!/,
]
--- no_error_log
[emerg]
