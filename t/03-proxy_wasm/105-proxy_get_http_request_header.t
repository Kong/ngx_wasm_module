# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

skip_valgrind();

plan tests => repeat_each() * (blocks() * 8);

run_tests();

__DATA__

=== TEST 1: proxy_wasm - get_http_request_header() retrieves case insensitive header
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls;
    }
--- request eval
[
    "GET /t/echo/header/Hello",
    "GET /t/echo/header/hello"
]
--- more_headers
Hello: world
--- response_body eval
[
    "Hello: world\n",
    "hello: world\n"
]
--- no_error_log
[error]
[alert]



=== TEST 2: proxy_wasm - get_http_request_header() retrieves missing header
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls;
    }
--- request
GET /t/echo/header/Some-Header
--- response_body
Some-Header:
--- no_error_log
[error]
[crit]
[alert]
[stderr]
stub
stub



=== TEST 3: proxy_wasm - get_http_request_header() retrieves ':method'
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls;
    }
--- request eval
[
    "GET /t/echo/header/:method",
    "POST /t/echo/header/:method"
]
--- response_body eval
[
    ":method: GET\n",
    ":method: POST\n"
]
--- no_error_log
[error]
[crit]



=== TEST 4: proxy_wasm - get_http_request_header() retrieves ':path'
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls;
    }
--- request
GET /t/echo/header/:path
--- response_body
:path: /t/echo/header/:path
--- no_error_log
[error]
[crit]
[alert]
[stderr]
stub
stub



=== TEST 5: proxy_wasm - get_http_request_header() retrieves ':path' from r->uri (parsed)
URI component should be normalized
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls;
    }
--- request
GET //t/echo//header/:path
--- response_body
:path: /t/echo/header/:path
--- no_error_log
[error]
[crit]
[alert]
[stderr]
stub
stub



=== TEST 6: proxy_wasm - get_http_request_header() retrieves ':path' with r->args (querystring)
Path should include querystring
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls;
    }
--- request
GET /t/echo/header/:path?foo=bar&hello=world
--- response_body
:path: /t/echo/header/:path?foo=bar&hello=world
--- no_error_log
[error]
[crit]
[alert]
[stderr]
stub
stub



=== TEST 7: proxy_wasm - get_http_request_header() retrieves ':path' as a subrequest
should respond with the subrequest's path (/t/echo/header/...) and not the main
request's path (/t)
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t/echo/header/ {
        proxy_wasm hostcalls;
    }

    location /t {
        echo_subrequest GET /t/echo/header/:path;
    }
--- response_body
:path: /t/echo/header/:path
--- no_error_log
[error]
[crit]
[alert]
[stderr]
stub
stub



=== TEST 8: proxy_wasm - get_http_request_header() retrieves ':authority'
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    server_name localhost;

    location /t/echo/header/ {
        proxy_wasm hostcalls;
    }

    location /t {
        echo_subrequest GET /t/echo/header/:authority;
    }
--- response_body_like eval
qq^:authority: localhost:$ENV{TEST_NGINX_SERVER_PORT}^
--- no_error_log
[error]
[crit]
[alert]
[stderr]
stub
stub



=== TEST 9: proxy_wasm - get_http_request_header() retrieves ':authority' without server_name on unix listener
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- http_config eval
qq{
    upstream test_upstream {
        server unix:$ENV{TEST_NGINX_UNIX_SOCKET};
    }

    server {
        listen unix:$ENV{TEST_NGINX_UNIX_SOCKET};

        location / {
            proxy_wasm hostcalls;
        }
    }
}
--- config
    location /t {
        proxy_pass http://test_upstream/t/echo/header/:authority;
    }
--- response_body_like
:authority: [\S\s]+
--- no_error_log
[error]
[crit]
[alert]
[stderr]
stub
stub



=== TEST 10: proxy_wasm - get_http_request_header() retrieves ':scheme' (http)
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    server_name localhost;

    location /t/echo/header/ {
        proxy_wasm hostcalls;
    }

    location /t {
        echo_subrequest GET /t/echo/header/:scheme;
        echo_subrequest GET /t/echo/header/:scheme;
    }
--- response_body
:scheme: http
:scheme: http
--- no_error_log
[error]
[crit]
[alert]
[stderr]
stub
stub



=== TEST 11: proxy_wasm - get_http_request_header() retrieves ':scheme' (https)
--- skip_eval: 8: $::nginxV !~ m/built with OpenSSL/
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- http_config eval
qq{
    upstream test_upstream {
        server unix:$ENV{TEST_NGINX_UNIX_SOCKET};
    }

    server {
        listen               unix:$ENV{TEST_NGINX_UNIX_SOCKET} ssl;
        ssl_certificate      $ENV{TEST_NGINX_DATA_DIR}/cert.pem;
        ssl_certificate_key  $ENV{TEST_NGINX_DATA_DIR}/key.pem;

        location /t/echo/header/ {
            proxy_wasm hostcalls;
        }
    }
}
--- config
    location /t {
        proxy_pass https://test_upstream/t/echo/header/:scheme;
    }
--- response_body
:scheme: https
--- no_error_log
[error]
[crit]
[emerg]
[alert]
stub
stub



=== TEST 12: proxy_wasm - get_http_request_header() x on_phases
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t/A {
        proxy_wasm hostcalls 'on=request_headers \
                              test=/t/log/request_path';
        echo A;
    }

    location /t/B {
        proxy_wasm hostcalls 'on=response_headers \
                              test=/t/log/request_path';
        echo B;
    }

    location /t {
        echo_location /t/A;
        echo_location /t/B;
        proxy_wasm hostcalls 'on=log \
                              test=/t/log/request_path';
    }
--- ignore_response_body
--- grep_error_log eval: qr/(testing in|path:) .*/
--- grep_error_log_out eval
qr/testing in "RequestHeaders".*
path: \/t\/A.*
testing in "ResponseHeaders".*
path: \/t\/B.*
testing in "Log".*
path: \/t/
--- no_error_log
[error]
[crit]
[emerg]
[alert]
stub
stub
