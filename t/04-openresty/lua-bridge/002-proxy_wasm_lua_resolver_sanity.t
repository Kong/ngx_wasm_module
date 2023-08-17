# vim:set ft= ts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm::Lua;

our $ExtResolver = $t::TestWasm::extresolver;
our $ExtTimeout = $t::TestWasm::exttimeout;

skip_no_openresty();

plan tests => repeat_each() * (blocks() * 5);

run_tests();

__DATA__

=== TEST 1: Lua bridge - proxy_wasm_lua_resolver, sanity (on_request_headers)
Succeeds on:
- HTTP 200 (httpbin.org/headers success)
- HTTP 502 (httpbin.org Bad Gateway)
- HTTP 504 (httpbin.org Gateway timeout)
--- skip_no_debug: 5
--- timeout eval: $::ExtTimeout
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm_lua_resolver on;
        proxy_wasm hostcalls 'on=request_headers \
                              test=/t/dispatch_http_call \
                              host=httpbin.org \
                              path=/headers \
                              on_http_call_response=echo_response_body';
        echo failed;
    }
--- error_code_like: (200|502|504)
--- response_body_like: ("Host": "httpbin\.org"|.*?502 Bad Gateway.*|.*?504 Gateway Time-out.*)
--- error_log eval
[
    qr/\[debug\] .*? wasm lua resolver thread/,
    qr/\[debug\] .*? wasm lua resolver creating new dns_client/
]
--- no_error_log
[error]



=== TEST 2: proxy_wasm - proxy_wasm_lua_resolver, sanity (on_tick)
--- skip_no_debug: 5
--- load_nginx_modules: ngx_http_echo_module
--- main_config eval
qq{
    wasm {
        proxy_wasm_lua_resolver on;
        module hostcalls $t::TestWasm::crates/hostcalls.wasm;
    }
}
--- config
    location /dispatched {
        echo "Hello world";
    }

    location /t {
        proxy_wasm hostcalls 'tick_period=5 \
                              on_tick=dispatch \
                              host=localhost:$TEST_NGINX_SERVER_PORT \
                              path=/dispatched';
        echo ok;
    }
--- response_body
ok
--- error_log eval
[
    qr/on_root_http_call_response \(id: 0, status: 200, headers: 5, body_bytes: 12, trailers: 0\)/,
    qr/\[debug\] .*? wasm lua resolved "localhost" to "127\.0\.0\.1"/,
]
--- no_error_log
[error]



=== TEST 3: Lua bridge - proxy_wasm_lua_resolver, disabled by default
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
--- config eval
qq{
    resolver      $::ExtResolver ipv6=off;
    resolver_add  127.0.0.1 localhost;

    location /t {
        proxy_wasm_lua_resolver off;
        proxy_wasm hostcalls 'on=request_headers \
                              test=/t/dispatch_http_call \
                              host=localhost:$ENV{TEST_NGINX_SERVER_PORT2} \
                              path=/headers \
                              on_http_call_response=echo_response_body';
        echo failed;
    }
}
--- response_body_like: Host: localhost
--- no_error_log
[error]
[crit]
lua resolver



=== TEST 4: Lua bridge - proxy_wasm_lua_resolver, explicitly disabled
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
--- config eval
qq{
    resolver      $::ExtResolver ipv6=off;
    resolver_add  127.0.0.1 localhost;

    location /t {
        proxy_wasm_lua_resolver off;
        proxy_wasm hostcalls 'on=request_headers \
                              test=/t/dispatch_http_call \
                              host=localhost:$ENV{TEST_NGINX_SERVER_PORT2} \
                              path=/headers \
                              on_http_call_response=echo_response_body';
        echo failed;
    }
}
--- response_body_like: Host: localhost
--- no_error_log
[error]
[crit]
lua resolver



=== TEST 5: Lua bridge - proxy_wasm_lua_resolver, explicitly disabled while enabled in wasm{}
--- load_nginx_modules: ngx_http_echo_module
--- main_config eval
qq{
    wasm {
        proxy_wasm_lua_resolver on;
        module hostcalls $t::TestWasm::crates/hostcalls.wasm;
    }
}
--- http_config
server {
    listen       $TEST_NGINX_SERVER_PORT2;
    server_name  localhost;

    location /headers {
        echo $echo_client_request_headers;
    }
}
--- config eval
qq{
    resolver      $::ExtResolver ipv6=off;
    resolver_add  127.0.0.1 localhost;

    location /t {
        proxy_wasm_lua_resolver off;
        proxy_wasm hostcalls 'on=request_headers \
                              test=/t/dispatch_http_call \
                              host=localhost:$ENV{TEST_NGINX_SERVER_PORT2} \
                              path=/headers \
                              on_http_call_response=echo_response_body';
        echo failed;
    }
}
--- response_body_like: Host: localhost
--- no_error_log
[error]
[crit]
lua resolver



