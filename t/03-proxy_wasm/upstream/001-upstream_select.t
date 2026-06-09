# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasmX;

plan tests => 271;
run_tests();

__DATA__

=== TEST 1: duplicate wasm_upstream_select directive
--- wasm_modules: on_phases
--- http_config
    upstream test_upstream {
        server 127.0.0.1:8001;
        wasm_upstream_select;
        wasm_upstream_select;
    }
--- config
    location /t {
        proxy_pass http://test_upstream/;
    }
--- error_log eval
qr/\[emerg\] .*? "wasm_upstream_select" directive is duplicate/
--- no_error_log
[error]
[crit]
--- must_die



=== TEST 2: wasm_upstream_select - no wasm{} configuration block
--- main_config
--- http_config
    upstream test_upstream {
        server 127.0.0.1:8001;
        wasm_upstream_select;
    }
--- config
    location /t {
        proxy_pass http://test_upstream/;
    }
--- error_log eval
qr/\[emerg\] .*? "wasm_upstream_select" directive is specified but config has no "wasm" section/
--- no_error_log
[error]
[crit]
--- must_die



=== TEST 3: wasm_upstream_select outside upstream{} configuration block
--- wasm_modules: on_phases
--- config
    location /t {
        wasm_upstream_select;
        return 200;
    }
--- error_log eval
qr/\[emerg\] .*? "wasm_upstream_select" directive is not allowed here/
--- no_error_log
[error]
[crit]
--- must_die



=== TEST 4: wasm_upstream_select no plugin - fallback to round robin
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: on_phases
--- http_config
    server {
        listen       8891;
        location / {
            echo "1";
        }
    }
    server {
        listen       8892;
        location / {
            echo "2";
        }
    }
    server {
        listen       8893;
        location / {
            echo "3";
        }
    }
    server {
        listen       8894;
        location / {
            echo "4";
        }
    }
    upstream test_upstream {
        server 127.0.0.1:8891;
        server 127.0.0.1:8892;
        server 127.0.0.1:8893;
        server 127.0.0.1:8894;
        wasm_upstream_select;
    }
--- config
    location /t {
        proxy_pass http://test_upstream/;
    }
--- request eval
["GET /t", "GET /t", "GET /t", "GET /t"]
--- response_body eval
["1\n", "2\n", "3\n", "4\n"]
--- no_error_log
[error]
[crit]
[emerg]



=== TEST 5: plugin with no upstream_select - fallback to round robin
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: instance_lifecycle
--- http_config
    server {
        listen       8891;
        location / {
            echo "1";
        }
    }
    server {
        listen       8892;
        location / {
            echo "2";
        }
    }
    server {
        listen       8893;
        location / {
            echo "3";
        }
    }
    server {
        listen       8894;
        location / {
            echo "4";
        }
    }
    upstream test_upstream {
        server 127.0.0.1:8891;
        server 127.0.0.1:8892;
        server 127.0.0.1:8893;
        server 127.0.0.1:8894;
        wasm_upstream_select;
    }
--- config
    location /t {
        proxy_wasm instance_lifecycle;
        proxy_pass http://test_upstream/;
    }
--- request eval
["GET /t", "GET /t", "GET /t", "GET /t"]
--- response_body eval
["1\n", "2\n", "3\n", "4\n"]
--- error_log
[wasm] calling original get_peer
--- no_error_log
[error]
[crit]
[emerg]



=== TEST 6: plugin does not choose upstream - fallback to round robin
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: on_phases
--- http_config
    server {
        listen       8891;
        location / {
            echo "1";
        }
    }
    server {
        listen       8892;
        location / {
            echo "2";
        }
    }
    server {
        listen       8893;
        location / {
            echo "3";
        }
    }
    server {
        listen       8894;
        location / {
            echo "4";
        }
    }
    upstream test_upstream {
        server 127.0.0.1:8891;
        server 127.0.0.1:8892;
        server 127.0.0.1:8893;
        server 127.0.0.1:8894;
        wasm_upstream_select;
    }
--- config
    location /t {
        proxy_wasm on_phases;
        proxy_pass http://test_upstream/;
    }
