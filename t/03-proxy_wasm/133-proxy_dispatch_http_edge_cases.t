# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

skip_valgrind('wasmtime');

plan tests => repeat_each() * (blocks() * 6);

run_tests();

__DATA__

=== TEST 1: proxy_wasm - dispatch_http_call() on dispatch response (1 subsequent call)
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /dispatched {
        echo "Hello world";
    }

    location /t {
        proxy_wasm hostcalls 'on=request_headers \
                              test=/t/dispatch_http_call \
                              host=127.0.0.1:$TEST_NGINX_SERVER_PORT \
                              path=/dispatched \
                              on_http_call_response=call_again';
        echo fail;
    }
--- response_headers_like
pwm-call-1: Hello world
--- response_body
called 2 times
--- no_error_log
[error]
[crit]
[emerg]



=== TEST 2: proxy_wasm - dispatch_http_call() on dispatch response (2 subsequent calls)
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /dispatched {
        echo "dispatch 1";
    }

    location /dispatched1 {
        echo "dispatch 2";
    }

    location /dispatched2 {
        echo "dispatch 3";
    }

    location /t {
        proxy_wasm hostcalls 'on=request_headers \
                              test=/t/dispatch_http_call \
                              host=127.0.0.1:$TEST_NGINX_SERVER_PORT \
                              path=/dispatched \
                              on_http_call_response=call_again \
                              ncalls=2';
        echo fail;
    }
--- response_headers_like
pwm-call-1: dispatch 1
pwm-call-2: dispatch 2
pwm-call-3: dispatch 3
--- response_body
called 3 times
--- no_error_log
[error]



=== TEST 3: proxy_wasm - dispatch_http_call() on_tick (1 call)
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /dispatched {
        echo "Hello world";
    }

    location /t {
        proxy_wasm hostcalls 'tick_period=5 \
                              on_tick=dispatch \
                              host=127.0.0.1:$TEST_NGINX_SERVER_PORT \
                              path=/dispatched';
        echo ok;
    }
--- response_body
ok
--- error_log eval
qr/on_root_http_call_response \(id: 0, headers: 5, body_bytes: 12, trailers: 0\)/
--- no_error_log
[error]
[crit]
[emerg]



=== TEST 4: proxy_wasm - dispatch_http_call() on_tick (10 calls)
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /dispatched {
        echo "Hello world";
    }

    location /t {
        proxy_wasm hostcalls 'tick_period=5 \
                              on_tick=dispatch \
                              host=127.0.0.1:$TEST_NGINX_SERVER_PORT \
                              path=/dispatched \
                              ncalls=10';
        echo ok;
    }
--- response_body
ok
--- grep_error_log eval: qr/on_root_http_call_response \(.*?\)/
--- grep_error_log_out eval
qr/\Aon_root_http_call_response \(id: 0, headers: 5, body_bytes: 12, trailers: 0\)
on_root_http_call_response \(id: 1, headers: 5, body_bytes: 12, trailers: 0\)
on_root_http_call_response \(id: 2, headers: 5, body_bytes: 12, trailers: 0\)
on_root_http_call_response \(id: 3, headers: 5, body_bytes: 12, trailers: 0\)
on_root_http_call_response \(id: 4, headers: 5, body_bytes: 12, trailers: 0\)
on_root_http_call_response \(id: 5, headers: 5, body_bytes: 12, trailers: 0\)
on_root_http_call_response \(id: 6, headers: 5, body_bytes: 12, trailers: 0\)
on_root_http_call_response \(id: 7, headers: 5, body_bytes: 12, trailers: 0\)
on_root_http_call_response \(id: 8, headers: 5, body_bytes: 12, trailers: 0\)
on_root_http_call_response \(id: 9, headers: 5, body_bytes: 12, trailers: 0\)\Z/
--- no_error_log
[error]
[crit]
[emerg]



=== TEST 5: proxy_wasm - dispatch_https_call() on_tick (SSL)
--- skip_no_debug: 4
--- load_nginx_modules: ngx_http_echo_module
--- main_config eval
qq{
    wasm {
        module hostcalls        $ENV{TEST_NGINX_CRATES_DIR}/hostcalls.wasm;
        tls_trusted_certificate $ENV{TEST_NGINX_DATA_DIR}/hostname_cert.pem;
    }
}
--- config
    listen              $TEST_NGINX_SERVER_PORT2 ssl;
    server_name         hostname;
    ssl_certificate     $TEST_NGINX_DATA_DIR/hostname_cert.pem;
    ssl_certificate_key $TEST_NGINX_DATA_DIR/hostname_key.pem;
    resolver 1.1.1.1 ipv6=off;
    resolver_add 127.0.0.1 hostname;
    location /dispatch {
        echo "Hello World";
    }
    location /t {
        proxy_wasm hostcalls 'tick_period=5 \
                              on_tick=dispatch \
                              host=hostname:$TEST_NGINX_SERVER_PORT2 \
                              https=yes \
                              path=/dispatch';
        echo ok;
    }
--- response_body
ok
--- error_log
upstream tls server name: "hostname"
on_root_http_call_response (id: 0, headers: 5, body_bytes: 12, trailers: 0)
--- no_error_log
[error]
[crit]
