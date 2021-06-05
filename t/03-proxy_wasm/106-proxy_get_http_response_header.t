# vim:set ft= ts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

skip_valgrind();
no_long_string();

plan tests => repeat_each() * (blocks() * 5);

run_tests();

__DATA__

=== TEST 1: proxy_wasm - get_http_response_header() retrieves case insensitive header
--- load_nginx_modules: ngx_http_headers_more_filter_module
--- wasm_modules: hostcalls
--- config
    location /t {
        more_set_headers "Hello: world";
        proxy_wasm hostcalls on_phase=log;
        return 200;
    }
--- request
GET /t/log/response_header
--- more_headers
pwm-log-resp-header: hello
--- response_headers
Hello: world
--- error_log eval
qr/\[wasm\] resp header "hello: world"/
--- no_error_log
[error]
[crit]



=== TEST 2: proxy_wasm - get_http_response_header() does not retrieve missing header
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls on_phase=log;
        return 200;
    }
--- request
GET /t/log/response_header
--- more_headers
pwm-log-resp-header: hello
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
        proxy_wasm hostcalls 'on_phase=http_request_headers test_case=/t/add_http_response_header';
        proxy_wasm hostcalls  on_phase=http_request_headers;
        proxy_wasm hostcalls  on_phase=http_response_headers;
        echo ok;
        proxy_wasm hostcalls  on_phase=log;
    }
--- request
GET /t/log/response_header
--- more_headers
pwm-add-resp-header: Hello=there
pwm-log-resp-header: hello
--- response_headers
Hello: there
--- response_body
ok
--- grep_error_log eval: qr/\[wasm\] .*?(#\d+ entering "\S+"|resp\s).*?(?=\s+<)/
--- grep_error_log_out eval
qr/\[wasm\] .*? entering "HttpRequestHeaders"
\[wasm\] resp header "hello: there"
\[wasm\] .*? entering "HttpResponseHeaders"
\[wasm\] resp header "hello: there"
\[wasm\] .*? entering "Log"
\[wasm\] resp header "hello: there"/
--- no_error_log
[error]



=== TEST 4: proxy_wasm - get_http_response_header() get Server header
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls  on_phase=http_request_headers;
        proxy_wasm hostcalls  on_phase=http_response_headers;
        echo ok;
        proxy_wasm hostcalls  on_phase=log;
    }
--- request
GET /t/log/response_header
--- more_headers
pwm-log-resp-header: Server
--- response_headers_like
Server: nginx.*?
--- response_body
ok
--- grep_error_log eval: qr/\[wasm\] .*?(#\d+ entering "\S+"|resp\s).*?(?=\s+<)/
--- grep_error_log_out eval
qr/\[wasm\] .*? entering "HttpRequestHeaders"
\[wasm\] .*? entering "HttpResponseHeaders"
\[wasm\] resp header "Server: nginx.*?"
\[wasm\] .*? entering "Log"
\[wasm\] resp header "Server: nginx.*?"/
--- no_error_log
[error]



=== TEST 5: proxy_wasm - get_http_response_header() get Date header
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls  on_phase=http_request_headers;
        proxy_wasm hostcalls  on_phase=http_response_headers;
        echo ok;
        proxy_wasm hostcalls  on_phase=log;
    }
