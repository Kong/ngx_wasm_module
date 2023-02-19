# vim:set ft= ts=4 sw=4 et fdm=marker:

BEGIN {
    $ENV{MOCKEAGAIN} = 'rw';
    $ENV{MOCKEAGAIN_VERBOSE} ||= 0;
    $ENV{TEST_NGINX_EVENT_TYPE} = 'poll';
}

use strict;
use lib '.';
use t::TestWasm::Lua;

our $ExtResolver = $t::TestWasm::extresolver;
our $ExtTimeout = $t::TestWasm::exttimeout;

skip_no_openresty();

plan tests => repeat_each() * (blocks() * 5);

run_tests();

__DATA__

=== TEST 1: Lua bridge - Lua threads can EAGAIN
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



=== TEST 2: Lua bridge - Lua threads can timeout
lua-resty-dns-resolver client timeout
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- http_config eval
qq{
    server {
        listen       $ENV{TEST_NGINX_SERVER_PORT2};
        server_name  localhost;

        location / {
            echo "hello world";
        }
    }

    init_worker_by_lua_block {
        dns_client = require 'resty.dns.client'
        local opts = {
            noSynchronisation = true,
            hosts = { '127.0.0.1 localhost' },
            resolvConf = {
                'nameserver $::ExtResolver',
                'options timeout:1',
            }
        }
        assert(dns_client.init(opts))
        opts.timeout = 1
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
--- error_code: 500
--- response_body_like: 500 Internal Server Error
--- error_log eval
qr/\[error\] .*? lua entry thread aborted: .*? wasm lua failed resolving "httpbin\.org": failed to receive reply/
--- no_error_log
[crit]
[alert]
