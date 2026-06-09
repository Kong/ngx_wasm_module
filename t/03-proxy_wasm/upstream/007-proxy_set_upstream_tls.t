# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasmX;

plan tests => 20;
run_tests();

__DATA__

=== TEST 1: set_upstream tls=0 falls back to plain HTTP
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- http_config
    server {
        listen       8891;
        location / {
            echo "plain";
        }
    }
    upstream test_upstream {
        server 0.0.0.0;
        wasm_upstream_select;
    }
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/set_upstream on=upstream_select ip=127.0.0.1 port=8891 tls=0';
        proxy_pass http://test_upstream/;
    }
--- response_body
plain
--- error_log
[wasm] set upstream peer "127.0.0.1:8891"
--- no_error_log
upstream SSL server name
[error]
[crit]
[emerg]



=== TEST 2: set_upstream tls=1 with sni reaches HTTPS upstream
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- http_config
    server {
        listen              $TEST_NGINX_SERVER_PORT2 ssl;
        server_name         hostname;
        ssl_certificate     $TEST_NGINX_DATA_DIR/hostname_cert.pem;
        ssl_certificate_key $TEST_NGINX_DATA_DIR/hostname_key.pem;
        location / {
            echo "tls-ok";
        }
    }
    upstream test_upstream {
        server 0.0.0.0;
        wasm_upstream_select;
    }
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/set_upstream on=upstream_select ip=127.0.0.1 port=$TEST_NGINX_SERVER_PORT2 tls=1 sni=hostname';
        proxy_ssl_server_name on;
        proxy_ssl_verify off;
        proxy_pass https://test_upstream/;
    }
--- response_body
tls-ok
--- error_log eval
[
    qr/\[wasm\] set upstream peer "127\.0\.0\.1:\d+"/,
    qr/upstream SSL server name: "hostname"/,
]
--- no_error_log
[error]
[crit]
[emerg]



=== TEST 3: set_upstream tls=1 without sni still engages SSL
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- http_config
    server {
        listen              $TEST_NGINX_SERVER_PORT2 ssl;
        ssl_certificate     $TEST_NGINX_DATA_DIR/cert.pem;
        ssl_certificate_key $TEST_NGINX_DATA_DIR/key.pem;
        location / {
            echo "tls-no-sni";
        }
    }
    upstream test_upstream {
        server 0.0.0.0;
        wasm_upstream_select;
    }
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/set_upstream on=upstream_select ip=127.0.0.1 port=$TEST_NGINX_SERVER_PORT2 tls=1';
        proxy_ssl_verify off;
        proxy_pass https://test_upstream/;
    }
--- response_body
tls-no-sni
--- error_log
[wasm] set upstream peer
--- no_error_log
[error]
[crit]
[emerg]
