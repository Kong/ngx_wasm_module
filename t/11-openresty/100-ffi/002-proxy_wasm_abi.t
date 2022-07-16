# vim:set ft= ts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm::Lua;

our $nginxV = $t::TestWasm::nginxV;

skip_no_openresty();

plan tests => repeat_each() * (blocks() * 4);

run_tests();

__DATA__

=== TEST 1: proxy_wasm new() - no VM
--- config
    location /t {
        access_by_lua_block {
            local proxy_wasm = require "resty.http.proxy_wasm"
            local filters = {}

            local pok, perr = pcall(proxy_wasm.new, filters)
            if not pok then
                return ngx.say(perr)
            end
        }
    }
--- response_body
no wasm vm
--- no_error_log
[error]
[crit]



=== TEST 2: proxy_wasm new() - bad filters
--- wasm_modules: on_phases
--- config
    location /t {
        access_by_lua_block {
            local proxy_wasm = require "resty.http.proxy_wasm"

            local pok, perr = pcall(proxy_wasm.new, nil)
            if not pok then
                return ngx.say(perr)
            end
        }
    }
--- response_body
filters must be a table
--- no_error_log
[error]
[crit]



=== TEST 3: proxy_wasm new() - sanity
--- wasm_modules: on_phases hostcalls
--- config
    location /t {
        access_by_lua_block {
            local proxy_wasm = require "resty.http.proxy_wasm"
            local filters = {
                { name = "on_phases", config = "a" },
                { name = "hostcalls", config = "b" },
            }

            local c_ops, err = proxy_wasm.new(filters)
            if not c_ops then
                return ngx.say(err)
            end

            ngx.say("ops: ", tostring(c_ops))
        }
    }
--- response_body_like: ops: cdata<struct ngx_wasm_ops_engine_t \*>: 0x[0-9a-f]+
--- no_error_log
[error]
[crit]



=== TEST 4: proxy_wasm new() - request engine is GC'ed
--- skip_eval: 4: $::nginxV !~ m/debug/
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: on_phases hostcalls
--- config
    location /t {
        access_by_lua_block {
            local proxy_wasm = require "resty.http.proxy_wasm"
            local filters = {
                { name = "on_phases", config = "a" },
                { name = "hostcalls", config = "b" },
            }

            local c_ops, err = proxy_wasm.new(filters)
            if not c_ops then
                return ngx.say(err)
            end
        }

        echo -n;
    }
--- response_body
--- shutdown_error_log eval: qr/\[debug\].*?free engine.*/
--- no_error_log
[error]



=== TEST 5: proxy_wasm new() - in init
modules not loaded yet, bad phase
--- wasm_modules: on_phases
--- http_config
    init_by_lua_block {
        local proxy_wasm = require "resty.http.proxy_wasm"
        local filters = {
            { name = "on_phases", config = "a" },
        }

        proxy_wasm.new(filters)
    }
--- error_log
must be called from "init_worker" or "access"
--- no_error_log
[crit]
[alert]
--- must_die



=== TEST 6: proxy_wasm new() - sanity in init_worker
--- wasm_modules: on_phases
--- http_config
    init_worker_by_lua_block {
        local proxy_wasm = require "resty.http.proxy_wasm"
        local filters = {
            { name = "on_phases" },
        }

        local c_ops, err = proxy_wasm.new(filters)
        if not c_ops then
            return ngx.log(ngx.ERR, err)
        end
    }
--- grep_error_log eval: qr/#\d+ on_configure.*/
--- grep_error_log_out eval
qr/#0 on_configure, config_size: 0.*/
--- no_error_log
[error]
[crit]



=== TEST 7: proxy_wasm attach() - bad phase
--- config
    location /t {
        rewrite_by_lua_block {
            local proxy_wasm = require "resty.http.proxy_wasm"

            local pok, perr = pcall(proxy_wasm.attach, c_ops)
            if not pok then
                ngx.log(ngx.INFO, perr)
            end
        }

        return 200;
    }
--- no_error_log
[error]
[crit]
[alert]
--- error
no request found



=== TEST 8: proxy_wasm new() + attach() - sanity
--- wasm_modules: hostcalls
--- config
    location /t {
        access_by_lua_block {
            local proxy_wasm = require "resty.http.proxy_wasm"
            local filters = {
                { name = "hostcalls", config = "test=/t/echo/headers" },
            }

            local c_ops, err = proxy_wasm.new(filters)
            if not c_ops then
                return ngx.say(err)
            end

            local ok, err = proxy_wasm.attach(c_ops)
            if not ok then
                return ngx.say(err)
            end
        }
    }
--- response_body
Host: localhost
Connection: close
--- no_error_log
[error]
[crit]



=== TEST 9: proxy_wasm new() + attach() - from access phase
on_configure invoked twice: once for the root context+instance,
once for the request context+instance
--- wasm_modules: on_phases
--- config
    location /t {
        access_by_lua_block {
            local proxy_wasm = require "resty.http.proxy_wasm"
            local filters = {
                { name = "on_phases" },
            }

            local c_ops, err = proxy_wasm.new(filters)
            if not c_ops then
                return ngx.say(err)
            end

            local ok, err = proxy_wasm.attach(c_ops)
            if not ok then
                return ngx.say(err)
            end

            ngx.say("ok")
        }
    }
--- request
POST /t
Hello world
--- response_body
ok
--- grep_error_log eval: qr/#\d+ on_(configure|request|response|log).*/
--- grep_error_log_out eval
qr/#0 on_configure, config_size: 0.*
#0 on_configure, config_size: 0.*
#\d+ on_request_headers, 3 headers.*
#\d+ on_request_body, 11 bytes.*
#\d+ on_response_headers, 5 headers.*
#\d+ on_response_body, 3 bytes, end_of_stream: false.*
#\d+ on_response_body, 0 bytes, end_of_stream: true.*
#\d+ on_log.*/
--- no_error_log
[error]



=== TEST 10: proxy_wasm new() + attach() - from init/access phases
--- wasm_modules: on_phases
--- http_config
    init_worker_by_lua_block {
        local proxy_wasm = require "resty.http.proxy_wasm"
        local filters = {
            { name = "on_phases" },
        }

        local c_ops, err = proxy_wasm.new(filters)
        if not c_ops then
            return ngx.log(ngx.ERR, err)
        end

        _G.c_ops = c_ops
    }
--- config
    location /t {
        access_by_lua_block {
            local proxy_wasm = require "resty.http.proxy_wasm"

            local c_ops = _G.c_ops

            local ok, err = proxy_wasm.attach(c_ops)
            if not ok then
                return ngx.say(err)
            end

            ngx.say("ok")
        }
    }
--- request
POST /t
Hello world
--- response_body
ok
--- grep_error_log eval: qr/#\d+ on_(configure|request|response|log).*/
--- grep_error_log_out eval
qr/#0 on_configure, config_size: 0.*
#0 on_configure, config_size: 0.*
#\d+ on_request_headers, 3 headers.*
#\d+ on_request_body, 11 bytes.*
#\d+ on_response_headers, 5 headers.*
#\d+ on_response_body, 3 bytes, end_of_stream: false.*
#\d+ on_response_body, 0 bytes, end_of_stream: true.*
#\d+ on_log.*/
--- no_error_log
[error]
