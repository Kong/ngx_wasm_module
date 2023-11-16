# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

plan_tests(4);
run_tests();

__DATA__

=== TEST 1: proxy_wasm - get_http_request_body() retrieves request body
--- valgrind
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'on=request_body';
        echo fail;
    }
--- request
POST /t/echo/body
Hello world
--- response_body
Hello world
--- no_error_log
[error]
[crit]



=== TEST 2: proxy_wasm - get_http_request_body() returns None until body is read
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'on=request_headers';
        echo fail;
    }
--- request
POST /t/echo/body
Hello world
--- response_body
--- no_error_log
[error]
[crit]



=== TEST 3: proxy_wasm - get_http_request_body() returns None when client_body_in_file_only
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    client_body_in_file_only on;

    location /t {
        proxy_wasm hostcalls 'on=request_body';
        echo fail;
    }
--- request
POST /t/echo/body
Hello world
--- error_code: 200
--- response_body
--- error_log eval
qr/\[error\] .*? \[wasm\] cannot read request body buffered to file at "\/.*?\/\d+"/
--- no_error_log
[crit]



=== TEST 4: proxy_wasm - get_http_request_body() returns None when no body
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'on=log';
        return 200;
    }
--- pipelined_requests eval
[
    "POST /t/log/request_body",
    "GET /t/log/request_body"
]
--- ignore_response_body
--- no_error_log eval
qr/\[info\] .*? \[wasm\] request body:/



=== TEST 5: proxy_wasm - get_http_request_body() from a subrequest with body
--- valgrind
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        internal;
        proxy_wasm hostcalls 'on=request_body';
        echo fail;
    }

    location /main {
        echo_subrequest POST '/t/echo/body' -b 'Hello from subrequest';
    }
--- request
GET /main
--- response_body
Hello from subrequest
--- no_error_log
[error]
[crit]



=== TEST 6: proxy_wasm - get_http_request_body() from a subrequest with main request body
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        internal;
        proxy_wasm hostcalls 'on=request_body';
        echo fail;
    }

    location /main {
        echo_subrequest POST '/t/echo/body';
    }
--- request
POST /main
Hello from main request body
--- response_body
Hello from main request body
--- no_error_log
[error]
[crit]



=== TEST 7: proxy_wasm - get_http_request_body() from a subrequest with no body
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        internal;
        proxy_wasm hostcalls 'on=request_body';
        echo fail;
    }

    location /main {
        echo_subrequest GET '/t/echo/body';
    }
--- request
POST /main
Hello from main request body
--- response_body
Hello from main request body
--- no_error_log
[error]
[crit]



=== TEST 8: proxy_wasm - get_http_request_body() from on_log
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'on=log';
        echo ok;
    }
--- request
POST /t/log/request_body
Hello from main request body
--- response_body
ok
--- error_log eval
qr/request body: Hello from main request body/
--- no_error_log
[error]
