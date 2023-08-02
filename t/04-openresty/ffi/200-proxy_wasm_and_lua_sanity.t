# vim:set ft= ts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm::Lua;

skip_no_openresty();

plan tests => repeat_each() * (blocks() * 6);

run_tests();

__DATA__

=== TEST 1: proxy_wasm FFI - request headers manipulation alongside Lua VM
--- wasm_modules: hostcalls
--- http_config
    init_worker_by_lua_block {
        local proxy_wasm = require "resty.wasmx.proxy_wasm"
        local filters = {
            -- Wasm -> Lua
            { name = "hostcalls", config = "test=/t/set_request_headers/special" },
            -- Lua -> Wasm
            { name = "hostcalls", config = "test=/t/log/request_header on=log name=Lua" },
        }

        local c_plan = assert(proxy_wasm.new(filters))
        assert(proxy_wasm.load(c_plan))

        _G.c_plan = c_plan
    }
--- config
    location /t {
        rewrite_by_lua_block {
            local proxy_wasm = require "resty.wasmx.proxy_wasm"
            assert(proxy_wasm.attach(_G.c_plan))
            assert(proxy_wasm.start())

            ngx.req.set_header("Lua", "yes")
        }

        content_by_lua_block {
            local h = assert(ngx.req.get_headers())
            ngx.say(
                "request_uri: ", ngx.var.request_uri, "\n",
                "uri: ", ngx.var.uri, "\n",
                "Host: ", h.host, "\n",
                "Connection: ", h.connection, "\n",
                "X-Forwarded-For: ", h.x_forwarded_for
            )
        }
    }
--- response_body
request_uri: /t
uri: /updated
Host: somehost
Connection: closed
X-Forwarded-For: 128.168.0.1
--- error_log
request header "Lua: yes" while logging request
--- no_error_log
[error]
[crit]
[emerg]



=== TEST 2: proxy_wasm FFI - response headers manipulation alongside Lua VM
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- http_config
    init_worker_by_lua_block {
        local proxy_wasm = require "resty.wasmx.proxy_wasm"
        local filters = {
            -- Wasm -> Lua
            { name = "hostcalls", config = "test=/t/set_response_headers" },
            -- Lua -> Wasm
            { name = "hostcalls", config = "test=/t/log/response_header on=log name=hello" },
        }

        local c_plan = assert(proxy_wasm.new(filters))
        assert(proxy_wasm.load(c_plan))

        _G.c_plan = c_plan
    }
--- config
    location /t {
        rewrite_by_lua_block {
            local proxy_wasm = require "resty.wasmx.proxy_wasm"
            assert(proxy_wasm.attach(_G.c_plan))
            assert(proxy_wasm.start())
        }

        echo ok;

        header_filter_by_lua_block {
            -- Lua -> Wasm
            ngx.header.hello = nil
        }
    }
--- response_headers
Welcome: wasm
--- response_body
ok
--- error_log
resp header "hello: " while logging request
--- no_error_log
[error]
[crit]



=== TEST 3: proxy_wasm FFI - request body manipulation alongside Lua VM
--- wasm_modules: hostcalls
--- http_config
    init_worker_by_lua_block {
        local proxy_wasm = require "resty.wasmx.proxy_wasm"
        local filters = {
            -- Wasm -> Lua
            { name = "hostcalls", config = "test=/t/set_request_body" },
            -- Lua -> Wasm
            { name = "hostcalls", config = "test=/t/log/request_body on=log" },
        }

        local c_plan = assert(proxy_wasm.new(filters))
        assert(proxy_wasm.load(c_plan))

        _G.c_plan = c_plan
    }
--- config
    location /t {
        rewrite_by_lua_block {
            local proxy_wasm = require "resty.wasmx.proxy_wasm"
            assert(proxy_wasm.attach(_G.c_plan))
            assert(proxy_wasm.start())
        }

        content_by_lua_block {
            local body = ngx.req.get_body_data()
            ngx.say(body)

            ngx.req.set_body_data("From Lua")
        }
    }
--- response_body
Hello world
--- error_log
request body: From Lua while logging request
--- no_error_log
[error]
[crit]
[emerg]



=== TEST 4: proxy_wasm FFI - response body manipulation alongside Lua VM
--- wasm_modules: hostcalls
--- http_config
    init_worker_by_lua_block {
        local proxy_wasm = require "resty.wasmx.proxy_wasm"
        local filters = {
            -- Wasm -> Lua
            { name = "hostcalls", config = "test=/t/set_response_body on=response_body" },
        }

        local c_plan = assert(proxy_wasm.new(filters))
        assert(proxy_wasm.load(c_plan))

        _G.c_plan = c_plan
    }
