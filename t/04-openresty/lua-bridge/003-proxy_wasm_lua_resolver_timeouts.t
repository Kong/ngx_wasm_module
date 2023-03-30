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

=== TEST 1: Lua bridge - Lua resolver can EAGAIN
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
--- response_body_like
\s+"Host": "httpbin\.org".*
--- error_log eval
[
    qr/\[debug\] .*? wasm lua resolver thread/,
    qr/\[debug\] .*? wasm lua resolver creating new dns_client/,
]
--- no_error_log
[error]



=== TEST 2: Lua bridge - on_request_headers Lua resolver can timeout during cosocket I/O
lua-resty-dns-resolver client timeout
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- http_config eval
qq{
    init_worker_by_lua_block {
        dns_client = require 'resty.dns.client'
        local opts = {
            noSynchronisation = true,
            order = { 'A' },
            hosts = { '127.0.0.1 localhost' },
            resolvConf = {
                'nameserver $::ExtResolver',
                'options timeout:1', -- 1000ms timeout
                'options attempts:1',
            }
        }
        assert(dns_client.init(opts))
        opts.timeout = 1 -- force 1ms timeout
    }
}
--- config
    location /t {
        proxy_wasm_lua_resolver on;
        proxy_wasm hostcalls 'on=request_headers \
                              test=/t/dispatch_http_call \
                              host=httpbin.org \
                              path=/headers';
        echo_sleep 0.3;
        echo failed;
    }
--- error_code: 500
--- response_body_like: 500 Internal Server Error
--- grep_error_log eval: qr/\[error\].*/
--- grep_error_log_out eval
qr/\[error\] .*? lua udp socket read timed out.*?
\[error\] .*? lua entry thread aborted: .*? wasm lua failed resolving "httpbin\.org": failed to receive reply.*?
\[error\] .*? dispatch failed: tcp socket - lua resolver failed.*?/
--- no_error_log
[crit]
[alert]



=== TEST 3: Lua bridge - on_tick Lua resolver can timeout during cosocket I/O
lua-resty-dns-resolver client timeout
--- load_nginx_modules: ngx_http_echo_module
--- main_config eval
qq{
    wasm {
        proxy_wasm_lua_resolver on;
        module hostcalls $t::TestWasm::crates/hostcalls.wasm;
    }
}
--- http_config eval
qq{
    init_worker_by_lua_block {
        dns_client = require 'resty.dns.client'
        local opts = {
            noSynchronisation = true,
            order = { 'A' },
            hosts = { '127.0.0.1 localhost' },
            resolvConf = {
                'nameserver $::ExtResolver',
                'options timeout:1', -- 1000ms timeout
                'options attempts:1',
            }
        }
        assert(dns_client.init(opts))
        opts.timeout = 1 -- force 1ms timeout
    }
}
--- config
    location /t {
        proxy_wasm_lua_resolver on;
        proxy_wasm hostcalls 'tick_period=5 \
                              on_tick=dispatch \
                              host=httpbin.org \
                              path=/headers';
        echo_sleep 0.3;
        echo ok;
    }
--- response_body
ok
--- grep_error_log eval: qr/\[error\].*/
--- grep_error_log_out eval
qr/\[error\] .*? lua udp socket read timed out.*?
\[error\] .*? lua entry thread aborted: .*? wasm lua failed resolving "httpbin\.org": failed to receive reply.*?
\[error\] .*? dispatch failed: tcp socket - lua resolver failed.*?/
--- no_error_log
[crit]
[alert]
