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
        local proxy_wasm = require "resty.http.proxy_wasm"
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
            local proxy_wasm = require "resty.http.proxy_wasm"
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
        local proxy_wasm = require "resty.http.proxy_wasm"
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
            local proxy_wasm = require "resty.http.proxy_wasm"
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
        local proxy_wasm = require "resty.http.proxy_wasm"
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
            local proxy_wasm = require "resty.http.proxy_wasm"
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
        local proxy_wasm = require "resty.http.proxy_wasm"
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
            local proxy_wasm = require "resty.http.proxy_wasm"
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
        local proxy_wasm = require "resty.http.proxy_wasm"
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
            local proxy_wasm = require "resty.http.proxy_wasm"
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
        local proxy_wasm = require "resty.http.proxy_wasm"
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
            local proxy_wasm = require "resty.http.proxy_wasm"
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
qr/on_http_call_response \(id: \d+, headers: 5, body_bytes: \d+, trailers: 0/
--- no_error_log
[error]
[crit]
[emerg]



=== TEST 7: proxy_wasm FFI - HTTP dispatch alongside Lua VM (trap on response)
Wasmtime trap format:
    [error] error while executing ...
    [stacktrace]
    Caused by:
        [trap msg]

Wasmer trap format:
    [error] [trap msg]

V8 trap format:
    [error] Uncaught RuntimeError: [trap msg]
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
        local proxy_wasm = require "resty.http.proxy_wasm"
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
            local proxy_wasm = require "resty.http.proxy_wasm"
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
    qr/on_http_call_response \(id: \d+, headers: 5, body_bytes: \d+, trailers: 0, op: trap\)/,
    qr/\[crit\] .*? panicked at 'trap!'/,
    qr/\[error\] .*? (error while executing at wasm backtrace:|(Uncaught RuntimeError)?unreachable)/
]
--- no_error_log
[emerg]