--- config
    location /t {
        rewrite_by_lua_block {
            local proxy_wasm = require "resty.wasmx.proxy_wasm"
            assert(proxy_wasm.attach(_G.c_plan))
            assert(proxy_wasm.start())
        }

        content_by_lua_block {
            ngx.say("fail")
        }
    }
--- response_body
Hello world
--- no_error_log
[error]
[crit]
[emerg]
[alert]



=== TEST 5: proxy_wasm FFI - HTTP dispatch alongside Lua VM (echo dispatch response)
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- http_config
    server {
        listen       $TEST_NGINX_SERVER_PORT2;
        server_name  localhost;

        location /headers {
            echo $echo_client_request_headers;
        }
    }

    init_worker_by_lua_block {
        local proxy_wasm = require "resty.wasmx.proxy_wasm"
        local filters = {
            {
                name = "hostcalls",
                config = "test=/t/dispatch_http_call " ..
                         "host=localhost:$TEST_NGINX_SERVER_PORT2 " ..
                         "path=/headers " ..
                         "on_http_call_response=echo_response_body"
            }
        }

        local c_plan = assert(proxy_wasm.new(filters))
        assert(proxy_wasm.load(c_plan))

        _G.c_plan = c_plan
    }
--- config
    resolver     1.1.1.1 ipv6=off;
    resolver_add 127.0.0.1 localhost;

    location /t {
        rewrite_by_lua_block {
            local proxy_wasm = require "resty.wasmx.proxy_wasm"
            assert(proxy_wasm.attach(_G.c_plan))
            assert(proxy_wasm.start())
        }

        content_by_lua_block {
            ngx.say("fail")
        }
    }
--- response_body_like
GET \/headers HTTP\/.*
Host: localhost:\d+.*
Connection: close.*
Content-Length: 0.*
--- no_error_log
[error]
[crit]
[emerg]
[alert]



=== TEST 6: proxy_wasm FFI - HTTP dispatch alongside Lua VM (ignore dispatch response)
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- http_config
    server {
        listen       $TEST_NGINX_SERVER_PORT2;
        server_name  localhost;

        location /headers {
            echo $echo_client_request_headers;
        }
    }

    init_worker_by_lua_block {
        local proxy_wasm = require "resty.wasmx.proxy_wasm"
        local filters = {
            {
                name = "hostcalls",
                config = "test=/t/dispatch_http_call " ..
                         "host=localhost:$TEST_NGINX_SERVER_PORT2 " ..
                         "path=/headers "
            }
        }

        local c_plan = assert(proxy_wasm.new(filters))
        assert(proxy_wasm.load(c_plan))

        _G.c_plan = c_plan
    }
--- config
    resolver     1.1.1.1 ipv6=off;
    resolver_add 127.0.0.1 localhost;

    location /t {
        rewrite_by_lua_block {
            local proxy_wasm = require "resty.wasmx.proxy_wasm"
            assert(proxy_wasm.attach(_G.c_plan))
            assert(proxy_wasm.start())
        }

        content_by_lua_block {
            ngx.say("ok")
        }
    }