--- request eval
["GET /t", "GET /t", "GET /t", "GET /t"]
--- response_body eval
["1\n", "2\n", "3\n", "4\n"]
--- error_log
on_upstream_select
[wasm] no upstream selected in "on_upstream_select"
[wasm] calling original get_peer
--- no_error_log
[error]
[crit]
[emerg]



=== TEST 7: set_upstream called in request_headers phase not allowed
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- http_config
    server {
        listen       8891;
        location / {
            echo "ok";
        }
    }
    server {
        listen       8892;
        location / {
            echo "original";
        }
    }
    upstream test_upstream {
        server 127.0.0.1:8892;
        wasm_upstream_select;
    }
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/set_upstream on=request_headers ip=127.0.0.1 port=8891';
        proxy_pass http://test_upstream/;
    }
--- error_code: 500
--- error_log
host trap (bad usage): can only set upstream during "on_upstream_select"
--- no_error_log
[emerg]



=== TEST 8: set_upstream called in request_body phase not allowed
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- http_config
    server {
        listen       8891;
        location / {
            echo "ok";
        }
    }
    server {
        listen       8892;
        location / {
            echo "original";
        }
    }
    upstream test_upstream {
        server 127.0.0.1:8892;
        wasm_upstream_select;
    }
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/set_upstream on=request_body ip=127.0.0.1 port=8891';
        proxy_pass http://test_upstream/;
    }
--- request
POST /t
hello
--- error_code: 500
--- error_log
host trap (bad usage): can only set upstream during "on_upstream_select"
--- no_error_log
[emerg]



=== TEST 9: set_upstream called in response_headers phase not allowed
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- http_config
    server {
        listen       8891;
        location / {
            echo "ok";
        }
    }
    server {
        listen       8892;
        location / {
            echo "original";
        }
    }
    upstream test_upstream {
        server 127.0.0.1:8892;
        wasm_upstream_select;
    }
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/set_upstream on=response_headers ip=127.0.0.1 port=8891';
        proxy_pass http://test_upstream/;
    }
--- error_code: 200
--- response_body
original
--- error_log
host trap (bad usage): can only set upstream during "on_upstream_select"
--- no_error_log
[emerg]



=== TEST 10: set_upstream called in response_body phase not allowed
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- http_config
    server {
        listen       8891;
        location / {
            echo "ok";
        }
    }
    server {
        listen       8892;
        location / {
            echo "original";
        }
    }
    upstream test_upstream {
        server 127.0.0.1:8892;
        wasm_upstream_select;
    }
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/set_upstream on=response_body ip=127.0.0.1 port=8891';
        proxy_pass http://test_upstream/;
    }
--- error_code: 200
--- response_body
original
--- error_log
host trap (bad usage): can only set upstream during "on_upstream_select"
--- no_error_log
[emerg]



=== TEST 11: set_upstream called in on_upstream_info phase not allowed
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- http_config
    server {
        listen       8891;
        location / {
            echo "ok";
        }
    }
    server {
        listen       8892;
        location / {
            echo "original";
        }
    }
    upstream test_upstream {
        server 127.0.0.1:8892;
        wasm_upstream_select;
    }
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/set_upstream on=upstream_select,upstream_info ip=127.0.0.1 port=8891';
        proxy_pass http://test_upstream/;
    }
--- error_code: 200
--- response_body
ok
--- error_log
host trap (bad usage): can only set upstream during "on_upstream_select"
--- no_error_log
[emerg]



=== TEST 12: set_upstream in on_upstream_select sets upstream server
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
        proxy_pass http://test_upstream/;
    }
--- response_body
ok
--- error_log
[wasm] set upstream peer "127.0.0.1:8891"
--- no_error_log
[error]
[crit]
[emerg]



=== TEST 13: set_upstream IPv6 with brackets sets upstream server
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- http_config
    server {
        listen       [::1]:8891;
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
        proxy_wasm hostcalls 'test=/t/set_upstream on=upstream_select ip=[::1] port=8891';
        proxy_pass http://test_upstream/;
    }
