# vim:set ft= ts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm::Lua;

skip_no_openresty();

plan_tests(5);
run_tests();

__DATA__

=== TEST 1: set_property() - setting host properties fails on init_worker_by_lua
--- wasm_modules: on_phases
--- http_config
    init_worker_by_lua_block {
        local proxy_wasm = require "resty.wasmx.proxy_wasm"
        local filters = {
            { name = "on_phases" },
        }

        local c_plan = assert(proxy_wasm.new(filters))
        assert(proxy_wasm.load(c_plan))

        local ok, err = proxy_wasm.set_property("wasmx.my_property", "my_value")
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
    qr/\[error\] .*? cannot set host properties outside of a request/,
    qr/\[error\] .*? init_worker_by_lua(\(nginx\.conf:\d+\))?:\d+: unknown error/,
]
--- no_error_log
[crit]



=== TEST 2: set_property() - setting host properties works on rewrite_by_lua
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
    qr/\[info\] .*? wasmx.my_property: my_value/,
]
--- no_error_log
[error]



=== TEST 3: set_property() - setting host properties works on access_by_lua
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

        proxy_wasm hostcalls 'on=response_body \
                              test=/t/log/property \
                              name=wasmx.my_property';

        access_by_lua_block {
            local proxy_wasm = require "resty.wasmx.proxy_wasm"
            assert(proxy_wasm.attach(_G.c_plan))

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
    qr/\[info\] .*? wasmx.my_property: my_value/,
]
--- no_error_log
[error]



=== TEST 4: set_property() - setting host properties works on header_filter_by_lua
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

        proxy_wasm hostcalls 'on=response_body \
                              test=/t/log/property \
                              name=wasmx.my_property';

        access_by_lua_block {
            local proxy_wasm = require "resty.wasmx.proxy_wasm"
            assert(proxy_wasm.attach(_G.c_plan))

            assert(proxy_wasm.start())
            ngx.say("ok")
        }

        header_filter_by_lua_block {
            local proxy_wasm = require "resty.wasmx.proxy_wasm"
            assert(proxy_wasm.set_property("wasmx.my_property", "my_value"))
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



=== TEST 5: set_property() - setting host properties works on body_filter_by_lua
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

        proxy_wasm hostcalls 'on=response_body \
                              test=/t/log/property \
                              name=wasmx.my_property';

        access_by_lua_block {
            local proxy_wasm = require "resty.wasmx.proxy_wasm"
            assert(proxy_wasm.attach(_G.c_plan))

            assert(proxy_wasm.start())
            ngx.say("ok")
        }

        body_filter_by_lua_block {
            local proxy_wasm = require "resty.wasmx.proxy_wasm"
            assert(proxy_wasm.set_property("wasmx.my_property", "my_value"))
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



=== TEST 6: set_property() - setting host properties works on log_by_lua
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
            assert(proxy_wasm.set_property("wasmx.my_property", "my_value"))
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



=== TEST 7: set_property() - regression test: setting properties at startup doesn't reset filter list
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

            assert(proxy_wasm.set_property("wasmx.startup_property", "foo"))

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
