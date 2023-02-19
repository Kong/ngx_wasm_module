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

=== TEST 1: Lua bridge - proxy_wasm_lua_resolver enabled (on_request_headers)
--- skip_no_debug: 4
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
--- response_body_like
\s+"Host": "httpbin\.org".*
--- error_log eval
[
    qr/\[debug\] .*? wasm lua resolver resuming thread/,
    qr/\[debug\] .*? wasm lua resolver creating new dns_client/
]
--- no_error_log
[error]



=== TEST 2: Lua bridge - proxy_wasm_lua_resolver enabled, use default nameserver and timeout (on_request_headers)
lua-resty-dns-client default timeout is 2000ms
NGX_WASM_DEFAULT_RESOLVER_TIMEOUT is 30000ms
--- skip_no_debug: 4
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
--- response_body_like
\s+"Host": "httpbin\.org".*
--- error_log eval
[
    qr/\[debug\] .*? wasm lua resolver creating new dns_client/,
    qr/\[debug\] .*? \[dns-client\] nameserver 8\.8\.8\.8/,
    qr/\[debug\] .*? \[dns-client\] timeout = 30000 ms/
]



=== TEST 3: Lua bridge - proxy_wasm_lua_resolver enabled with existing dns_client (on_request_headers)
--- skip_no_debug: 4
--- timeout eval: $::ExtTimeout
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
--- response_body_like
hello world
--- error_log eval
qr/\[debug\] .*? wasm lua resolver using existing dns_client/
--- no_error_log
[error]
[crit]



=== TEST 4: Lua bridge - proxy_wasm_lua_resolver disabled (on_request_headers)
--- skip_no_debug: 4
--- timeout eval: $::ExtTimeout
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config eval
qq{
    resolver         $::ExtResolver ipv6=off;
    resolver_timeout $::ExtTimeout;

    location /t {
        proxy_wasm_lua_resolver off;
        proxy_wasm hostcalls 'on=request_headers \
                              test=/t/dispatch_http_call \
                              host=httpbin.org \
                              path=/headers \
                              on_http_call_response=echo_response_body';
        echo failed;
    }
}
--- response_body_like
\s+"Host": "httpbin\.org".*
--- no_error_log
[error]
[crit]
lua resolver