--- response_body
ok
--- error_log
[wasm] set upstream peer "[::1]:8891"
--- no_error_log
[error]
[crit]
[emerg]



=== TEST 14: set_upstream IPv6 no brackets sets upstream server
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- http_config
    server {
        listen       [::1]:8891;
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
        proxy_wasm hostcalls 'test=/t/set_upstream on=upstream_select ip=::1 port=8891';
        proxy_pass http://test_upstream/;
    }
--- response_body
ok
--- error_log
[wasm] set upstream peer "[::1]:8891"
--- no_error_log
[error]
[crit]
[emerg]



=== TEST 15: set_upstream invalid port
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- http_config
    upstream test_upstream {
        server 127.0.0.1:8892;
        wasm_upstream_select;
    }
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/set_upstream on=upstream_select ip=127.0.0.1 port=70000';
        proxy_pass http://test_upstream/;
    }
--- error_code: 500
--- error_log
[wasm] proxy_set_upstream: port out of range: 70000
--- no_error_log
[emerg]



=== TEST 16: set_upstream empty ip
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- http_config
    upstream test_upstream {
        server 127.0.0.1:8892;
        wasm_upstream_select;
    }
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/set_upstream on=upstream_select ip= port=8891';
        proxy_pass http://test_upstream/;
    }
--- error_code: 500
--- error_log
[wasm] invalid upstream address ""
--- no_error_log
[emerg]



=== TEST 17: set_upstream hostname as ip
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- http_config
    upstream test_upstream {
        server 127.0.0.1:8892;
        wasm_upstream_select;
    }
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/set_upstream on=upstream_select ip=example.com port=8891';
        proxy_pass http://test_upstream/;
    }
--- error_code: 500
--- error_log
[wasm] invalid upstream address "example.com"
--- no_error_log
[emerg]



=== TEST 18: set_upstream invalid characters in ip
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- http_config
    upstream test_upstream {
        server 127.0.0.1:8892;
        wasm_upstream_select;
    }
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/set_upstream on=upstream_select ip=@!#$% port=8891';
        proxy_pass http://test_upstream/;
    }
--- error_code: 500
--- error_log
[wasm] invalid upstream address "@!#$%"
--- no_error_log
[emerg]



=== TEST 19: set_upstream called twice not allowed
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- http_config
    upstream test_upstream {
        server 127.0.0.1:8892;
        wasm_upstream_select;
    }
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/set_upstream on=upstream_select ip=127.0.0.1 port=8891';
        proxy_wasm hostcalls 'test=/t/set_upstream on=upstream_select ip=127.0.0.1 port=8892';
        proxy_pass http://test_upstream/;
    }
--- error_code: 500
--- error_log
[wasm] upstream "127.0.0.1:8891" already set, overwriting not allowed
--- no_error_log
[emerg]



=== TEST 20: on_upstream_info state ok
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
        proxy_pass http://test_upstream/;
    }
--- error_code: 200
--- response_body
ok
--- error_log
[wasm] set upstream peer "127.0.0.1:8891"
[hostcalls] on_upstream_info, last_state: Ok
--- no_error_log
[error]
[crit]
[emerg]



=== TEST 21: on_upstream_info state Failed
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- http_config
    upstream test_upstream {
        server 0.0.0.0;
        wasm_upstream_select;
    }
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/set_upstream on=upstream_select ip=127.0.0.1 port=9999';
        proxy_pass http://test_upstream/;
    }
--- error_code: 502
--- error_log
[wasm] set upstream peer "127.0.0.1:9999"
[hostcalls] on_upstream_info, last_state: Failed
--- no_error_log
[emerg]



=== TEST 22: proxy_next_upstream_tries overrides number of servers in upstream block used as tries
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- http_config
    proxy_next_upstream_tries 3;
    upstream test_upstream {
        server 0.0.0.0;
        wasm_upstream_select;
    }
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/set_upstream on=upstream_select ip=127.0.0.1 port=9999';
        proxy_pass http://test_upstream/;
    }
