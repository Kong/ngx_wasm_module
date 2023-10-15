# vim:set ft= ts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm::Lua;

skip_no_openresty();

plan tests => repeat_each() * (blocks() * 5);

run_tests();

__DATA__

=== TEST 1: set_property_getter() - setting host properties getter fails on init_worker_by_lua
--- wasm_modules: on_phases
--- http_config
    init_worker_by_lua_block {
        local proxy_wasm = require "resty.wasmx.proxy_wasm"
        local filters = {
            { name = "on_phases" },
        }

        _G.c_plan = assert(proxy_wasm.new(filters))
        assert(proxy_wasm.load(_G.c_plan))

        local function getter(key)
            if key == "my_key" then
                return true, "my_value"
            end
            return false
        end

        assert(proxy_wasm.set_property_getter(getter))
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
    qr/\[error\] .*? cannot set host properties getter outside of a request/,
    qr/\[error\] .*? init_worker_by_lua(\(nginx\.conf:\d+\))?:\d+: could not set property getter/,
]
--- no_error_log
[crit]



=== TEST 2: set_property_getter() - setting host properties getter works on rewrite_by_lua
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

            local function getter(key)
                if key == "wasmx.my_property" then
                    return true, "my_value"
                end
                return false
            end

            assert(proxy_wasm.set_property_getter(getter))

            assert(proxy_wasm.start())
            ngx.say("ok")
        }
    }
--- response_body
ok
--- error_log eval
[
    qr/\[info\] .*? \[hostcalls\] on_response_body/,
    qr/\[info\] .*? wasmx.my_property: my_value/,
]
--- no_error_log
[error]



=== TEST 3: set_property_getter() - setting host properties getter works on access_by_lua
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

            local function getter(key)
                if key == "wasmx.my_property" then
                    return true, "my_value"
                end
                return false
            end

            assert(proxy_wasm.set_property_getter(getter))

            assert(proxy_wasm.start())
            ngx.say("ok")
        }
    }
--- response_body
ok
--- error_log eval
[
    qr/\[info\] .*? \[hostcalls\] on_response_body/,
    qr/\[info\] .*? wasmx.my_property: my_value/,
]
--- no_error_log
[error]



=== TEST 4: set_property_getter() - setting host properties getter works on header_filter_by_lua
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

            local function getter(key)
                if key == "wasmx.my_property" then
                    return true, "my_value"
                end
                return false
            end

            assert(proxy_wasm.set_property_getter(getter))
        }
    }
--- response_body
ok
--- error_log eval
[
    qr/\[info\] .*? \[hostcalls\] on_response_body/,
    qr/\[info\] .*? wasmx.my_property: my_value/,
]
--- no_error_log
[error]



=== TEST 5: set_property_getter() - setting host properties getter works on body_filter_by_lua
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

            local function getter(key)
                if key == "wasmx.my_property" then
                    return true, "my_value"
                end
                return false
            end

            assert(proxy_wasm.set_property_getter(getter))
        }
    }
--- response_body
ok
--- error_log eval
[
    qr/\[info\] .*? \[hostcalls\] on_response_body/,
    qr/\[info\] .*? wasmx.my_property: my_value/,
]
--- no_error_log
[error]



=== TEST 6: set_property_getter() - setting host properties getter works on log_by_lua
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

            local function getter(key)
                if key == "wasmx.my_property" then
                    return true, "my_value"
                end
                return false
            end

            assert(proxy_wasm.set_property_getter(getter))
        }
    }
--- response_body
ok
--- error_log eval
[
    qr/\[info\] .*? \[hostcalls\] on_log/,
    qr/\[info\] .*? wasmx.my_property: my_value/,
]
--- no_error_log
[error]



=== TEST 7: set_property_getter() - setting property getter at startup doesn't reset filter list
--- wasm_modules: hostcalls
--- http_config
    init_worker_by_lua_block {
        local proxy_wasm = require "resty.wasmx.proxy_wasm"
        local filters = {
            { name = "hostcalls", config = "on=log " ..
                                           "test=/t/log/property " ..
                                           "name=wasmx.startup_property" },
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

            local function getter(key)
                if key == "wasmx.startup_property" then
                    return true, "foo"
                end
                return false
            end

            assert(proxy_wasm.set_property_getter(getter))

            assert(proxy_wasm.start())

            ngx.say("ok")
        }
    }
--- response_body
ok
--- error_log eval
[
    qr/\[info\] .*? \[hostcalls\] on_log/,
    qr/\[info\] .*?hostcalls.*? wasmx.startup_property: foo/,
]
--- no_error_log
[error]



