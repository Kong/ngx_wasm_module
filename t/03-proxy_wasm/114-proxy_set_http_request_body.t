# vim:set ft= ts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

skip_valgrind();

plan tests => repeat_each() * (blocks() * 5);

run_tests();

__DATA__

=== TEST 1: proxy_wasm - set_http_request_body() new request body is retrieved by $request_body
should set body even with GET request
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test_case=/t/set_http_request_body';
        echo $request_body;
    }
--- request
GET /t
--- response_body
Hello world
--- no_error_log
[error]
[crit]
[emerg]



=== TEST 2: proxy_wasm - set_http_request_body() new request body is retrieved by filter
should be able to then read body in http_request_headers, while typically this can only
be done on or after http_request_body.
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test_case=/t/set_http_request_body';
        proxy_wasm hostcalls 'test_case=/t/echo/body';
    }
--- request
GET /t
--- response_body
Hello world
--- no_error_log
[error]
[crit]
[emerg]



=== TEST 3: proxy_wasm - set_http_request_body() can set a request body to empty value ''
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test_case=/t/set_http_request_body value=';
        proxy_wasm hostcalls 'test_case=/t/echo/body';
    }
--- request
POST /t
Remove me
--- response_headers
Content-Length: 0
--- response_body
--- error_log
on_response_body, 0 bytes
--- no_error_log
[error]



=== TEST 4: proxy_wasm - set_http_request_body() set request body from subrequest
should be reflected in subrequest (/t/echo/body) and in main request ($request_body)
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /subrequest {
        internal;
        proxy_wasm hostcalls 'test_case=/t/set_http_request_body';
        proxy_wasm hostcalls 'test_case=/t/echo/body';
    }

    location /t {
        echo_subrequest GET '/subrequest';
        echo $request_body;
    }
--- request
GET /t
--- response_body
Hello world
Hello world
--- no_error_log
[error]
[crit]
[emerg]



=== TEST 5: proxy_wasm - set_http_request_body() with offset argument
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /reset {
        proxy_wasm hostcalls 'test_case=/t/set_http_request_body value=';
        return 200;
    }

    location /a {
        proxy_wasm hostcalls 'test_case=/t/set_http_request_body value=HelloWorld';
        proxy_wasm hostcalls 'test_case=/t/echo/body';
    }

    location /b {
        proxy_wasm hostcalls 'test_case=/t/set_http_request_body value=Wasm offset=5';
        proxy_wasm hostcalls 'test_case=/t/echo/body';
    }

    location /c {
        # offset == 0
        proxy_wasm hostcalls 'test_case=/t/set_http_request_body value=Goodbye offset=0';
        proxy_wasm hostcalls 'test_case=/t/echo/body';
    }

    location /d {
        # offset == 0
        proxy_wasm hostcalls 'test_case=/t/set_http_request_body value=HelloWorld offset=0';
        proxy_wasm hostcalls 'test_case=/t/echo/body';
    }

    location /e {
        # offset larger than buffer
        proxy_wasm hostcalls 'test_case=/t/set_http_request_body value=LAST offset=10';
        proxy_wasm hostcalls 'test_case=/t/echo/body';
    }

    location /t {
        echo_subrequest GET /a;
        echo_subrequest GET /b;
        echo_subrequest GET /c;
        echo_subrequest GET /d;
        echo_subrequest GET /reset;
        echo_subrequest GET /e;
    }
--- response_body
HelloWorld
HelloWasm
Goodbye
HelloWorld
          LAST
--- no_error_log
[error]
[crit]
[emerg]



=== TEST 6: proxy_wasm - set_http_request_body() with max argument
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /a {
        proxy_wasm hostcalls 'test_case=/t/set_http_request_body value=HelloWorld';
        proxy_wasm hostcalls 'test_case=/t/echo/body';
    }

    location /b {
        proxy_wasm hostcalls 'test_case=/t/set_http_request_body value=Wasm offset=5 max=3';
        proxy_wasm hostcalls 'test_case=/t/echo/body';
    }

    location /c {
        proxy_wasm hostcalls 'test_case=/t/set_http_request_body value=Goodbye offset=0 max=0';
        proxy_wasm hostcalls 'test_case=/t/echo/body';
    }

    location /d {
        proxy_wasm hostcalls 'test_case=/t/set_http_request_body value=HelloWorld offset=0 max=20';
        proxy_wasm hostcalls 'test_case=/t/echo/body';
    }

    location /t {
        echo_subrequest GET /a;
        echo_subrequest GET /b;
        echo_subrequest GET /c;
        echo_subrequest GET /d;
    }
--- response_body
HelloWorld
HelloWas
HelloWorld
--- no_error_log
[error]
[crit]
[emerg]
