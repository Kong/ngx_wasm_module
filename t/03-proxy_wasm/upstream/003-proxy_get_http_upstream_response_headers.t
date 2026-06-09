# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasmX;

plan tests => 41;
run_tests();

__DATA__

=== TEST 1: proxy_wasm - get upstream response headers in request_headers not allowed
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/log/upstream_response_headers on=request_headers';
        return 200;
    }
--- error_code: 500
--- error_log eval
qr/panicked at/
--- no_error_log
[emerg]



=== TEST 2: proxy_wasm - get upstream response headers in request_body not allowed
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/log/upstream_response_headers on=request_body';
        echo "ok";
    }
--- request
POST /t
hello world
--- error_code: 500
--- error_log eval
qr/panicked at/
--- no_error_log
[emerg]



=== TEST 3: proxy_wasm - get upstream response headers in upstream_select not allowed
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- http_config
    server {
        listen       8891;
        location / {
            echo "ok";
        }
    }
    upstream test_upstream {
        server 0.0.0.0;
        wasm_upstream_select;
    }
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/set_upstream on=upstream_select ip=127.0.0.1 port=8891';
        proxy_wasm hostcalls 'test=/t/log/upstream_response_headers on=upstream_select';
        proxy_pass http://test_upstream/;
    }
--- error_code: 500
--- error_log eval
qr/panicked at/
--- no_error_log
[emerg]



=== TEST 4: proxy_wasm - get upstream response headers in upstream_info not allowed
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- http_config
    server {
        listen       8891;
        location / {
            echo "ok";
        }
    }
    upstream test_upstream {
        server 0.0.0.0;
        wasm_upstream_select;
    }
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/set_upstream on=upstream_select ip=127.0.0.1 port=8891';
        proxy_wasm hostcalls 'test=/t/log/upstream_response_headers on=upstream_info';
        proxy_pass http://test_upstream/;
    }
--- response_body
ok
--- error_log eval
qr/panicked at/
--- no_error_log
[emerg]



=== TEST 5: proxy_wasm - get upstream response headers in response_headers not allowed
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/log/upstream_response_headers on=response_headers';
        echo "ok";
    }
--- error_log eval
qr/panicked at/
--- no_error_log
[emerg]



=== TEST 6: proxy_wasm - get upstream response headers in response_body not allowed
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/log/upstream_response_headers on=response_body';
        echo "ok";
    }
--- error_log eval
qr/panicked at/
--- no_error_log
[emerg]



=== TEST 7: proxy_wasm - get upstream response headers in log not allowed
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/log/upstream_response_headers on=log';
        return 200;
    }
--- error_log eval
qr/panicked at/
--- no_error_log
[emerg]



=== TEST 8: proxy_wasm - get upstream response headers in next_upstream
--- wasm_modules: hostcalls
--- http_config
    server {
        listen       8891;
        location / {
            return 404;
        }
    }
    proxy_next_upstream_tries 2;
    proxy_next_upstream http_404;
    upstream test_upstream {
        server 0.0.0.0;
        wasm_upstream_select;
    }
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/set_upstream on=upstream_select ip=127.0.0.1 port=8891';
        proxy_wasm hostcalls 'test=/t/log/upstream_response_headers on=next_upstream';
        proxy_pass http://test_upstream/;
    }
--- error_code: 404
--- error_log
upstream resp Server:
upstream resp Date:
upstream resp Content-Type:
upstream resp Content-Length:
upstream resp Connection:
--- no_error_log
[emerg]



=== TEST 9: proxy_wasm - get custom upstream response header
--- load_nginx_modules: ngx_http_headers_more_filter_module
--- wasm_modules: hostcalls
--- http_config
    server {
        listen       8891;
        location / {
            more_set_headers "Hello: world";
            return 404;
        }
    }
    proxy_next_upstream_tries 2;
    proxy_next_upstream http_404;
    upstream test_upstream {
        server 0.0.0.0;
        wasm_upstream_select;
    }
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/set_upstream on=upstream_select ip=127.0.0.1 port=8891';
        proxy_wasm hostcalls 'test=/t/log/upstream_response_header on=next_upstream name=hello';
        proxy_pass http://test_upstream/;
    }
--- error_code: 404
--- error_log
upstream resp header "hello: world"
--- no_error_log
[emerg]



=== TEST 10: proxy_wasm - get upstream response header for each response
--- load_nginx_modules: ngx_http_headers_more_filter_module ngx_http_echo_module
--- wasm_modules: hostcalls
--- http_config
    server {
        listen       8891;
        location / {
            more_set_headers "Hello: world1";
            return 404;
        }
    }

    server {
        listen       8892;
        location / {
            more_set_headers "Hello: world2";
            return 500;
        }
    }

    server {
        listen       8893;
        location / {
            echo "ok";
        }
    }
    proxy_next_upstream_tries 3;
    proxy_next_upstream http_404 http_500;
    upstream test_upstream {
        server 0.0.0.0;
        wasm_upstream_select;
    }
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/set_upstreams on=upstream_select upstreams=127.0.0.1:8891,127.0.0.1:8892,127.0.0.1:8893';
        proxy_wasm hostcalls 'test=/t/log/upstream_response_header on=next_upstream name=hello';
        proxy_pass http://test_upstream/;
    }
--- response_body
ok
--- error_log
on_next_upstream, status: 404
upstream resp header "hello: world1"
on_next_upstream, status: 500
upstream resp header "hello: world2"
--- no_error_log
[emerg]



=== TEST 11: proxy_wasm - get nonexisting upstream header
--- load_nginx_modules: ngx_http_headers_more_filter_module
--- wasm_modules: hostcalls
--- http_config
    server {
        listen       8891;
        location / {
            return 404;
        }
    }
    proxy_next_upstream_tries 2;
    proxy_next_upstream http_404;
    upstream test_upstream {
        server 0.0.0.0;
        wasm_upstream_select;
    }
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/set_upstream on=upstream_select ip=127.0.0.1 port=8891';
        proxy_wasm hostcalls 'test=/t/log/upstream_response_header on=next_upstream name=nonexisting';
        proxy_pass http://test_upstream/;
    }
--- error_code: 404
--- no_error_log
upstream resp header
--- no_error_log
[emerg]
