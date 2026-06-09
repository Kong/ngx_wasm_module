# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasmX;

plan tests => 46;
run_tests();

__DATA__

=== TEST 1: set_upstream_timeouts called in request_headers phase not allowed
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
        proxy_wasm hostcalls 'test=/t/set_upstream_timeouts on=request_headers connect=500 send=500 read=500';
        proxy_pass http://test_upstream/;
    }
--- error_code: 500
--- error_log
host trap (bad usage): can only set upstream timeouts during "on_upstream_select"
--- no_error_log
[emerg]



=== TEST 2: set_upstream_timeouts called in request_body phase not allowed
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
        proxy_wasm hostcalls 'test=/t/set_upstream_timeouts on=request_body connect=500 send=500 read=500';
        proxy_pass http://test_upstream/;
    }
--- request
POST /t
hello
--- error_code: 500
--- error_log
host trap (bad usage): can only set upstream timeouts during "on_upstream_select"
--- no_error_log
[emerg]



=== TEST 3: set_upstream_timeouts called in response_headers phase not allowed
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
        proxy_wasm hostcalls 'test=/t/set_upstream_timeouts on=response_headers connect=500 send=500 read=500';
        proxy_pass http://test_upstream/;
    }
--- error_code: 200
--- response_body
ok
--- error_log
host trap (bad usage): can only set upstream timeouts during "on_upstream_select"
--- no_error_log
[emerg]



=== TEST 4: set_upstream_timeouts called in response_body phase not allowed
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
        proxy_wasm hostcalls 'test=/t/set_upstream_timeouts on=response_body connect=500 send=500 read=500';
        proxy_pass http://test_upstream/;
    }
--- error_code: 200
--- error_log
host trap (bad usage): can only set upstream timeouts during "on_upstream_select"
--- no_error_log
[emerg]



=== TEST 5: set_upstream_timeouts called in on_log phase not allowed
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
        proxy_wasm hostcalls 'test=/t/set_upstream_timeouts on=log connect=500 send=500 read=500';
        proxy_pass http://test_upstream/;
    }
--- error_code: 200
--- response_body
ok
--- error_log
host trap (bad usage): can only set upstream timeouts during "on_upstream_select"
--- no_error_log
[emerg]



=== TEST 6: set_upstream_timeouts called in on_upstream_info phase not allowed
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
        proxy_wasm hostcalls 'test=/t/set_upstream on=upstream_select ip=127.0.0.1 port=8891';
        proxy_wasm hostcalls 'test=/t/set_upstream_timeouts on=upstream_info connect=500 send=500 read=500';
        proxy_pass http://test_upstream/;
    }
--- error_code: 200
--- response_body
ok
--- error_log
host trap (bad usage): can only set upstream timeouts during "on_upstream_select"
--- no_error_log
[emerg]



=== TEST 7: set_upstream_timeouts called in on_next_upstream phase not allowed
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
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/set_upstream on=upstream_select ip=127.0.0.1 port=8891';
        proxy_wasm hostcalls 'test=/t/set_upstream_timeouts on=next_upstream connect=500 send=500 read=500';
        proxy_pass http://test_upstream/;
    }
--- error_code: 500
--- error_log
host trap (bad usage): can only set upstream timeouts during "on_upstream_select"
--- no_error_log
[emerg]



=== TEST 8: set_upstream_timeouts in on_upstream_select is allowed
--- wasm_modules: hostcalls
--- http_config
    server {
        listen       8891;
        location / {
            return 200;
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
        proxy_wasm hostcalls 'test=/t/set_upstream_timeouts on=upstream_select connect=500 send=1000 read=1500';
        proxy_wasm hostcalls 'test=/t/get_upstream_timeouts on=upstream_select';
        proxy_pass http://test_upstream/;
    }
--- error_log
upstream timeouts - connect: "1000", send: "2000", read: "3000"
upstream timeouts - connect: "500", send: "1000", read: "1500"
--- no_error_log
[emerg]



=== TEST 9: set_upstream_timeouts - connect timeout set
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- http_config
    proxy_connect_timeout 10;
    proxy_send_timeout 2;
    proxy_read_timeout 3;
    upstream test_upstream {
        server 0.0.0.0;
        wasm_upstream_select;
    }
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/set_upstream on=upstream_select ip=1.1.1.1 port=80';
        proxy_wasm hostcalls 'test=/t/set_upstream_timeouts on=upstream_select connect=1';
        proxy_wasm hostcalls 'test=/t/get_upstream_timeouts on=upstream_select';
        proxy_pass http://test_upstream/;
    }
