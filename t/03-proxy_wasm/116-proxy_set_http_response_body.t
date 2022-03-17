# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

skip_valgrind();

plan tests => repeat_each() * (blocks() * 7);

run_tests();

__DATA__

=== TEST 1: proxy_wasm - set_http_response_body() sets body chunk in http_response_body (Content-Length)
should update response chunk
cannot update response headers (Content-Length)
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/send_local_response/body';

        proxy_wasm hostcalls 'on=response_body \
                              test=/t/log/response_body';

        proxy_wasm hostcalls 'on=response_body \
                              test=/t/set_response_body \
                              value=updated';

        proxy_wasm hostcalls 'on=response_body \
                              test=/t/log/response_body';
    }
--- response_headers
Content-Length: 12
--- response_body
updated
--- grep_error_log eval: qr/\[info\] .*? response body chunk.*/
--- grep_error_log_out eval
qr/.*? response body chunk: "Hello world\\n".*
.*? response body chunk: "updated\\n".*/
--- error_log eval
[
    qr/on_response_body, 12 bytes, end_of_stream true/,
    qr/\[warn\] .*? overriding response body chunk while Content-Length header already sent/
]
--- no_error_log
[error]



=== TEST 2: proxy_wasm - set_http_response_body() sets body chunk in http_response_body (Transfer-Encoding)
should update response chunk
should be retrieved by get_http_response_body()
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        echo 'Hello';

        proxy_wasm hostcalls 'on=response_body \
                              test=/t/log/response_body';

        proxy_wasm hostcalls 'on=response_body \
                              test=/t/set_response_body \
                              value=updated';

        proxy_wasm hostcalls 'on=response_body \
                              test=/t/log/response_body';
    }
--- response_headers
Transfer-Encoding: chunked
Content-Length:
--- response_body
updated
--- grep_error_log eval: qr/(on_response_body,|response body chunk).*/
--- grep_error_log_out eval
qr/on_response_body, 6 bytes, end_of_stream false.*
response body chunk: "Hello\\n".*
on_response_body, 6 bytes, end_of_stream false.*
on_response_body, 8 bytes, end_of_stream false.*
response body chunk: "updated\\n".*
on_response_body, 0 bytes, end_of_stream true.*
on_response_body, 0 bytes, end_of_stream true.*
on_response_body, 0 bytes, end_of_stream true.*/
--- no_error_log
[warn]
[error]



=== TEST 3: proxy_wasm - set_http_response_body() can set a response body to empty value ''
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        echo fail;

        proxy_wasm hostcalls 'on=response_body \
                              test=/t/set_response_body value=';

        proxy_wasm hostcalls 'on=log \
                              test=/t/log/response_body';
    }
--- response_headers
Transfer-Encoding: chunked
Content-Length:
--- response_body chomp
--- grep_error_log eval: qr/(on_response_body,|response body chunk).*/
--- grep_error_log_out eval
qr/on_response_body, 5 bytes, end_of_stream false.*
on_response_body, 0 bytes, end_of_stream true.*
on_response_body, 0 bytes, end_of_stream true.*/
--- no_error_log
[error]
[crit]



=== TEST 4: proxy_wasm - set_http_response_body() set response body from subrequest
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /subrequest {
        internal;
        echo fail;
        proxy_wasm hostcalls 'on=response_body \
                              test=/t/set_response_body \
                              value=HelloWorld';

        proxy_wasm hostcalls 'on=response_body \
                              test=/t/log/response_body';
    }

    location /t {
        echo_subrequest GET /subrequest;
        proxy_wasm hostcalls 'on=response_headers \
                              test=/t/log/response_body';
    }
--- response_headers
Transfer-Encoding: chunked
Content-Length:
--- response_body
HelloWorld
--- grep_error_log eval: qr/(on_response_body,|(overriding\s)?response body chunk).*/
--- grep_error_log_out eval
qr/on_response_body, 5 bytes, end_of_stream false.*
overriding response body chunk while Content-Length header already sent.*
on_response_body, 11 bytes, end_of_stream false.*
response body chunk: "HelloWorld\\n".*
on_response_body, 0 bytes, end_of_stream true.*
on_response_body, 0 bytes, end_of_stream true.*
on_response_body, 0 bytes, end_of_stream true.*/
--- no_error_log
[error]
[crit]



=== TEST 5: proxy_wasm - set_http_response_body() with offset argument
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /a {
        echo 'hello world';
        proxy_wasm hostcalls 'on=response_body \
                              test=/t/set_response_body \
                              value=wasm offset=6';
        proxy_wasm hostcalls 'on=response_body test=/t/log/response_body';
    }

    location /b {
        # offset == 0
        echo 'hello world';
        proxy_wasm hostcalls 'on=response_body \
                              test=/t/set_response_body \
                              value=Goodbye offset=0';
        proxy_wasm hostcalls 'on=response_body test=/t/log/response_body';
    }

    location /c {
        # offset larger than buffer
        echo -n 'hello';
        proxy_wasm hostcalls 'on=response_body \
                              test=/t/set_response_body \
                              value=LAST offset=10';
        proxy_wasm hostcalls 'on=response_body test=/t/log/response_body';
    }

    location /t {
        echo_subrequest GET /a;
        echo_subrequest GET /b;
        echo_subrequest GET /c;
    }