--- request
GET /t/log/response_header
--- more_headers
pwm-log-resp-header: Date
--- response_headers_like
Date: .*? GMT
--- response_body
ok
--- grep_error_log eval: qr/\[wasm\] .*?(#\d+ entering "\S+"|resp\s).*?(?=\s+<)/
--- grep_error_log_out eval
qr/\[wasm\] .*? entering "HttpRequestHeaders"
\[wasm\] .*? entering "HttpResponseHeaders"
\[wasm\] resp header "Date: .*? GMT"
\[wasm\] .*? entering "Log"
\[wasm\] resp header "Date: .*? GMT"/
--- no_error_log
[error]



=== TEST 6: proxy_wasm - get_http_response_header() get Last-Modified header
--- wasm_modules: hostcalls
--- config
    location /t {
        empty_gif;

        proxy_wasm hostcalls  on_phase=http_request_headers;
        proxy_wasm hostcalls  on_phase=http_response_headers;
        proxy_wasm hostcalls  on_phase=log;
    }
--- request
GET /t/log/response_header
--- more_headers
pwm-log-resp-header: Last-Modified
--- response_headers_like
Last-Modified: .*? GMT
--- ignore_response_body
--- grep_error_log eval: qr/\[wasm\] .*?(#\d+ entering "\S+"|resp\s).*?(?=\s+<)/
--- grep_error_log_out eval
qr/\[wasm\] .*? entering "HttpRequestHeaders"
\[wasm\] .*? entering "HttpResponseHeaders"
\[wasm\] resp header "Last-Modified: .*? GMT"
\[wasm\] .*? entering "Log"
\[wasm\] resp header "Last-Modified: .*? GMT"/
--- no_error_log
[error]
[crit]



=== TEST 7: proxy_wasm - get_http_response_header() get Content-Type header (some)
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls   on_phase=http_request_headers;
        proxy_wasm hostcalls  'on_phase=http_request_headers test_case=/t/send_local_response/body';
        proxy_wasm hostcalls   on_phase=http_response_headers;
        proxy_wasm hostcalls   on_phase=log;
    }
--- request
GET /t/log/response_header
--- more_headers
pwm-log-resp-header: Content-Type
--- response_headers
Content-Type: text/plain
--- response_body
Hello world
--- grep_error_log eval: qr/\[wasm\] .*?(#\d+ entering "\S+"|resp\s).*?(?=\s+<)/
--- grep_error_log_out eval
qr/\[wasm\] .*? entering "HttpRequestHeaders"
\[wasm\] .*? entering "HttpResponseHeaders"
\[wasm\] resp header "Content-Type: text\/plain"
\[wasm\] .*? entering "Log"
\[wasm\] resp header "Content-Type: text\/plain"/
--- no_error_log
[error]



=== TEST 8: proxy_wasm - get_http_response_header() get Content-Type header (none)
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls   on_phase=http_request_headers;
        proxy_wasm hostcalls  'on_phase=http_request_headers test_case=/t/send_local_response/status/204';
        proxy_wasm hostcalls   on_phase=http_response_headers;
        proxy_wasm hostcalls   on_phase=log;
    }
--- request
GET /t/log/response_header
--- more_headers
pwm-log-resp-header: Content-Type
--- error_code: 204
--- response_headers
Content-Type:
--- response_body
--- grep_error_log eval: qr/\[wasm\] .*?(#\d+ entering "\S+"|resp\s).*?(?=\s+<)/
--- grep_error_log_out eval
qr/\[wasm\] .*? entering "HttpRequestHeaders"
\[wasm\] .*? entering "HttpResponseHeaders"
\[wasm\] .*? entering "Log"/
--- no_error_log
[error]



=== TEST 9: proxy_wasm - get_http_response_header() get Connection header (close)
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls  on_phase=http_request_headers;
        proxy_wasm hostcalls  on_phase=http_response_headers;
        proxy_wasm hostcalls  on_phase=log;
        return 200;
    }
--- request
GET /t/log/response_header
--- more_headers
pwm-log-resp-header: connection
--- response_headers_like
Connection: close
--- response_body
--- grep_error_log eval: qr/\[wasm\] .*?(#\d+ entering "\S+"|resp\s).*?(?=\s+<)/
--- grep_error_log_out eval
qr/\[wasm\] .*? entering "HttpRequestHeaders"
\[wasm\] resp header "connection: close"
\[wasm\] .*? entering "HttpResponseHeaders"
\[wasm\] resp header "connection: close"
\[wasm\] .*? entering "Log"
\[wasm\] resp header "connection: close"/
--- no_error_log
[error]



=== TEST 10: proxy_wasm - get_http_response_header() get Connection header (keep-alive)
--- abort
--- timeout: 3s
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls  on_phase=http_request_headers;
        proxy_wasm hostcalls  on_phase=http_response_headers;
        proxy_wasm hostcalls  on_phase=log;
        return 200;
    }
--- request
GET /t/log/response_header
--- more_headers
Connection: keep-alive
pwm-log-resp-header: connection
--- response_headers
Connection: keep-alive
--- response_body
--- grep_error_log eval: qr/\[wasm\] .*?(#\d+ entering "\S+"|resp\s).*?(?=\s+<)/
--- grep_error_log_out eval
qr/\[wasm\] .*? entering "HttpRequestHeaders"
\[wasm\] resp header "connection: keep-alive"
\[wasm\] .*? entering "HttpResponseHeaders"
\[wasm\] resp header "connection: keep-alive"
\[wasm\] .*? entering "Log"
\[wasm\] resp header "connection: keep-alive"/
--- no_error_log
[error]



=== TEST 11: proxy_wasm - get_http_response_header() get Connection header (upgrade)
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls  on_phase=http_request_headers;
        proxy_wasm hostcalls  on_phase=http_response_headers;
        proxy_wasm hostcalls  on_phase=log;
        return 101;
    }
--- request
GET /t/log/response_header
--- more_headers
pwm-log-resp-header: connection
--- response_headers
Connection: upgrade
--- error_code: 101
--- ignore_response_body
--- grep_error_log eval: qr/\[wasm\] .*?(#\d+ entering "\S+"|resp\s).*?(?=\s+<)/
--- grep_error_log_out eval
qr/\[wasm\] .*? entering "HttpRequestHeaders"
\[wasm\] resp header "connection: close"
\[wasm\] .*? entering "HttpResponseHeaders"
\[wasm\] resp header "connection: upgrade"
\[wasm\] .*? entering "Log"
\[wasm\] resp header "connection: upgrade"/
--- no_error_log
[error]
[crit]



=== TEST 12: proxy_wasm - get_http_response_header() get Keep-Alive header
--- abort
--- timeout: 3s
--- wasm_modules: hostcalls
--- config
    location /t {
        keepalive_timeout 1s 1m;
        proxy_wasm hostcalls  on_phase=http_request_headers;
        proxy_wasm hostcalls  on_phase=http_response_headers;
        proxy_wasm hostcalls  on_phase=log;
        return 200;
    }
--- request
GET /t/log/response_header
--- more_headers
Connection: keep-alive
pwm-log-resp-header: Keep-Alive
--- response_headers
Keep-Alive: timeout=60
--- ignore_response_body
--- grep_error_log eval: qr/\[wasm\] .*?(#\d+ entering "\S+"|resp\s).*?(?=\s+<)/
--- grep_error_log_out eval
qr/\[wasm\] .*? entering "HttpRequestHeaders"
\[wasm\] resp header "Keep-Alive: timeout=60"
\[wasm\] .*? entering "HttpResponseHeaders"
\[wasm\] resp header "Keep-Alive: timeout=60"
\[wasm\] .*? entering "Log"
\[wasm\] resp header "Keep-Alive: timeout=60"/
--- no_error_log
[error]
[crit]



=== TEST 13: proxy_wasm - get_http_response_header() get Transfer-Encoding header (chunked)
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls  on_phase=http_request_headers;
        proxy_wasm hostcalls  on_phase=http_response_headers;
        proxy_wasm hostcalls  on_phase=log;
        echo ok;
    }
--- request
GET /t/log/response_header
--- more_headers
pwm-log-resp-header: Transfer-Encoding
--- response_headers
Transfer-Encoding: chunked
--- ignore_response_body
--- grep_error_log eval: qr/\[wasm\] .*?(#\d+ entering "\S+"|resp\s).*?(?=\s+<)/
--- grep_error_log_out eval
qr/\[wasm\] .*? entering "HttpRequestHeaders"
\[wasm\] resp header "Transfer-Encoding: chunked"
\[wasm\] .*? entering "HttpResponseHeaders"
\[wasm\] resp header "Transfer-Encoding: chunked"
\[wasm\] .*? entering "Log"
\[wasm\] resp header "Transfer-Encoding: chunked"/
--- no_error_log
[error]
[crit]



=== TEST 14: proxy_wasm - get_http_response_header() get Transfer-Encoding header (none)
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls  on_phase=http_request_headers;
        proxy_wasm hostcalls  on_phase=http_response_headers;
        proxy_wasm hostcalls  on_phase=log;
        return 200;
    }
--- request
GET /t/log/response_header
--- more_headers
pwm-log-resp-header: Transfer-Encoding
--- response_headers
Transfer-Encoding:
--- ignore_response_body
--- grep_error_log eval: qr/\[wasm\] .*?(#\d+ entering "\S+"|resp\s).*?(?=\s+<)/
--- grep_error_log_out eval
qr/\[wasm\] .*? entering "HttpRequestHeaders"
\[wasm\] resp header "Transfer-Encoding: chunked"
\[wasm\] .*? entering "HttpResponseHeaders"
\[wasm\] .*? entering "Log"/
--- no_error_log
[error]
[crit]
