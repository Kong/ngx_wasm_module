# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

skip_no_assemblyscript_sdk();

plan_tests(5);
run_tests();

__DATA__

=== TEST 1: proxy_wasm AssemblyScript SDK - addheader example
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: assemblyscript_addheader
--- config
    location /t {
        proxy_wasm assemblyscript_addheader 'wasm';
        echo ok;
    }
--- response_body
ok
--- response_headers
hello: wasm
--- no_error_log
[error]
[crit]



=== TEST 2: proxy_wasm AssemblyScript SDK - auth example, authorized
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: assemblyscript_auth
--- tcp_listen: $TEST_NGINX_SERVER_PORT2
--- tcp_reply eval
sub {
    return ["HTTP/1.1 200 OK\r\n",
            "Connection: close\r\n",
            "Content-Length: 0\r\n",
            "\r\n"];
}
--- config
    location /t {
        proxy_wasm assemblyscript_auth '127.0.0.1:$TEST_NGINX_SERVER_PORT2';
        echo ok;
    }
--- response_body
ok
--- response_headers
hello: wasm!
--- no_error_log
[error]
[crit]



=== TEST 3: proxy_wasm AssemblyScript SDK - auth example, unauthorized
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: assemblyscript_auth
--- tcp_listen: $TEST_NGINX_SERVER_PORT2
--- tcp_reply eval
sub {
    return ["HTTP/1.1 401 Unauthorized\r\n",
            "Connection: close\r\n",
            "Content-Length: 0\r\n",
            "\r\n"];
}
--- config
    location /t {
        proxy_wasm assemblyscript_auth '127.0.0.1:$TEST_NGINX_SERVER_PORT2';
        echo ok;
    }
--- error_code: 401
--- no_error_log
[error]
[crit]
[emerg]
[stub]



=== TEST 4: proxy_wasm AssemblyScript SDK - remove_headers example
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: assemblyscript_remove_headers
--- http_config
    upstream test_upstream {
        server unix:$TEST_NGINX_UNIX_SOCKET;
    }

    server {
        listen unix:$TEST_NGINX_UNIX_SOCKET;

        location / {
            add_header foo bar;
            echo ok;
        }
    }
--- config
    location /t {
        proxy_wasm assemblyscript_remove_headers 'foo';
        proxy_pass http://test_upstream/;
    }
--- response_body
ok
--- raw_response_headers_unlike: foo: bar
--- no_error_log
[error]
[crit]