--- response_headers
Transfer-Encoding: chunked
Content-Length:
--- response_body
hello wasm
Goodbye
hello     LAST
--- grep_error_log eval: qr/(on_response_body,|(overriding\s)?response body chunk).*/
--- grep_error_log_out eval
qr/on_response_body, 12 bytes, end_of_stream false.*
overriding response body chunk while Content-Length header already sent.*
on_response_body, 11 bytes, end_of_stream false.*
response body chunk: "hello wasm\\n".*
on_response_body, 0 bytes, end_of_stream true.*
on_response_body, 0 bytes, end_of_stream true.*
on_response_body, 12 bytes, end_of_stream false.*
overriding response body chunk while Content-Length header already sent.*
on_response_body, 8 bytes, end_of_stream false.*
response body chunk: "Goodbye\\n".*
on_response_body, 0 bytes, end_of_stream true.*
on_response_body, 0 bytes, end_of_stream true.*
on_response_body, 5 bytes, end_of_stream false.*
overriding response body chunk while Content-Length header already sent.*
on_response_body, 15 bytes, end_of_stream false.*
response body chunk: "hello     LAST\\n".*
on_response_body, 0 bytes, end_of_stream true.*
on_response_body, 0 bytes, end_of_stream true.*/
--- no_error_log
[error]
[crit]



=== TEST 6: proxy_wasm - set_http_response_body() with max argument
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /a {
        echo 'hello world';
        proxy_wasm hostcalls 'on=response_body \
                              test=/t/set_response_body \
                              value=wasm offset=6 max=3';
        proxy_wasm hostcalls 'on=response_body test=/t/log/response_body';
    }

    location /b {
        # offset == 0
        echo 'hello world';
        proxy_wasm hostcalls 'on=response_body \
                              test=/t/set_response_body \
                              value=Goodbye offset=0 max=0';
        proxy_wasm hostcalls 'on=response_body test=/t/log/response_body';
    }

    location /c {
        # offset larger than buffer
        echo -n 'hello';
        proxy_wasm hostcalls 'on=response_body \
                              test=/t/set_response_body \
                              value=LAST offset=0 max=20';
        proxy_wasm hostcalls 'on=response_body test=/t/log/response_body';
    }

    location /t {
        echo_subrequest GET /a;
        echo;
        echo_subrequest GET /b;
        echo_subrequest GET /c;
    }
--- response_headers
Transfer-Encoding: chunked
Content-Length:
--- response_body
hello was
LAST
--- grep_error_log eval: qr/(on_response_body,|(overriding\s)?response body chunk).*/
--- grep_error_log_out eval
qr/on_response_body, 12 bytes, end_of_stream false.*
overriding response body chunk while Content-Length header already sent.*
on_response_body, 9 bytes, end_of_stream false.*
response body chunk: "hello was".*
on_response_body, 0 bytes, end_of_stream true.*
on_response_body, 0 bytes, end_of_stream true.*
on_response_body, 12 bytes, end_of_stream false.*
overriding response body chunk while Content-Length header already sent.*
on_response_body, 0 bytes, end_of_stream true.*
on_response_body, 0 bytes, end_of_stream true.*
on_response_body, 5 bytes, end_of_stream false.*
overriding response body chunk while Content-Length header already sent.*
on_response_body, 5 bytes, end_of_stream false.*
response body chunk: "LAST\\n".*
on_response_body, 0 bytes, end_of_stream true.*
on_response_body, 0 bytes, end_of_stream true.*/
--- no_error_log
[error]
[crit]



=== TEST 7: proxy_wasm - set_http_response_body() x on_phases
should not be usable anywhere else than on_http_response_body
should not be retrievable after on_http_response_body since buffers are consumed
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    proxy_wasm_isolation filter;

    location /request_headers {
        internal;
        echo;
        proxy_wasm hostcalls 'on=request_headers \
                              test=/t/set_response_body';
        proxy_wasm hostcalls 'on=request_headers \
                              test=/t/log/response_body';
    }

    location /response_headers {
        internal;
        echo;
        proxy_wasm hostcalls 'on=response_headers \
                              test=/t/set_response_body';
        proxy_wasm hostcalls 'on=response_headers \
                              test=/t/log/response_body';
    }

    location /t {
        echo_subrequest GET /request_headers;
        echo_subrequest GET /response_headers;
        proxy_wasm hostcalls 'on=log test=/t/set_response_body';
    }
--- response_headers
Transfer-Encoding: chunked
Content-Length:
--- response_body eval
qr/500 Internal Server Error/
--- grep_error_log eval: qr/(\[error\]|\[.*?failed resuming).*/
--- grep_error_log_out eval
qr/\[error\] .*?cannot set response body.*
\[warn\] .*? \*\d+ \[wasm\] proxy_wasm "hostcalls" filter \(1\/2\) failed resuming \(instance trapped\).*? subrequest: "\/request_headers".*
\[error\] .*?cannot set response body.*
\[warn\] .*? \*\d+ \[wasm\] proxy_wasm "hostcalls" filter \(1\/2\) failed resuming \(instance trapped\).*? subrequest: "\/response_headers".*
\[error\] .*?cannot set response body.*/
--- no_error_log
[alert]
[stderr]
