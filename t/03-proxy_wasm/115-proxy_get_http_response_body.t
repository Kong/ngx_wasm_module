# vim:set ft= ts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

skip_valgrind();

plan tests => repeat_each() * (blocks() * 5);

run_tests();

__DATA__

=== TEST 1: proxy_wasm - get_http_response_body() retrieves response body
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'on_phase=http_response_body';
        echo 'Hello world';
    }
--- request
GET /t/log/response_body
--- response_body
Hello world
--- grep_error_log eval: qr/\[(debug|info)\] .*? \[wasm\] .*?response(_|\s)body.*/
--- grep_error_log_out eval
qr/\[debug\] .*? \[wasm\] #\d+ on_response_body, 12 bytes, end_of_stream false
\[info\] .*? \[wasm\] #\d+ entering "HttpResponseBody" .*?
\[info\] .*? \[wasm\] response body chunk: "Hello world\\n" .*?
\[debug\] .*? \[wasm\] #\d+ on_response_body, 0 bytes, end_of_stream true/
--- no_error_log
[error]
[crit]



=== TEST 2: proxy_wasm - get_http_response_body() retrieves chunked response body
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'on_phase=http_response_body';
        echo 'Hello';
        echo 'world';
    }
--- request
GET /t/log/response_body
--- response_body
Hello
world
--- grep_error_log eval: qr/\[wasm\] .*?(#\d+ on_response_body|(response body chunk)).*/
--- grep_error_log_out eval
qr/\[wasm\] #\d+ on_response_body, 6 bytes, end_of_stream false
\[wasm\] response body chunk: "Hello\\n" .*?
\[wasm\] #\d+ on_response_body, 6 bytes, end_of_stream false
\[wasm\] response body chunk: "world\\n" .*?
\[wasm\] #\d+ on_response_body, 0 bytes, end_of_stream true/
--- no_error_log
[error]
[crit]



=== TEST 3: proxy_wasm - get_http_response_body() retrieves None (no response body)
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test_case=/t/log/response_body on_phase=http_response_body';
        return 200;
    }
--- response_body
--- grep_error_log eval: qr/\[wasm\] .*?(#\d+ on_response_body|(response body chunk)).*/
--- grep_error_log_out eval
qr/\[wasm\] #\d+ on_response_body, 0 bytes, end_of_stream true/
--- no_error_log
[error]
[crit]



=== TEST 4: proxy_wasm - get_http_response_body() retrieves None (too early)
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test_case=/t/log/response_body on_phase=http_request_body';
        echo 'Hello world';
    }
--- response_body
Hello world
--- grep_error_log eval: qr/\[wasm\] .*?(#\d+ on_response_body|(response body chunk)).*/
--- grep_error_log_out eval
qr/\[wasm\] #\d+ on_response_body, 12 bytes, end_of_stream false
\[wasm\] #\d+ on_response_body, 0 bytes, end_of_stream true/
--- no_error_log
[error]
[crit]



=== TEST 5: proxy_wasm - get_http_response_body() truncates body if longer than max_size
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    output_buffers 1 2;

    location /t {
        proxy_wasm hostcalls 'test_case=/t/log/response_body on_phase=http_response_body';
        echo 'aaaaaaaaaaaaaaaaaaaa';
    }
--- more_headers
pwm-log-resp-body-size: 10
--- response_body
aaaaaaaaaaaaaaaaaaaa
--- grep_error_log eval: qr/\[wasm\] .*?(#\d+ on_response_body|(response body chunk)).*/
--- grep_error_log_out eval
qr/\[wasm\] #\d+ on_response_body, 21 bytes, end_of_stream false
\[wasm\] response body chunk: "aaaaaaaaaa" .*?
\[wasm\] #\d+ on_response_body, 0 bytes, end_of_stream true/
--- no_error_log
[crit]
[alert]



=== TEST 6: proxy_wasm - get_http_response_body() errors out on overflowing arguments
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test_case=/t/error/get_response_body on_phase=http_response_body';
        echo 'Hello world';
    }
--- response_body
Hello world
--- error_log eval
[
    qr/\[wasm\] #\d+ on_response_body, 12 bytes, end_of_stream false/,
    qr/\[crit\] .*? panicked at 'unexpected status: 2'/
]
--- no_error_log
[alert]
