# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

skip_valgrind('wasmtime');

plan tests => repeat_each() * (blocks() * 4);

run_tests();

__DATA__

=== TEST 1: proxy_wasm - dispatch_http_call() https resolver + hostname
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    resolver 1.1.1.1 ipv6=off;

    location /t {
        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=httpbin.org \
                              headers=X-Thing:foo|X-Thing:bar|Hello:world \
                              use_https=yes \
                              path=/headers';
        echo fail;
    }
--- response_body_like
\s*"Hello": "world",\s*
.*?
\s*"X-Thing": "foo,bar"\s*
--- no_error_log
[error]
[crit]



=== TEST 2: proxy_wasm - dispatch_http_call() https resolver + hostname (expired certificate)
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    resolver 1.1.1.1 ipv6=off;

    location /t {
        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=expired.badssl.com \
                              use_https=yes \
                              path=/';
        echo fail;
    }
--- error_code: 500
--- response_body_like: 500 Internal Server Error
--- error_log eval
qr/\[error\] .*? dispatch failed \(SSL certificate verify error: \(10:certificate has expired\)\)/
--- no_error_log
[crit]



=== TEST 3: proxy_wasm - dispatch_http_call() https resolver + hostname (expired certificate, skip verify)
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- ssl_skip_verify: on
--- ssl_skip_host_check: on
--- config
    resolver 1.1.1.1 ipv6=off;

    location /t {
        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=expired.badssl.com \
                              use_https=yes \
                              path=/';
        echo fail;
    }
--- response_body_like: expired.<br>badssl.com
--- no_error_log
[error]
[crit]