=== TEST 8: set_property_getter() - getter returning false produces property not found
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
                              name=wasmx.missing_property';

        access_by_lua_block {
            local proxy_wasm = require "resty.wasmx.proxy_wasm"
            assert(proxy_wasm.attach(_G.c_plan))

            local function getter(key)
                if key == "wasmx.my_property" then
                    return true, "my_value"
                end
                return false
            end

            assert(proxy_wasm.set_property_getter(getter))

            assert(proxy_wasm.start())
            ngx.say("ok")
        }
    }
--- response_body
ok
--- error_log eval
[
    qr/\[info\] .*? \[hostcalls\] on_response_body/,
    qr/\[info\] .*? property not found: wasmx.missing_property/,
]
--- no_error_log
[error]



=== TEST 9: set_property_getter() - getter returning nil produces property not found
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
                              name=wasmx.missing_property';

        access_by_lua_block {
            local proxy_wasm = require "resty.wasmx.proxy_wasm"
            assert(proxy_wasm.attach(_G.c_plan))

            local function getter(key)
                if key == "wasmx.my_property" then
                    return true, "my_value"
                end
            end

            assert(proxy_wasm.set_property_getter(getter))

            assert(proxy_wasm.start())
            ngx.say("ok")
        }
    }
--- response_body
ok
--- error_log eval
[
    qr/\[info\] .*? \[hostcalls\] on_response_body/,
    qr/\[info\] .*? property not found: wasmx.missing_property/,
]
--- no_error_log
[error]



=== TEST 10: set_property_getter() - getter returning true on 3rd value caches result
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

            local function getter(key)
                counter = counter + 1

                ngx.log(ngx.INFO, "getting ", key, ", counter: ",
                        tostring(counter))

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
--- grep_error_log eval: qr/.*wasmx.*property.*/
--- grep_error_log_out eval
qr/^[^\n]*?\[info\] [^\n]*? getting wasmx.const_property, counter: 1[^\n]*?
[^\n]*?\[info\] [^\n]*? wasmx.const_property: string "1"[^\n]*?
[^\n]*?\[info\] [^\n]*? getting wasmx.dyn_property, counter: 2[^\n]*?
[^\n]*?\[info\] [^\n]*? wasmx.dyn_property: string "2"[^\n]*?
[^\n]*?\[info\] [^\n]*? wasmx.const_property: string "1"[^\n]*?
[^\n]*?\[info\] [^\n]*? getting wasmx.dyn_property, counter: 3[^\n]*?
[^\n]*?\[info\] [^\n]*? wasmx.dyn_property: string "3"[^\n]*?
[^\n]*?\[info\] [^\n]*? getting wasmx.nil_const_property, counter: 4[^\n]*?
[^\n]*?\[info\] [^\n]*? wasmx.nil_const_property: nil "nil"[^\n]*?
[^\n]*?\[info\] [^\n]*? getting wasmx.nil_dyn_property, counter: 5[^\n]*?
[^\n]*?\[info\] [^\n]*? wasmx.nil_dyn_property: nil "nil"[^\n]*?
[^\n]*?\[info\] [^\n]*? wasmx.nil_const_property: nil "nil"[^\n]*?
[^\n]*?\[info\] [^\n]*? getting wasmx.nil_dyn_property, counter: 6[^\n]*?
[^\n]*?\[info\] [^\n]*? wasmx.nil_dyn_property: nil "nil"[^\n]*?
[^\n]*?\[info\] [^\n]*? getting wasmx.empty_const_property, counter: 7[^\n]*?
[^\n]*?\[info\] [^\n]*? wasmx.empty_const_property: string ""[^\n]*?
[^\n]*?\[info\] [^\n]*? getting wasmx.empty_dyn_property, counter: 8[^\n]*?
[^\n]*?\[info\] [^\n]*? wasmx.empty_dyn_property: string ""[^\n]*?
[^\n]*?\[info\] [^\n]*? wasmx.empty_const_property: string ""[^\n]*?
[^\n]*?\[info\] [^\n]*? getting wasmx.empty_dyn_property, counter: 9[^\n]*?
[^\n]*?\[info\] [^\n]*? wasmx.empty_dyn_property: string ""[^\n]*?$/
--- no_error_log
[error]
[crit]



=== TEST 11: set_property_getter() - errors in getter are caught by pcall in Lua library and don't crash the worker
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

            local function getter(key)
                error("crash!")
            end

            assert(proxy_wasm.set_property_getter(getter))

            assert(proxy_wasm.start())
            ngx.say("ok")
        }
    }
--- response_body
ok
--- error_log eval
[
    qr/\[info\] .*? \[hostcalls\] on_response_body/,
    qr/\[error\] .*? error getting property from Lua: rewrite_by_lua\(nginx.conf:[0-9]+\):[0-9]+: crash\!/,
]
--- no_error_log
[emerg]