--- error_code: 502
--- grep_error_log eval: qr/\[wasm\] set upstream peer "[^"]*"|\[hostcalls\] on_upstream_info, last_state: \w+/
--- grep_error_log_out
[wasm] set upstream peer "127.0.0.1:9999"
[hostcalls] on_upstream_info, last_state: Failed
[wasm] set upstream peer "127.0.0.1:9999"
[hostcalls] on_upstream_info, last_state: Failed
[wasm] set upstream peer "127.0.0.1:9999"
[hostcalls] on_upstream_info, last_state: Failed
--- no_error_log
[emerg]



=== TEST 23: on_upstream_select called for retries
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- http_config
    server {
        listen       8891;
        location / {
            return 500;
        }
    }
    server {
        listen       8892;
        location / {
            return 404;
        }
    }
    server {
        listen       8893;
        location / {
            echo "ok";
        }
    }
    proxy_next_upstream_tries 3;
    proxy_next_upstream http_500 http_404;
    upstream test_upstream {
        server 0.0.0.0;
        wasm_upstream_select;
    }
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/set_upstreams on=upstream_select upstreams=127.0.0.1:8891,127.0.0.1:8892,127.0.0.1:8893';
        proxy_pass http://test_upstream/;
    }
--- response_body
ok
--- grep_error_log eval: qr/\[wasm\] set upstream peer "[^"]*"|\[hostcalls\] on_upstream_info, last_state: \w+/
--- grep_error_log_out
[wasm] set upstream peer "127.0.0.1:8891"
[hostcalls] on_upstream_info, last_state: Failed
[wasm] set upstream peer "127.0.0.1:8892"
[hostcalls] on_upstream_info, last_state: Failed
[wasm] set upstream peer "127.0.0.1:8893"
[hostcalls] on_upstream_info, last_state: Ok
--- no_error_log
[emerg]



=== TEST 24: runs out of retries last response used
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- http_config
    server {
        listen       8891;
        location / {
            return 500;
        }
    }
    server {
        listen       8892;
        location / {
            return 404;
        }
    }
    server {
        listen       8893;
        location / {
            echo "ok";
        }
    }
    proxy_next_upstream_tries 2;
    proxy_next_upstream http_500 http_404;
    upstream test_upstream {
        server 0.0.0.0;
        wasm_upstream_select;
    }
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/set_upstreams on=upstream_select upstreams=127.0.0.1:8891,127.0.0.1:8892,127.0.0.1:8893';
        proxy_pass http://test_upstream/;
    }
--- error_code: 404
--- grep_error_log eval: qr/\[wasm\] set upstream peer "[^"]*"|\[hostcalls\] on_upstream_info, last_state: \w+/
--- grep_error_log_out
[wasm] set upstream peer "127.0.0.1:8891"
[hostcalls] on_upstream_info, last_state: Failed
[wasm] set upstream peer "127.0.0.1:8892"
[hostcalls] on_upstream_info, last_state: Failed
--- no_error_log
[emerg]



=== TEST 25: on_next_upstream called after special response before retry
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- http_config
    server {
        listen       8891;
        location / {
            return 500;
        }
    }
    server {
        listen       8892;
        location / {
            echo "ok";
        }
    }
    proxy_next_upstream_tries 2;
    proxy_next_upstream http_500;
    upstream test_upstream {
        server 0.0.0.0;
        wasm_upstream_select;
    }
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/set_upstreams on=upstream_select upstreams=127.0.0.1:8891,127.0.0.1:8892';
        proxy_pass http://test_upstream/;
    }
--- response_body
ok
--- grep_error_log eval: qr/\[wasm\] set upstream peer "[^"]*"|\[hostcalls\] on_upstream_info, last_state: \w+|\[hostcalls\] on_next_upstream, status: \w+/
--- grep_error_log_out
[wasm] set upstream peer "127.0.0.1:8891"
[hostcalls] on_next_upstream, status: 500
[hostcalls] on_upstream_info, last_state: Failed
[wasm] set upstream peer "127.0.0.1:8892"
[hostcalls] on_upstream_info, last_state: Ok
--- no_error_log
[emerg]



