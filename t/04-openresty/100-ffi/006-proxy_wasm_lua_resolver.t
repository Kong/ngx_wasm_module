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

=== TEST 1: lua-resty-dns-client ran by wasm_tcp_socket (on_request_headers)
--- skip_no_debug: 4
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm_lua_resolver on;
        proxy_wasm hostcalls 'on=request_headers test=/t/dispatch_http_call \
                              host=www.google.com \
                              path=/ \
                              on_http_call_response=echo_response_body';
        echo failed;
    }
--- response_body_like
.+google.+
--- error_log eval
qr/\[debug\] .*? lua resolver resuming thread \(co: .*\)/
--- no_error_log
[error]
[crit]



=== TEST 2: lua_resolver not ran by wasm_tcp_socket (on_request_headers)
--- skip_no_debug: 4
--- timeout eval: $::ExtTimeout
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config eval
qq{
    resolver         $::ExtResolver ipv6=off;
    resolver_timeout $::ExtTimeout;

    location /t {
        proxy_wasm hostcalls 'on=request_headers test=/t/dispatch_http_call \
                              host=www.google.com \
                              path=/ \
                              on_http_call_response=echo_response_body';
        echo failed;
    }
}
--- response_body_like
.+google.+
--- no_error_log
[error]
[crit]
lua resolver



=== TEST 3: lua-resty-dns-client ran by wasm_tcp_socket (on_request_headers)
--- skip_no_debug: 4
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- http_config
server {
    listen       $TEST_NGINX_SERVER_PORT2;
    server_name  local_wasmx;

    location / {
        echo "wasmx rocks";
    }
}

    init_worker_by_lua_block {
        dns_client = require 'resty.dns.client'
        dns_client.init(
        {
            noSynchronisation = true,
            hosts = {'127.0.0.1 local_wasmx'}
        })
    }
--- config
    location /t {
        proxy_wasm_lua_resolver on;
        proxy_wasm hostcalls 'on=request_headers test=/t/dispatch_http_call \
                              host=local_wasmx:$TEST_NGINX_SERVER_PORT2 \
                              path=/ \
                              on_http_call_response=echo_response_body';
        echo failed;
    }
--- response_body_like
wasmx rocks
--- error_log eval
qr/\[debug\] .*? lua resolver using existing dns_client/
--- no_error_log
[error]
[crit]
