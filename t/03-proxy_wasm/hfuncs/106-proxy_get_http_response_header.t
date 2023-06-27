# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

skip_valgrind();

plan tests => repeat_each() * (blocks() * 5);

run_tests();

__DATA__

=== TEST 1: proxy_wasm - get_http_response_header() retrieves case insensitive header
--- load_nginx_modules: ngx_http_headers_more_filter_module
--- wasm_modules: hostcalls
--- config
    location /t {
        more_set_headers "Hello: world";
        proxy_wasm hostcalls 'on=log \
                              test=/t/log/response_header \
                              name=hello';
        return 200;
    }
--- response_headers
Hello: world
--- error_log eval
qr/resp header "hello: world"/
--- no_error_log
[error]
[crit]



=== TEST 2: proxy_wasm - get_http_response_header() does not retrieve missing header
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'on=log \
                              test=/t/log/response_header \
                              name=hello';
        return 200;
    }
--- response_headers
Hello:
--- no_error_log eval
[
    qr/\[wasm\] resp header "hello: world"/,
    qr/\[error\]/,
    qr/\[crit\]/,
]



=== TEST 3: proxy_wasm - get_http_response_header() x on_phases
should produce a result in: on_request_headers, on_response_header, on_log
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'on=request_headers \
                              test=/t/add_response_header \
                              value=Hello:there';

        proxy_wasm hostcalls 'on=request_headers \
                              test=/t/log/response_header \
                              name=hello';

        echo ok;

        proxy_wasm hostcalls 'on=response_headers \
                              test=/t/log/response_header \
                              name=hello';

        proxy_wasm hostcalls 'on=log \
                              test=/t/log/response_header \
                              name=hello';
    }
--- response_headers
Hello: there
--- response_body
ok
--- grep_error_log eval: qr/(testing in|resp) .*/
--- grep_error_log_out eval
qr/testing in "RequestHeaders".*
resp header "hello: there".*
testing in "ResponseHeaders".*
resp header "hello: there".*
testing in "Log".*
resp header "hello: there".*/
--- no_error_log
[error]



=== TEST 4: proxy_wasm - get_http_response_header() get Server header
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'on=request_headers \
                              test=/t/log/response_header \
                              name=Server';

        echo ok;

        proxy_wasm hostcalls 'on=response_headers \
                              test=/t/log/response_header \
                              name=Server';

        proxy_wasm hostcalls 'on=log \
                              test=/t/log/response_header \
                              name=Server';
    }
--- response_headers_like
Server: (nginx|openresty).*?
--- response_body
ok
--- grep_error_log eval: qr/(testing in|resp) .*/
--- grep_error_log_out eval
qr/testing in "RequestHeaders".*
testing in "ResponseHeaders".*
resp header "Server: (nginx|openresty).*?".*
testing in "Log".*
resp header "Server: (nginx|openresty).*?".*/
--- no_error_log
[error]



=== TEST 5: proxy_wasm - get_http_response_header() get Date header
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'on=request_headers \
                              test=/t/log/response_header \
                              name=Date';

        echo ok;

        proxy_wasm hostcalls 'on=response_headers \
                              test=/t/log/response_header \
                              name=Date';

        proxy_wasm hostcalls 'on=log \
                              test=/t/log/response_header \
                              name=Date';
    }
--- response_headers_like
Date: .*? GMT
--- response_body
ok
--- grep_error_log eval: qr/(testing in|resp) .*/
--- grep_error_log_out eval
qr/testing in "RequestHeaders".*
testing in "ResponseHeaders".*
resp header "Date: .*? GMT".*
testing in "Log".*
resp header "Date: .*? GMT".*/
--- no_error_log
[error]



=== TEST 6: proxy_wasm - get_http_response_header() get Last-Modified header
--- wasm_modules: hostcalls
--- config
    location /t {
        empty_gif;

        proxy_wasm hostcalls 'on=request_headers \
                              test=/t/log/response_header \
                              name=Last-Modified';

        proxy_wasm hostcalls 'on=response_headers \
                              test=/t/log/response_header \
                              name=Last-Modified';

        proxy_wasm hostcalls 'on=log \
                              test=/t/log/response_header \
                              name=Last-Modified';
    }
--- response_headers_like
Last-Modified: .*? GMT
--- ignore_response_body
--- grep_error_log eval: qr/(testing in|resp) .*/
--- grep_error_log_out eval
qr/testing in "RequestHeaders".*
testing in "ResponseHeaders".*
resp header "Last-Modified: .*? GMT".*
testing in "Log".*
resp header "Last-Modified: .*? GMT".*/
--- no_error_log
[error]
[crit]