=== TEST 26: connection refused - upstream_info failed, no next_upstream called
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- http_config
    server {
        listen       8891;
        location / {
            echo "ok";
        }
    }
    proxy_next_upstream_tries 2;
    upstream test_upstream {
        server 0.0.0.0;
        wasm_upstream_select;
    }
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/set_upstreams on=upstream_select upstreams=127.0.0.1:9999,127.0.0.1:8891';
        proxy_pass http://test_upstream/;
    }
--- response_body
ok
--- error_log
[wasm] set upstream peer "127.0.0.1:9999"
[hostcalls] on_upstream_info, last_state: Failed
[wasm] set upstream peer "127.0.0.1:8891"
[hostcalls] on_upstream_info, last_state: Ok
--- no_error_log
[wasm] accepting upstream response
[emerg]



=== TEST 27: connection timeout - upstream_info failed, no next_upstream called
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- http_config
    server {
        listen       8891;
        location / {
            echo "ok";
        }
    }
    proxy_next_upstream_tries 2;
    proxy_connect_timeout 1s;
    upstream test_upstream {
        server 0.0.0.0;
        wasm_upstream_select;
    }
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/set_upstreams on=upstream_select upstreams=192.0.2.1:12345,127.0.0.1:8891';
        proxy_pass http://test_upstream/;
    }
--- response_body
ok
--- error_log
[wasm] set upstream peer "192.0.2.1:12345"
[hostcalls] on_upstream_info, last_state: Failed
[wasm] set upstream peer "127.0.0.1:8891"
[hostcalls] on_upstream_info, last_state: Ok
--- no_error_log
[wasm] accepting upstream response
[emerg]



=== TEST 28: read timeout - upstream_info failed
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- http_config
    server {
        listen       8891;
        location / {
            echo_sleep 2;
            echo "ok";
        }
    }
    proxy_next_upstream_tries 1;
    proxy_read_timeout 1s;
    upstream test_upstream {
        server 0.0.0.0;
        wasm_upstream_select;
    }
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/set_upstream on=upstream_select ip=127.0.0.1 port=8891';
        proxy_pass http://test_upstream/;
    }
--- error_code: 504
--- error_log
[hostcalls] on_upstream_info, last_state: Failed
--- no_error_log
[emerg]



=== TEST 29: on_next_upstream not called after normal response
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- http_config
    server {
        listen       8891;
        location / {
            echo "ok";
        }
    }
    proxy_next_upstream_tries 2;
    proxy_next_upstream http_500;
    upstream test_upstream {
        server 0.0.0.0;
        wasm_upstream_select;
    }
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/set_upstream on=upstream_select ip=127.0.0.1 port=8891';
        proxy_pass http://test_upstream/;
    }
--- response_body
ok
--- error_log
[wasm] set upstream peer "127.0.0.1:8891"
[hostcalls] on_upstream_info, last_state: Ok
--- no_error_log
[wasm] accepting upstream response
[emerg]



=== TEST 30: accept_upstream_response results in no more tries and response sent
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- http_config
    server {
        listen       8891;
        location / {
            return 404;
        }
    }
    server {
        listen       8892;
        location / {
            echo "ok";
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
        proxy_wasm hostcalls 'test=/t/set_upstreams on=upstream_select upstreams=127.0.0.1:8891,127.0.0.1:8892';
        proxy_wasm hostcalls 'test=/t/accept_response on=next_upstream status=404';
        proxy_pass http://test_upstream/;
    }
--- error_code: 404
--- error_log
[wasm] set upstream peer "127.0.0.1:8891"
[hostcalls] on_next_upstream, status: 404
[wasm] accepting upstream response with status: 404
[hostcalls] on_upstream_info, last_state: Ok
--- no_error_log
[emerg]



=== TEST 31: accept_upstream_response called from on_upstream_select not allowed
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
        proxy_wasm hostcalls 'test=/t/accept_response on=upstream_select';
        proxy_pass http://test_upstream/;
    }
--- error_code: 500
--- error_log
host trap (bad usage): can only accept response during "on_next_upstream"
--- no_error_log
[emerg]



