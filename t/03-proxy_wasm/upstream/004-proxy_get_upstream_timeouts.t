# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasmX;

plan tests => 31;
run_tests();

__DATA__

=== TEST 1: get_upstream_timeouts called in request_headers phase not allowed
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
        server 127.0.0.1:8891;
        wasm_upstream_select;
    }
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/get_upstream_timeouts on=request_headers';
        proxy_pass http://test_upstream/;
    }
--- error_code: 500
--- error_log
host trap (bad usage): can only get upstream timeouts during "on_upstream_*" phases
--- no_error_log
[emerg]



=== TEST 2: get_upstream_timeouts called in response_headers phase not allowed
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
        server 127.0.0.1:8891;
        wasm_upstream_select;
    }
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/get_upstream_timeouts on=response_headers';
        proxy_pass http://test_upstream/;
    }
--- error_code: 200
--- response_body
ok
--- error_log
host trap (bad usage): can only get upstream timeouts during "on_upstream_*" phases
--- no_error_log
[emerg]



=== TEST 3: get_upstream_timeouts called in request_body phase not allowed
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
        server 127.0.0.1:8891;
        wasm_upstream_select;
    }
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/get_upstream_timeouts on=request_body';
        proxy_pass http://test_upstream/;
    }
--- request
POST /t
hello
--- error_code: 500
--- error_log
host trap (bad usage): can only get upstream timeouts during "on_upstream_*" phases
--- no_error_log
[emerg]



=== TEST 4: get_upstream_timeouts called in response_body phase not allowed
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
        server 127.0.0.1:8891;
        wasm_upstream_select;
    }
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/get_upstream_timeouts on=response_body';
        proxy_pass http://test_upstream/;
    }
--- error_code: 200
--- error_log
host trap (bad usage): can only get upstream timeouts during "on_upstream_*" phases
--- no_error_log
[emerg]



=== TEST 5: get_upstream_timeouts called in on_log phase not allowed
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
        server 127.0.0.1:8891;
        wasm_upstream_select;
    }
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/get_upstream_timeouts on=log';
        proxy_pass http://test_upstream/;
    }
--- error_code: 200
--- response_body
ok
--- error_log
host trap (bad usage): can only get upstream timeouts during "on_upstream_*" phases
--- no_error_log
[emerg]



=== TEST 6: get_upstream_timeouts allowed in on_upstream_select
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
        server 127.0.0.1:8891;
        wasm_upstream_select;
    }
    proxy_connect_timeout 1;
    proxy_send_timeout 2;
    proxy_read_timeout 3;
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/get_upstream_timeouts on=upstream_select';
        proxy_pass http://test_upstream/;
    }
--- error_code: 200
--- response_body
ok
--- error_log
upstream timeouts - connect: "1000", send: "2000", read: "3000"
--- no_error_log
host trap
[emerg]



=== TEST 7: get_upstream_timeouts allowed in on_upstream_info
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
        server 127.0.0.1:8891;
        wasm_upstream_select;
    }
    proxy_connect_timeout 1;
    proxy_send_timeout 2;
    proxy_read_timeout 3;
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/set_upstream on=upstream_select ip=127.0.0.1 port=8891';
        proxy_wasm hostcalls 'test=/t/get_upstream_timeouts on=upstream_info';
        proxy_pass http://test_upstream/;
    }
--- error_code: 200
--- response_body
ok
--- error_log
upstream timeouts - connect: "1000", send: "2000", read: "3000"
--- no_error_log
host trap
[emerg]



=== TEST 8: get_upstream_timeouts allowed in on_next_upstream
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- http_config
    server {
        listen       8891;
        location / {
            return 404;
        }
    }
    proxy_next_upstream http_404;
    upstream test_upstream {
        server 127.0.0.1:8891;
        wasm_upstream_select;
    }
    proxy_connect_timeout 1;
    proxy_send_timeout 2;
    proxy_read_timeout 3;
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/set_upstream on=upstream_select ip=127.0.0.1 port=8891';
        proxy_wasm hostcalls 'test=/t/get_upstream_timeouts on=next_upstream';
        proxy_pass http://test_upstream/;
    }
--- error_code: 404
--- error_log
upstream timeouts - connect: "1000", send: "2000", read: "3000"
--- no_error_log
host trap
[emerg]