=== TEST 7: proxy_wasm - get_http_response_header() get Content-Type header (some)
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'on=request_headers \
                              test=/t/log/response_header \
                              name=Content-Type';

        proxy_wasm hostcalls 'on=request_headers \
                              test=/t/send_local_response/body';

        proxy_wasm hostcalls 'on=response_headers \
                              test=/t/log/response_header \
                              name=Content-Type';

        proxy_wasm hostcalls 'on=log \
                              test=/t/log/response_header \
                              name=Content-Type';
    }
--- response_headers
Content-Type: text/plain
--- response_body
Hello world
--- grep_error_log eval: qr/(testing in|resp) .*/
--- grep_error_log_out eval
qr/testing in "RequestHeaders".*
testing in "ResponseHeaders".*
resp header "Content-Type: text\/plain".*
testing in "Log.*"
resp header "Content-Type: text\/plain".*/
--- no_error_log
[error]



=== TEST 8: proxy_wasm - get_http_response_header() get Content-Type header (none)
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'on=request_headers \
                              test=/t/log/response_header \
                              name=Content-Type';

        proxy_wasm hostcalls 'on=request_headers \
                              test=/t/send_local_response/status/204';

        proxy_wasm hostcalls 'on=response_headers \
                              test=/t/log/response_header \
                              name=Content-Type';

        proxy_wasm hostcalls 'on=log \
                              test=/t/log/response_header \
                              name=Content-Type';
    }
--- error_code: 204
--- response_headers
Content-Type:
--- response_body
--- grep_error_log eval: qr/(testing in|resp) .*/
--- grep_error_log_out eval
qr/testing in "RequestHeaders".*
testing in "ResponseHeaders".*
testing in "Log.*"/
--- no_error_log
[error]



=== TEST 9: proxy_wasm - get_http_response_header() get Connection header (close)
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'on=request_headers \
                              test=/t/log/response_header \
                              name=connection';

        return 200;

        proxy_wasm hostcalls 'on=response_headers \
                              test=/t/log/response_header \
                              name=connection';

        proxy_wasm hostcalls 'on=log \
                              test=/t/log/response_header \
                              name=connection';
    }
--- response_headers_like
Connection: close
--- response_body
--- grep_error_log eval: qr/(testing in|resp) .*/
--- grep_error_log_out eval
qr/testing in "RequestHeaders".*
resp header "connection: close".*
testing in "ResponseHeaders".*
resp header "connection: close".*
testing in "Log".*
resp header "connection: close".*/
--- no_error_log
[error]



=== TEST 10: proxy_wasm - get_http_response_header() get Connection header (keep-alive)
--- timeout_expected: 5
--- abort
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'on=request_headers \
                              test=/t/log/response_header \
                              name=connection';

        echo ok;

        proxy_wasm hostcalls 'on=response_headers \
                              test=/t/log/response_header \
                              name=connection';

        proxy_wasm hostcalls 'on=log \
                              test=/t/log/response_header \
                              name=connection';
    }
--- more_headers
Connection: keep-alive
--- response_headers
Connection: keep-alive
--- response_body
ok
--- grep_error_log eval: qr/(testing in|resp) .*/
--- grep_error_log_out eval
qr/testing in "RequestHeaders".*
resp header "connection: keep-alive".*
testing in "ResponseHeaders".*
resp header "connection: keep-alive".*
testing in "Log".*
resp header "connection: keep-alive".*/
--- no_error_log
[error]



=== TEST 11: proxy_wasm - get_http_response_header() get Connection header (upgrade)
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'on=request_headers \
                              test=/t/log/response_header \
                              name=connection';

        echo_status 101;
        echo_flush;

        proxy_wasm hostcalls 'on=response_headers \
                              test=/t/log/response_header \
                              name=connection';

        proxy_wasm hostcalls 'on=log \
                              test=/t/log/response_header \
                              name=connection';
    }
--- response_headers
Connection: upgrade
--- error_code: 101
--- ignore_response_body
--- grep_error_log eval: qr/(testing in|resp) .*/
--- grep_error_log_out eval
qr/testing in "RequestHeaders".*
resp header "connection: close".*
testing in "ResponseHeaders".*
resp header "connection: upgrade".*
testing in "Log".*
resp header "connection: upgrade".*/
--- no_error_log
[error]
[crit]



=== TEST 12: proxy_wasm - get_http_response_header() get Keep-Alive header
--- timeout_expected: 5
--- abort
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        keepalive_timeout 1s 1m;

        proxy_wasm hostcalls 'on=request_headers \
                              test=/t/log/response_header \
                              name=Keep-Alive';

        echo ok;

        proxy_wasm hostcalls 'on=response_headers \
                              test=/t/log/response_header \
                              name=Keep-Alive';

        proxy_wasm hostcalls 'on=log \
                              test=/t/log/response_header \
                              name=Keep-Alive';
    }