=== TEST 32: accept_upstream_response called from on_upstream_info not allowed
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
        proxy_wasm hostcalls 'test=/t/accept_response on=upstream_info';
        proxy_pass http://test_upstream/;
    }
--- error_code: 200
--- response_body
ok
--- error_log
host trap (bad usage): can only accept response during "on_next_upstream"
--- no_error_log
[emerg]



=== TEST 33: accept_upstream_response called from request_headers not allowed
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
        proxy_wasm hostcalls 'test=/t/accept_response on=request_headers';
        proxy_pass http://test_upstream/;
    }
--- error_code: 500
--- error_log
host trap (bad usage): can only accept response during "on_next_upstream"
--- no_error_log
[emerg]



=== TEST 34: trap in on_upstream_select
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
        proxy_wasm hostcalls 'test=/t/trap on=upstream_select';
        proxy_pass http://test_upstream/;
    }
--- error_code: 500
--- error_log eval
[
    qr/\[crit\] .*? panicked at/,
    qr/custom trap/,
]
--- no_error_log
[emerg]



=== TEST 35: trap in on_next_upstream
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- http_config
    server {
        listen       8891;
        location / {
            return 404;
        }
    }
    server {
        listen       8892;
        location / {
            echo "ok";
        }
    }
    proxy_next_upstream_tries 2;
    proxy_next_upstream http_404;
    upstream test_upstream {
        server 127.0.0.1:8891;
        wasm_upstream_select;
    }
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/set_upstreams on=upstream_select upstreams=127.0.0.1:8891,127.0.0.1:8892';
        proxy_wasm hostcalls 'test=/t/trap on=next_upstream';
        proxy_pass http://test_upstream/;
    }
--- error_code: 500
--- error_log eval
[
    qr/\[crit\] .*? panicked at/,
    qr/custom trap/,
]
--- no_error_log
[emerg]



=== TEST 36: trap in on_upstream_info ignored
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
        proxy_wasm hostcalls 'test=/t/trap on=upstream_info';
        proxy_pass http://test_upstream/;
    }
--- error_log eval
[
    qr/\[crit\] .*? panicked at/,
    qr/custom trap/,
]
--- no_error_log
[emerg]



=== TEST 37: send_local_response in on_upstream_select
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- http_config
    upstream test_upstream {
        server 127.0.0.1:8891;
        wasm_upstream_select;
    }
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/send_local_response/status/204 on=upstream_select';
        proxy_pass http://test_upstream/;
    }
--- error_code: 204
--- no_error_log
[emerg]



=== TEST 38: modify path in on_upstream_select
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- http_config
    server {
        listen       8891;
        location /test {
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
        proxy_wasm hostcalls 'test=/t/set_request_header on=upstream_select name=:path value=/test';
        proxy_pass http://test_upstream/;
    }
--- response_body
ok
--- no_error_log
[emerg]



=== TEST 39: modify method in on_upstream_select
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- http_config
    server {
        listen       8891;
        location / {
            echo $request_method;
        }
    }
    upstream test_upstream {
        server 127.0.0.1:8891;
        wasm_upstream_select;
    }
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/set_upstream on=upstream_select ip=127.0.0.1 port=8891';
        proxy_wasm hostcalls 'test=/t/set_request_header on=upstream_select name=:method value=POST';
        proxy_pass http://test_upstream/;
    }
--- response_body
POST
--- no_error_log
[emerg]



=== TEST 40: add new header in on_upstream_select
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- http_config
    server {
        listen       8891;
        location / {
            echo $http_custom_header;
        }
    }
    upstream test_upstream {
        server 0.0.0.0;
        wasm_upstream_select;
    }
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/set_upstream on=upstream_select ip=127.0.0.1 port=8891';
        proxy_wasm hostcalls 'test=/t/set_request_header on=upstream_select name=custom-header value=custom-value';
        proxy_pass http://test_upstream/;
    }
--- response_body
custom-value
--- no_error_log
[emerg]



