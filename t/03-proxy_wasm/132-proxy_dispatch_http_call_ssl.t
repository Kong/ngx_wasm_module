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
                              on_http_call_response=echo_response_body \
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
                              on_http_call_response=echo_response_body \
                              use_https=yes \
                              path=/';
        echo fail;
    }
--- error_code: 500
--- response_body_like: 500 Internal Server Error
--- error_log eval
qr/\[error\] .*certificate has expired/
--- no_error_log
[crit]



=== TEST 3: proxy_wasm - dispatch_http_call() https resolver + hostname (expired certificate, skip verify)
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- tls_skip_verify: on
--- config
    resolver 1.1.1.1 ipv6=off;

    location /t {
        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=expired.badssl.com \
                              use_https=yes \
                              on_http_call_response=echo_response_body \
                              path=/';
        echo fail;
    }
--- response_body_like: expired.<br>badssl.com
--- no_error_log
[error]
[crit]



=== TEST 4: proxy_wasm - dispatch_http_call() https resolver + hostname (wrong hostname)
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    resolver 1.1.1.1 ipv6=off;

    location /t {
        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=wrong.host.badssl.com \
                              use_https=yes \
                              on_http_call_response=echo_response_body \
                              path=/';
        echo fail;
    }
--- error_code: 500
--- response_body_like: 500 Internal Server Error
--- error_log eval
qr/\[error\] .*tls certificate does not match/
--- no_error_log
[crit]



=== TEST 5: proxy_wasm - dispatch_http_call() https resolver + hostname (wrong hostname, skip host check)
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- tls_skip_host_check: on
--- config
    resolver 1.1.1.1 ipv6=off;

    location /t {
        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=wrong.host.badssl.com \
                              use_https=yes \
                              on_http_call_response=echo_response_body \
                              path=/';
        echo fail;
    }
--- response_body_like: wrong.host.<br>badssl.com
--- no_error_log
[error]
[crit]



=== TEST 6: proxy_wasm - dispatch_http_call() https resolver + hostname (untrusted root)
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    resolver 1.1.1.1 ipv6=off;

    location /t {
        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=untrusted-root.badssl.com \
                              use_https=yes \
                              on_http_call_response=echo_response_body \
                              path=/';
        echo fail;
    }
--- error_code: 500
--- response_body_like: 500 Internal Server Error
--- error_log eval
qr/\[error\]/
--- no_error_log
[crit]



=== TEST 7: proxy_wasm - dispatch_http_call() attempt https at non-https host
--- timeout_no_valgrind: 1
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=127.0.0.1:$TEST_NGINX_SERVER_PORT \
                              use_https=yes \
                              path=/dispatched';
        echo fail;
    }

    location /dispatched {
        echo -n fail;
    }
--- error_code: 500
--- response_body_like: 500 Internal Server Error
--- error_log eval
[
    qr/\[crit\] .*? SSL_do_handshake\(\) failed/,
    qr/\[error\] .*? \[wasm\] dispatch failed \(tls handshake failed\)/,
]
