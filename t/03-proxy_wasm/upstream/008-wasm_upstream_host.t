# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasmX;

plan tests => 22;
run_tests();

__DATA__

=== TEST 1: $wasm_upstream_host falls back to $proxy_host when plugin does not set Host
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- http_config
    server {
        listen       8891;
        location / {
            echo "got-host=$http_host";
        }
    }
    upstream test_upstream {
        server 0.0.0.0;
        wasm_upstream_select;
    }
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/set_upstream on=upstream_select ip=127.0.0.1 port=8891';
        proxy_set_header Host $wasm_upstream_host;
        proxy_pass http://test_upstream/;
    }
--- response_body
got-host=test_upstream
--- no_error_log
[error]
[crit]
[emerg]



=== TEST 2: $wasm_upstream_host uses plugin-set Host during on_upstream_select
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- http_config
    server {
        listen       8891;
        location / {
            echo "host=$http_host";
        }
    }
    upstream test_upstream {
        server 0.0.0.0;
        wasm_upstream_select;
    }
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/set_upstream on=upstream_select ip=127.0.0.1 port=8891 host=my.example.com';
        proxy_set_header Host $wasm_upstream_host;
        proxy_pass http://test_upstream/;
    }
--- response_body
host=my.example.com
--- error_log
wasm setting request header: "Host: my.example.com"
--- no_error_log
[error]
[crit]
[emerg]



=== TEST 3: plugin-set Host during on_upstream_select does not modify client-side $http_host
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- http_config
    server {
        listen       8891;
        location / {
            echo "host=$http_host";
        }
    }
    upstream test_upstream {
        server 0.0.0.0;
        wasm_upstream_select;
    }
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/set_upstream on=upstream_select ip=127.0.0.1 port=8891 host=upstream';
        proxy_set_header Host $wasm_upstream_host;
        add_header X-Client-Host $http_host;
        proxy_pass http://test_upstream/;
    }
--- more_headers
Host: client.example
--- response_headers
X-Client-Host: client.example
--- response_body
host=upstream
--- no_error_log
[error]
[crit]
[emerg]



=== TEST 4: $wasm_upstream_host without proxy_pass returns not_found
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        echo "value=[$wasm_upstream_host]";
    }
--- response_body
value=[]
--- no_error_log
[error]
[crit]
[emerg]