=== TEST 41: modify request path between retries
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- http_config
    server {
        listen       8891;
        location /test1 {
            return 500;
        }

        location /test2 {
            return 404;
        }

        location /test3 {
            echo "ok";
        }
    }
    proxy_next_upstream_tries 3;
    proxy_next_upstream http_500 http_404;
    upstream test_upstream {
        server 0.0.0.0;
        wasm_upstream_select;
    }
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/set_upstream on=upstream_select ip=127.0.0.1 port=8891';
        proxy_wasm hostcalls 'test=/t/set_request_header_rotate on=upstream_select name=:path values=/test1,/test2,/test3';
        proxy_pass http://test_upstream/;
    }
--- response_body
ok
--- error_log
on_next_upstream, status: 500
on_next_upstream, status: 404
--- no_error_log
[emerg]



=== TEST 42: request body preserved after header modification in on_upstream_select
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- http_config
    server {
        listen       8891;

        location /test1 {
            return 500;
        }

        location /test2 {
            return 404;
        }

        location /test3 {
            echo_read_request_body;
            echo_request_body;
        }
    }
    proxy_next_upstream_tries 3;
    proxy_next_upstream http_500 http_404 non_idempotent;
    upstream test_upstream {
        server 0.0.0.0;
        wasm_upstream_select;
    }
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/set_upstream on=upstream_select ip=127.0.0.1 port=8891';
        proxy_wasm hostcalls 'test=/t/set_request_header_rotate on=upstream_select name=:path values=/test1,/test2,/test3';
        proxy_pass http://test_upstream/;
    }
--- request
POST /t
hello world
--- response_body eval
"hello world"
--- no_error_log
[emerg]



=== TEST 43: wasm_upstream_select with keepalive - connection reused
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- http_config
    server {
        listen       8891;

        location / {
            return 200;
        }
    }
    upstream test_upstream {
        server 0.0.0.0;
        wasm_upstream_select;
        keepalive 32;
    }
--- config
    location /t {
        proxy_http_version 1.1;
        proxy_set_header Connection "";
        proxy_wasm hostcalls 'test=/t/set_upstream on=upstream_select ip=127.0.0.1 port=8891';
        proxy_pass http://test_upstream/;
    }
--- request eval
["GET /t", "GET /t"]
--- grep_error_log eval
qr/(free|get) keepalive peer: (saving|using) connection/
--- grep_error_log_out eval
["free keepalive peer: saving connection\n",
"get keepalive peer: using connection\nfree keepalive peer: saving connection\n"]
--- no_error_log
[emerg]



=== TEST 44: wasm_next_upstream with keepalive - no connection reused
--- load_nginx_modules: ngx_http_echo_module
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
        keepalive 32;
    }
--- config
    location /t {
        proxy_http_version 1.1;
        proxy_set_header Connection "";
        proxy_wasm hostcalls 'test=/t/set_upstream on=upstream_select ip=127.0.0.1 port=8891';
        proxy_pass http://test_upstream/;
    }
--- error_code: 404
--- error_log
on_next_upstream, status: 404
--- no_error_log
[emerg]



=== TEST 45: wasm_upstream_select invalid argument
--- http_config
    upstream test_upstream {
        server 0.0.0.0;
        wasm_upstream_select foo=2;
    }
--- error_log eval
qr/\[emerg\] .*? invalid parameter "foo=2"/
--- must_die



=== TEST 46: wasm_upstream_select max_tries argument invalid value (1/2)
--- http_config
    upstream test_upstream {
        server 0.0.0.0;
        wasm_upstream_select max_tries=abc;
    }
--- error_log eval
qr/\[emerg\] .*? invalid max_tries value/
--- must_die



=== TEST 47: wasm_upstream_select max_tries argument invalid value (2/2)
--- http_config
    upstream test_upstream {
        server 0.0.0.0;
        wasm_upstream_select max_tries=-3;
    }
--- error_log eval
qr/\[emerg\] .*? invalid max_tries value/
--- must_die



=== TEST 48: wasm_upstream_select max_tries argument
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
        server 0.0.0.0;
        wasm_upstream_select max_tries=2;
    }
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/set_upstream on=upstream_select ip=127.0.0.1 port=8891';
        proxy_pass http://test_upstream/;
    }