--- error_code: 504
--- error_log
upstream timeouts - connect: "1", send: "2000", read: "3000"
--- no_error_log
[emerg]



=== TEST 10: set_upstream_timeouts - read timeout set
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- http_config
    server {
        listen       8891;
        location / {
            echo_sleep 10;
            echo "ok";
        }
    }
    proxy_connect_timeout 1;
    proxy_send_timeout 2;
    proxy_read_timeout 3;
    upstream test_upstream {
        server 0.0.0.0;
        wasm_upstream_select;
    }
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/set_upstream on=upstream_select ip=127.0.0.1 port=8891';
        proxy_wasm hostcalls 'test=/t/set_upstream_timeouts on=upstream_select read=1';
        proxy_wasm hostcalls 'test=/t/get_upstream_timeouts on=upstream_select';
        proxy_pass http://test_upstream/;
    }
--- error_code: 504
--- error_log
upstream timeouts - connect: "1000", send: "2000", read: "1"
--- no_error_log
[emerg]



=== TEST 11: set_upstream_timeouts - setting to higher limits not allowed
--- wasm_modules: hostcalls
--- http_config
    server {
        listen       8891;
        location / {
            return 200;
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
        proxy_wasm hostcalls 'test=/t/set_upstream_timeouts on=upstream_select connect=5000 send=5000 read=5000';
        proxy_wasm hostcalls 'test=/t/get_upstream_timeouts on=upstream_select';
        proxy_pass http://test_upstream/;
    }
--- error_log
upstream timeouts - connect: "1000", send: "2000", read: "3000"
--- no_error_log
upstream timeouts - connect: "5000", send: "5000", read: "5000"
[emerg]



=== TEST 12: set_upstream_timeouts - rotating timeouts between retries
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- http_config
    server {
        listen       8891;
        location / {
            echo_sleep 10;
            echo "ok";
        }
    }
    server {
        listen       8892;
        location / {
            echo "ok";
        }
    }
    proxy_next_upstream_tries 3;
    proxy_next_upstream error timeout;
    proxy_connect_timeout 10;
    proxy_send_timeout 10;
    proxy_read_timeout 10;
    upstream test_upstream {
        server 0.0.0.0;
        wasm_upstream_select;
    }
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/set_upstreams on=upstream_select upstreams=1.1.1.1:80,127.0.0.1:8891,127.0.0.1:8892';
        proxy_wasm hostcalls 'test=/t/set_upstream_timeouts_rotate on=upstream_select values=1:10000:10000,10000:10000:1,10000:10000:10000';
        proxy_wasm hostcalls 'test=/t/get_upstream_timeouts on=upstream_select';
        proxy_pass http://test_upstream/;
    }
--- response_body
ok
--- grep_error_log eval: qr/upstream timeouts - connect: "\d+", send: "\d+", read: "\d+"|on_upstream_info, last_state: \w+/
--- grep_error_log_out
upstream timeouts - connect: "1", send: "10000", read: "10000"
on_upstream_info, last_state: Failed
on_upstream_info, last_state: Failed
on_upstream_info, last_state: Failed
upstream timeouts - connect: "10000", send: "10000", read: "1"
on_upstream_info, last_state: Failed
on_upstream_info, last_state: Failed
on_upstream_info, last_state: Failed
upstream timeouts - connect: "10000", send: "10000", read: "10000"
on_upstream_info, last_state: Ok
on_upstream_info, last_state: Ok
on_upstream_info, last_state: Ok
--- no_error_log
[emerg]



=== TEST 13: set_upstream_timeouts - does not affect subsequent requests
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- http_config
    server {
        listen       8891;
        location / {
            echo_sleep 0.5;
            echo "ok";
        }
    }
    upstream test_upstream {
        server 0.0.0.0;
        wasm_upstream_select;
    }
    proxy_connect_timeout 10;
    proxy_send_timeout 10;
    proxy_read_timeout 10;
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/set_upstream on=upstream_select ip=127.0.0.1 port=8891';
        proxy_wasm hostcalls 'test=/t/set_upstream_timeouts_if_header on=upstream_select header=X-Set-Timeout read=1';
        proxy_pass http://test_upstream/;
    }
--- request eval
["GET /t", "GET /t"]
--- more_headers eval
["X-Set-Timeout: yes", ""]
--- error_code eval
[504, 200]
--- no_error_log
[emerg]