=== TEST 6: Lua bridge - proxy_wasm_lua_resolver, explicitly disabled while enabled in http{}
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
--- config eval
qq{
    resolver      $::ExtResolver ipv6=off;
    resolver_add  127.0.0.1 localhost;

    proxy_wasm_lua_resolver on;

    location /t {
        proxy_wasm_lua_resolver off;
        proxy_wasm hostcalls 'on=request_headers \
                              test=/t/dispatch_http_call \
                              host=localhost:$ENV{TEST_NGINX_SERVER_PORT2} \
                              path=/headers \
                              on_http_call_response=echo_response_body';
        echo failed;
    }
}
--- response_body_like: Host: localhost
--- no_error_log
[error]
[crit]
lua resolver



=== TEST 7: Lua bridge - proxy_wasm_lua_resolver, default client settings
lua-resty-dns-client default timeout is 2000ms
NGX_WASM_DEFAULT_RESOLVER_TIMEOUT is 30000ms
Needs IPv4 resolution + external I/O to succeed.
Succeeds on:
- HTTP 200 (httpbin.org/headers success)
- HTTP 502 (httpbin.org Bad Gateway)
- HTTP 504 (httpbin.org Gateway timeout)
--- skip_eval: 5: $t::TestWasm::nginxV !~ m/--with-debug/ || defined $ENV{GITHUB_ACTIONS}
--- skip_no_debug: 5
--- timeout eval: $::ExtTimeout
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm_lua_resolver on;
        proxy_wasm hostcalls 'on=request_headers \
                              test=/t/dispatch_http_call \
                              host=httpbin.org \
                              path=/headers \
                              on_http_call_response=echo_response_body';
        echo failed;
    }
--- error_code_like: (200|502|504)
--- response_body_like: ("Host": "httpbin\.org"|.*?502 Bad Gateway.*|.*?504 Gateway Time-out.*)
--- error_log eval
[
    qr/\[debug\] .*? wasm lua resolver creating new dns_client/,
    qr/\[debug\] .*? \[dns-client\] nameserver 8\.8\.8\.8/,
    qr/\[debug\] .*? \[dns-client\] timeout = 30000 ms/
]



=== TEST 8: Lua bridge - proxy_wasm_lua_resolver, existing client
--- skip_no_debug: 5
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- http_config
    server {
        listen       $TEST_NGINX_SERVER_PORT2;
        server_name  localhost;

        location / {
            echo "hello world";
        }
    }

    init_worker_by_lua_block {
        dns_client = require 'resty.dns.client'
        assert(dns_client.init({
            noSynchronisation = true,
            hosts = { '127.0.0.1 localhost' }
        }))
    }
--- config
    location /t {
        proxy_wasm_lua_resolver on;
        proxy_wasm hostcalls 'on=request_headers \
                              test=/t/dispatch_http_call \
                              host=localhost:$TEST_NGINX_SERVER_PORT2 \
                              path=/ \
                              on_http_call_response=echo_response_body';
        echo failed;
    }
--- response_body
hello world
--- error_log eval
qr/\[debug\] .*? wasm lua resolver using existing dns_client/
--- no_error_log
[error]
[crit]



=== TEST 9: Lua bridge - proxy_wasm_lua_resolver, synchronized client
Too slow for Valgrind.
Succeeds on:
- HTTP 200 (httpbin.org/headers success)
- HTTP 502 (httpbin.org Bad Gateway)
- HTTP 504 (httpbin.org Gateway timeout)
--- skip_valgrind: 5
--- skip_no_debug: 5
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- http_config eval
qq{
    init_worker_by_lua_block {
        dns_client = require 'resty.dns.client'
        assert(dns_client.init({
            --noSynchronisation = false, -- default
            order = { 'A' },
            hosts = { '127.0.0.1 localhost' },
            resolvConf = {
                'nameserver $::ExtResolver',
                'options timeout:$::ExtTimeout',
            }
        }))
    }
}
--- config
    location /t {
        proxy_wasm_lua_resolver on;
        proxy_wasm hostcalls 'on=request_headers \
                              test=/t/dispatch_http_call \
                              host=httpbin.org \
                              path=/headers \
                              on_http_call_response=echo_response_body';
        echo failed;
    }
--- error_code_like: (200|502|504)
--- response_body_like: ("Host": "httpbin(\.org)?"|.*?502 Bad Gateway.*|.*?504 Gateway Time-out.*)
--- error_log eval
qr/\[debug\] .*? wasm lua resolver using existing dns_client/
--- no_error_log
[error]
[crit]