--- error_code: 404
--- grep_error_log eval: qr/\[wasm\] set upstream peer "127\.0\.0\.1:8891"/
--- grep_error_log_out
[wasm] set upstream peer "127.0.0.1:8891"
[wasm] set upstream peer "127.0.0.1:8891"
--- no_error_log
[emerg]



=== TEST 49: wasm_upstream_select max_tries argument defaults to 3
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
        server 0.0.0.0;
        wasm_upstream_select;
    }
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/set_upstream on=upstream_select ip=127.0.0.1 port=8891';
        proxy_pass http://test_upstream/;
    }
--- error_code: 404
--- grep_error_log eval: qr/\[wasm\] set upstream peer "127\.0\.0\.1:8891"/
--- grep_error_log_out
[wasm] set upstream peer "127.0.0.1:8891"
[wasm] set upstream peer "127.0.0.1:8891"
[wasm] set upstream peer "127.0.0.1:8891"
--- no_error_log
[emerg]



=== TEST 50: tries set to min of max_tries, proxy_next_upstream_tries (1/2)
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
        proxy_pass http://test_upstream/;
    }
--- error_code: 404
--- grep_error_log eval: qr/\[wasm\] set upstream peer "127\.0\.0\.1:8891"/
--- grep_error_log_out
[wasm] set upstream peer "127.0.0.1:8891"
[wasm] set upstream peer "127.0.0.1:8891"
--- no_error_log
[emerg]



=== TEST 51: tries set to min of max_tries, proxy_next_upstream_tries (2/2)
--- wasm_modules: hostcalls
--- http_config
    server {
        listen       8891;

        location / {
            return 404;
        }
    }
    proxy_next_upstream_tries 3;
    proxy_next_upstream http_404;
    upstream test_upstream {
        server 0.0.0.0;
        wasm_upstream_select max_tries=2;
    }
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/set_upstream on=upstream_select ip=127.0.0.1 port=8891';
        proxy_pass http://test_upstream/;
    }
--- error_code: 404
--- grep_error_log eval: qr/\[wasm\] set upstream peer "127\.0\.0\.1:8891"/
--- grep_error_log_out
[wasm] set upstream peer "127.0.0.1:8891"
[wasm] set upstream peer "127.0.0.1:8891"
--- no_error_log
[emerg]



=== TEST 52: max_tries ignored when fallback to round robin
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: on_phases
--- http_config
    server {
        listen       8891;
        location / {
            return 500;
        }
    }
    server {
        listen       8892;
        location / {
            return 500;
        }
    }
    server {
        listen       8893;
        location / {
            return 500;
        }
    }
    server {
        listen       8894;
        location / {
            echo "ok";
        }
    }
    proxy_next_upstream http_500;
    upstream test_upstream {
        server 127.0.0.1:8891;
        server 127.0.0.1:8892;
        server 127.0.0.1:8893;
        server 127.0.0.1:8894;
        wasm_upstream_select max_tries=1;
    }
--- config
    location /t {
        proxy_pass http://test_upstream/;
    }
--- response_body
ok
--- no_error_log
[error]
[crit]
[emerg]



=== TEST 53: fallback to round robin, proxy_next_upstream limits tries
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: on_phases
--- http_config
    server {
        listen       8891;
        location / {
            return 500;
        }
    }
    server {
        listen       8892;
        location / {
            return 500;
        }
    }
    server {
        listen       8893;
        location / {
            return 500;
        }
    }
    server {
        listen       8894;
        location / {
            echo "ok";
        }
    }
    proxy_next_upstream_tries 2;
    proxy_next_upstream http_500;
    upstream test_upstream {
        server 127.0.0.1:8891;
        server 127.0.0.1:8892;
        server 127.0.0.1:8893;
        server 127.0.0.1:8894;
        wasm_upstream_select;
    }
--- config
    location /t {
        proxy_pass http://test_upstream/;
    }
--- error_code: 500
--- no_error_log
[error]
[crit]