--- more_headers
Connection: keep-alive
--- response_headers
Keep-Alive: timeout=60
--- ignore_response_body
--- grep_error_log eval: qr/(testing in|resp) .*/
--- grep_error_log_out eval
qr/testing in "RequestHeaders".*
resp header "Keep-Alive: timeout=60".*
testing in "ResponseHeaders".*
resp header "Keep-Alive: timeout=60".*
testing in "Log".*
resp header "Keep-Alive: timeout=60".*/
--- no_error_log
[error]
[crit]



=== TEST 13: proxy_wasm - get_http_response_header() get Transfer-Encoding header (chunked)
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'on=request_headers \
                              test=/t/log/response_header \
                              name=Transfer-Encoding';

        echo ok;

        proxy_wasm hostcalls 'on=response_headers \
                              test=/t/log/response_header \
                              name=Transfer-Encoding';

        proxy_wasm hostcalls 'on=log \
                              test=/t/log/response_header \
                              name=Transfer-Encoding';
    }
--- response_headers
Transfer-Encoding: chunked
--- ignore_response_body
--- grep_error_log eval: qr/(testing in|resp) .*/
--- grep_error_log_out eval
qr/testing in "RequestHeaders".*
resp header "Transfer-Encoding: chunked".*
testing in "ResponseHeaders".*
resp header "Transfer-Encoding: chunked".*
testing in "Log".*
resp header "Transfer-Encoding: chunked".*/
--- no_error_log
[error]
[crit]



=== TEST 14: proxy_wasm - get_http_response_header() get Transfer-Encoding header (none)
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'on=request_headers \
                              test=/t/log/response_header \
                              name=Transfer-Encoding';

        return 200;

        proxy_wasm hostcalls 'on=response_headers \
                              test=/t/log/response_header \
                              name=Transfer-Encoding';

        proxy_wasm hostcalls 'on=log \
                              test=/t/log/response_header \
                              name=Transfer-Encoding';
    }
--- response_headers
Transfer-Encoding:
--- ignore_response_body
--- grep_error_log eval: qr/(testing in|resp) .*/
--- grep_error_log_out eval
qr/testing in "RequestHeaders".*
resp header "Transfer-Encoding: chunked".*
testing in "ResponseHeaders".*
testing in "Log".*/
--- no_error_log
[error]
[crit]



=== TEST 15: proxy_wasm - get_http_response_header() get :status special header (sanity)
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'on=request_headers \
                              test=/t/log/response_header \
                              name=:status';

        return 201;

        proxy_wasm hostcalls 'on=response_headers \
                              test=/t/log/response_header \
                              name=:status';

        proxy_wasm hostcalls 'on=log \
                              test=/t/log/response_header \
                              name=:status';
    }
--- error_code: 201
--- response_body
--- grep_error_log eval: qr/(testing in|resp) .*/
--- grep_error_log_out eval
qr/testing in "RequestHeaders".*
testing in "ResponseHeaders".*
resp header ":status: 201".*
testing in "Log".*
resp header ":status: 201".*/
--- no_error_log
[error]
[crit]



=== TEST 16: proxy_wasm - get_http_response_header() get :status special header (different statuses)
Status strings are cached in the filter chain context so this covers the
re-alloc branch.
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls ngx_rust_tests
--- config
    location /t {
        wasm_call rewrite ngx_rust_tests set_resp_status;

        proxy_wasm hostcalls 'on=request_headers \
                              test=/t/log/response_header \
                              name=:status';

        proxy_wasm hostcalls 'on=request_headers \
                              test=/t/send_local_response/status/204';

        proxy_wasm hostcalls 'on=log \
                              test=/t/log/response_header \
                              name=:status';
    }
--- error_code: 204
--- response_body
--- grep_error_log eval: qr/(testing in|resp header) .*/
--- grep_error_log_out eval
qr/testing in "RequestHeaders".*
resp header ":status: 201".*
testing in "RequestHeaders".*
testing in "Log".*
resp header ":status: 204"/
--- no_error_log
[error]
[crit]



=== TEST 17: proxy_wasm - get_http_response_header() get :status special header (in error handler location)
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    error_page 500 /error;

    location /t {
        return 500;
    }

    location = /error {
        internal;
        echo 'error handler content';

        proxy_wasm hostcalls 'on=log \
                              test=/t/log/response_header \
                              name=:status';
    }
--- error_code: 500
--- response_body
error handler content
--- grep_error_log eval: qr/(testing in|resp) .*/
--- grep_error_log_out eval
qr/testing in "Log".*
resp header ":status: 500"/
--- no_error_log
[emerg]
[alert]



=== TEST 18: proxy_wasm - get_http_response_header() get :status special header (HTTP/0.9)
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'on=request_headers \
                              test=/t/log/response_header \
                              name=:status';
        echo ok;
    }
--- http09
--- raw_request eval
"GET /t\r\n"
--- ignore_response_body
--- grep_error_log eval: qr/(testing in|resp) .*/
--- grep_error_log_out eval
qr/testing in "RequestHeaders".*
resp header ":status: 009"/
--- no_error_log
[error]
[crit]
[emerg]
