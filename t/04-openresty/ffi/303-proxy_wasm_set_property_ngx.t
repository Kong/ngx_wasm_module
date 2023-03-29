# vim:set ft= ts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm::Lua;

skip_no_openresty();

plan tests => repeat_each() * (blocks() * 5);

run_tests();

__DATA__

=== TEST 1: set_property() - setting ngx properties fails on init_worker_by_lua
--- wasm_modules: on_phases
--- http_config
    init_worker_by_lua_block {
        local proxy_wasm = require "resty.http.proxy_wasm"
        local filters = {
            { name = "on_phases" },
        }

        local c_plan = assert(proxy_wasm.new(filters))
        assert(proxy_wasm.load(c_plan))

        local ok, err = proxy_wasm.set_property("ngx.my_var", "123")
        if not ok then
            ngx.log(ngx.ERR, err)
            return
        end
    }
--- config
    set $my_var 456;

    location /t {
        rewrite_by_lua_block {
            ngx.say("ok")
        }
    }
--- response_body
ok
--- error_log eval
[
    qr/\[error\] .*? cannot set ngx properties outside of a request/,
    qr/\[error\] .*? init_worker_by_lua:\d+: unknown error/,
]
--- no_error_log
[crit]



=== TEST 2: set_property() - setting ngx properties works on rewrite_by_lua
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
    set $my_var 456;

    location /t {
        proxy_wasm hostcalls 'on=response_body \
                              test=/t/log/property \
                              name=ngx.my_var';

        rewrite_by_lua_block {
            local proxy_wasm = require "resty.http.proxy_wasm"
            assert(proxy_wasm.attach(_G.c_plan))

            assert(proxy_wasm.set_property("ngx.my_var", "123"))

            assert(proxy_wasm.start())
            ngx.say("ok")
        }
    }
--- response_body
ok
--- error_log eval
[
    qr/\[info\] .*? \[hostcalls\] on_response_body/,
    qr/\[info\] .*? ngx.my_var: 123/,
]
--- no_error_log
[error]



=== TEST 3: set_property() - setting ngx properties works on access_by_lua
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
    set $my_var 456;

    location /t {
        proxy_wasm_request_headers_in_access on;

        proxy_wasm hostcalls 'on=response_body \
                              test=/t/log/property \
                              name=ngx.my_var';

        access_by_lua_block {
            local proxy_wasm = require "resty.http.proxy_wasm"
            assert(proxy_wasm.attach(_G.c_plan))

            assert(proxy_wasm.set_property("ngx.my_var", "123"))

            assert(proxy_wasm.start())
            ngx.say("ok")
        }
    }
--- response_body
ok
--- error_log eval
[
    qr/\[info\] .*? \[hostcalls\] on_response_body/,
    qr/\[info\] .*? ngx.my_var: 123/,
]
--- no_error_log
[error]



=== TEST 4: set_property() - setting ngx properties works on header_filter_by_lua
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
    set $my_var 456;

    location /t {
        proxy_wasm_request_headers_in_access on;

        proxy_wasm hostcalls 'on=response_body \
                              test=/t/log/property \
                              name=ngx.my_var';

        access_by_lua_block {
            local proxy_wasm = require "resty.http.proxy_wasm"
            assert(proxy_wasm.attach(_G.c_plan))

            assert(proxy_wasm.start())
            ngx.say("ok")
        }

        header_filter_by_lua_block {
            local proxy_wasm = require "resty.http.proxy_wasm"
            assert(proxy_wasm.set_property("ngx.my_var", "123"))
        }
    }
--- response_body
ok
--- error_log eval
[
    qr/\[info\] .*? \[hostcalls\] on_response_body/,
    qr/\[info\] .*? ngx.my_var: 123/,
]
--- no_error_log
[error]



=== TEST 5: set_property() - setting ngx properties works on body_filter_by_lua
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
    set $my_var 456;

    location /t {
        proxy_wasm_request_headers_in_access on;

        proxy_wasm hostcalls 'on=response_body \
                              test=/t/log/property \
                              name=ngx.my_var';

        access_by_lua_block {
            local proxy_wasm = require "resty.http.proxy_wasm"
            assert(proxy_wasm.attach(_G.c_plan))

            assert(proxy_wasm.start())
            ngx.say("ok")
        }

        body_filter_by_lua_block {
            local proxy_wasm = require "resty.http.proxy_wasm"
            assert(proxy_wasm.set_property("ngx.my_var", "123"))
        }
    }
--- response_body
ok
--- error_log eval
[
    qr/\[info\] .*? \[hostcalls\] on_response_body/,
    qr/\[info\] .*? ngx.my_var: 123/,
]
--- no_error_log
[error]



=== TEST 6: set_property() - setting ngx properties works on log_by_lua
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
    set $my_var 456;

    location /t {
        proxy_wasm_request_headers_in_access on;

        proxy_wasm hostcalls 'on=log \
                              test=/t/log/property \
                              name=ngx.my_var';

        access_by_lua_block {
            local proxy_wasm = require "resty.http.proxy_wasm"
            assert(proxy_wasm.attach(_G.c_plan))

            assert(proxy_wasm.start())
            ngx.say("ok")
        }

        log_by_lua_block {
            local proxy_wasm = require "resty.http.proxy_wasm"
            assert(proxy_wasm.set_property("ngx.my_var", "123"))
        }
    }
--- response_body
ok
--- error_log eval
[
    qr/\[info\] .*? \[hostcalls\] on_log/,
    qr/\[info\] .*? ngx.my_var: 123/,
]
--- no_error_log
[error]



=== TEST 7: set_property() - without loading a plan
--- config
    location /t {
        access_by_lua_block {
            local proxy_wasm = require "resty.http.proxy_wasm"

            local prop, err = proxy_wasm.set_property("ngx.my_var", "123")
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