=== TEST 10: Lua bridge - proxy_wasm_lua_resolver, SRV record
--- skip_no_debug: 5
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- http_config
    server {
        listen       $TEST_NGINX_SERVER_PORT2;
        server_name  localhost;

        location / {
            echo "hello world";
        }
    }

    init_worker_by_lua_block {
        local resolver = require 'resty.dns.resolver'
        local old_query = resolver.query

        local TYPE_SRV = 33
        local mock_records = {
            [TYPE_SRV] = {
                ['srv.localhost'] = {
                    {
                        type = TYPE_SRV,
                        name = 'srv.localhost',
                        target = 'localhost',
                        port = $TEST_NGINX_SERVER_PORT2,
                        weight = 10,
                        ttl = 600,
                        priority = 20,
                        class = 1
                    }
                }
            }
        }

        resolver.query = function(self, name, options, tries)
            local qtype = (options or {}).qtype or resolver.TYPE_A
            local record = (mock_records[qtype] or {})[name]
            if record then
                ngx.log(ngx.DEBUG, 'answering "', name, '" with mock record')
                return record, nil, tries
            end

            ngx.log(ngx.DEBUG, 'no mock record for "', name, '"')

            return old_query(self, name, options, tries)
        end

        dns_client = require 'resty.dns.client'
        assert(dns_client.init({
            noSynchronisation = true,
            hosts = { '127.0.0.1 localhost' }
        }))
    }
--- config
    location /t {
        proxy_wasm_lua_resolver on;
        proxy_wasm hostcalls 'on=request_headers \
                              test=/t/dispatch_http_call \
                              host=srv.localhost \
                              path=/ \
                              on_http_call_response=echo_response_body';
        echo failed;
    }
--- response_body_like
hello world
--- error_log eval
[
    qr/\[debug\] .*? wasm lua resolved "srv.localhost" to "127\.0\.0\.1:\d+"/,
    qr/\[debug\] .*? name was resolved to 127\.0\.0\.1:\d+/
]
--- no_error_log
[error]



=== TEST 11: Lua bridge - proxy_wasm_lua_resolver, IPv6 record
--- skip_eval: 5: system("cat /sys/module/ipv6/parameters/disable") ne 0 || defined $ENV{GITHUB_ACTIONS}
--- timeout eval: $::ExtTimeout
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm_lua_resolver on;
        proxy_wasm hostcalls 'on=request_headers \
                              test=/t/dispatch_http_call \
                              host=ipv6.google.com \
                              path=/
                              on_http_call_response=echo_response_body';
        echo failed;
    }
--- response_body_like: \<!doctype html\>
--- no_error_log
[error]
[crit]
[emerg]



=== TEST 12: Lua bridge - proxy_wasm_lua_resolver, NXDOMAIN
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm_lua_resolver on;
        proxy_wasm hostcalls 'on=request_headers \
                              test=/t/dispatch_http_call \
                              host=foo';
        echo failed;
    }
--- error_code: 500
--- response_body_like: 500 Internal Server Error
--- grep_error_log eval: qr/\[error\].*/
--- grep_error_log_out eval
qr/\[error\] .*? lua entry thread aborted: .*? wasm lua failed resolving "foo": dns client error: 101 empty record received.*?
\[error\] .*? dispatch failed: tcp socket - lua resolver failed.*?/
--- no_error_log
[crit]
[emerg]



=== TEST 13: Lua bridge - proxy_wasm_lua_resolver, failed before query
Failure before the Lua thread gets a chance to yield (immediate resolver failure)
--- skip_no_debug: 5
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- http_config
    init_worker_by_lua_block {
        local resolver = require 'resty.dns.resolver'

        resolver.query = function(self, name, options, tries)
            return nil, 'some made-up error'
        end

        dns_client = require 'resty.dns.client'
        assert(dns_client.init({
            noSynchronisation = true,
            order = { 'A' },
            hosts = { '127.0.0.1 localhost' }
        }))
    }
--- config
    location /t {
        proxy_wasm_lua_resolver on;
        proxy_wasm hostcalls 'on=request_headers \
                              test=/t/dispatch_http_call \
                              host=httpbin.org \
                              path=/ \
                              on_http_call_response=echo_response_body';
        echo failed;
    }
--- error_code: 500
--- response_body_like: 500 Internal Server Error
--- grep_error_log eval: qr/\[error\].*/
--- grep_error_log_out eval
qr/\[error\] .*? lua entry thread aborted: .*? wasm lua failed resolving "httpbin\.org": some made-up error.*?
\[error\] .*? dispatch failed.*?/
--- error_log
wasm tcp socket resolver failed before query
--- no_error_log
[crit]
