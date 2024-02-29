# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasmX;

plan_tests(5);
run_tests();

__DATA__

=== TEST 1: proxy_wasm - get_http_response_body() retrieves response body
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'on=response_body';
        echo 'Hello world';
    }
--- request
GET /t/log/response_body
--- response_body
Hello world
--- grep_error_log eval: qr/(testing in|on_response_body,|response body chunk).*/
--- grep_error_log_out eval
qr/on_response_body, 12 bytes, eof: false.*
testing in "ResponseBody".*
response body chunk: "Hello world\\n".*
on_response_body, 0 bytes, eof: true.*/
--- no_error_log
[error]
[crit]



=== TEST 2: proxy_wasm - get_http_response_body() retrieves chunked response body
--- valgrind
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'on=response_body';
        echo 'Hello';
        echo 'world';
    }
--- request
GET /t/log/response_body
--- response_body
Hello
world
--- grep_error_log eval: qr/(on_response_body,|response body chunk).*/
--- grep_error_log_out eval
qr/on_response_body, 6 bytes, eof: false.*
response body chunk: "Hello\\n".*
on_response_body, 6 bytes, eof: false.*
response body chunk: "world\\n".*
on_response_body, 0 bytes, eof: true.*/
--- no_error_log
[error]
[crit]



=== TEST 3: proxy_wasm - get_http_response_body() retrieves None (no response body)
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/log/response_body on=response_body';
        return 200;
    }
--- response_body
--- grep_error_log eval: qr/(on_response_body,|response body chunk).*/
--- grep_error_log_out eval
qr/on_response_body, 0 bytes, eof: true.*/
--- no_error_log
[error]
[crit]



=== TEST 4: proxy_wasm - get_http_response_body() truncates body if longer than max_size
--- valgrind
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    output_buffers 1 2;

    location /t {
        proxy_wasm hostcalls 'on=response_body \
                              test=/t/log/response_body \
                              max_len=10';
        echo 'aaaaaaaaaaaaaaaaaaaa';
    }
--- response_body
aaaaaaaaaaaaaaaaaaaa
--- grep_error_log eval: qr/(on_response_body,|response body chunk).*/
--- grep_error_log_out eval
qr/on_response_body, 21 bytes, eof: false.*
response body chunk: "aaaaaaaaaa".*
on_response_body, 0 bytes, eof: true.*/
--- no_error_log
[crit]
[alert]



=== TEST 5: proxy_wasm - get_http_response_body() errors out on overflowing arguments
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/error/get_response_body on=response_body';
        echo 'Hello world';
    }
--- response_body
Hello world
--- error_log eval
[
    qr/on_response_body, 12 bytes, eof: false/,
    qr/\[crit\] .*? panicked at/,
    qr/unexpected status: 10/
]