--- response_body
ok
--- error_log eval
qr/on_http_call_response \(id: \d+, status: 200, headers: 5, body_bytes: \d+, trailers: 0/
--- grep_error_log eval: qr/\*\d+.*?\[proxy-wasm\].*?(resuming|freeing).*/
--- grep_error_log_out eval
qr/\A\*\d+ .*? filter 1\/1 resuming "on_request_headers" step in "rewrite" phase[^#*]*
\*\d+ .*? filter 1\/1 resuming "on_dispatch_response" step in "access" phase[^#*]*
\*\d+ .*? filter 1\/1 resuming "on_response_body" step in "body_filter" phase[^#*]*
\*\d+ .*? filter 1\/1 resuming "on_response_body" step in "body_filter" phase[^#*]*
\*\d+ .*? filter 1\/1 resuming "on_log" step in "log" phase[^#*]*
\*\d+ .*? filter 1\/1 resuming "on_done" step in "done" phase[^#*]*
\*\d+ .*? filter freeing context #\d+ \(1\/1\)[^#*]*\Z/
--- no_error_log
[error]
[crit]



=== TEST 7: proxy_wasm FFI - HTTP dispatch alongside Lua VM (trap on response)
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- http_config
    server {
        listen       $TEST_NGINX_SERVER_PORT2;
        server_name  localhost;

        location /headers {
            echo $echo_client_request_headers;
        }
    }

    init_worker_by_lua_block {
        local proxy_wasm = require "resty.wasmx.proxy_wasm"
        local filters = {
            {
                name = "hostcalls",
                config = "test=/t/dispatch_http_call " ..
                         "host=localhost:$TEST_NGINX_SERVER_PORT2 " ..
                         "path=/headers " ..
                         "on_http_call_response=trap"
            }
        }

        local c_plan = assert(proxy_wasm.new(filters))
        assert(proxy_wasm.load(c_plan))

        _G.c_plan = c_plan
    }
--- config
    resolver     1.1.1.1 ipv6=off;
    resolver_add 127.0.0.1 localhost;

    location /t {
        rewrite_by_lua_block {
            local proxy_wasm = require "resty.wasmx.proxy_wasm"
            assert(proxy_wasm.attach(_G.c_plan))
            assert(proxy_wasm.start())
        }

        content_by_lua_block {
            ngx.say("fail")
        }
    }
--- error_code: 500
--- response_body_like: 500 Internal Server Error
--- error_log eval
[
    qr/on_http_call_response \(id: \d+, status: 200, headers: 5, body_bytes: \d+, trailers: 0, op: trap\)/,
    qr/\[crit\] .*? panicked at/,
    qr/trap!/,
]
--- grep_error_log eval: qr/\*\d+.*?\[proxy-wasm\].*?(resuming|freeing).*/
--- grep_error_log_out eval
qr/\A\*\d+ .*? filter 1\/1 resuming "on_request_headers" step in "rewrite" phase[^#*]*
\*\d+ .*? filter 1\/1 resuming "on_dispatch_response" step in "access" phase[^#*]*
\*\d+ .*? filter 1\/1 failed resuming "on_response_body" step in "body_filter" phase \(dispatch failed\)[^#*]*
\*\d+ .*? filter freeing context #\d+ \(1\/1\)[^#*]*\Z/



=== TEST 8: proxy_wasm FFI - filter chain attached before error handler location
Run 1 filter chain in /t, but produce content through /error

At r->pool cleanup, the done step is invoked the /t chain, and the chain is
freed. /error does not concern itself with any chain.

--- skip_no_debug: 6
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- http_config
    init_worker_by_lua_block {
        local proxy_wasm = require "resty.wasmx.proxy_wasm"

        local c_plan = assert(proxy_wasm.new({
            { name = "hostcalls", config = "test=/t/log/current_time on=log" },
        }))
        assert(proxy_wasm.load(c_plan))

        _G.c_plan = c_plan
    }
--- config
    error_page 502 /error;

    location /t {
        rewrite_by_lua_block {
            local proxy_wasm = require "resty.wasmx.proxy_wasm"
            assert(proxy_wasm.attach(_G.c_plan))
            assert(proxy_wasm.start())
        }

        access_by_lua_block {
            ngx.exit(502)
        }

        echo failed;
    }

    location = /error {
        internal;
        echo 'error handler content';
    }
--- error_code: 502
--- response_body
error handler content
--- grep_error_log eval: qr/\*\d+.*?(resuming "on_done"|wasm cleaning up).*/
--- grep_error_log_out eval
qr/.*? wasm cleaning up request pool.*
.*?resuming "on_done" step/
--- no_error_log
[error]
[crit]
[emerg]



=== TEST 9: proxy_wasm FFI - filter chain attached before and during error handler location
Run 2 filters chains:
1. /t
2. /error

At r->pool cleanup, the done step is invoked for both chains, and both chains
are freed.

--- skip_no_debug: 6
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- http_config
    init_worker_by_lua_block {
        local proxy_wasm = require "resty.wasmx.proxy_wasm"

        local t_plan = assert(proxy_wasm.new({
            { name = "hostcalls", config = "test=/t/log/current_time on=log" },
        }))
        assert(proxy_wasm.load(t_plan))

        local error_plan = assert(proxy_wasm.new({
            {
                name = "hostcalls",
                config = "test=/t/set_response_body value=error_handler_content on=response_body"
            }
        }))
        assert(proxy_wasm.load(error_plan))

        _G.t_plan = t_plan
        _G.error_plan = error_plan
    }
--- config
    error_page 502 /error;

    location /t {
        rewrite_by_lua_block {
            local proxy_wasm = require "resty.wasmx.proxy_wasm"
            assert(proxy_wasm.attach(_G.t_plan))
            assert(proxy_wasm.start())
        }

        access_by_lua_block {
            ngx.exit(502)
        }

        echo failed;
    }

    location = /error {
        internal;

        rewrite_by_lua_block {
            local proxy_wasm = require "resty.wasmx.proxy_wasm"
            assert(proxy_wasm.attach(_G.error_plan))
            assert(proxy_wasm.start())
        }

        echo failed;
    }
--- error_code: 502
--- response_body
error_handler_content
--- grep_error_log eval: qr/\*\d+.*?(resuming "on_done"|wasm cleaning up).*/
--- grep_error_log_out eval
qr/.*?wasm cleaning up request pool.*
.*?resuming "on_done" step.*
.*?wasm cleaning up request pool.*
.*?resuming "on_done" step/
--- no_error_log
[error]
[crit]
[emerg]
