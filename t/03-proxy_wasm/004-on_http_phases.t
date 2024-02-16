# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

plan_tests(5);
run_tests();

__DATA__

=== TEST 1: proxy_wasm steps - no request body + no response body (return 200)
--- wasm_modules: on_phases
--- config
    location /t {
        proxy_wasm on_phases;
        return 200;
    }
--- more_headers
Hello: wasm
--- raw_response_headers_like
HTTP\/1\.1 .*?\r
Content-Type: text\/plain\r
Content-Length: 0\r
Connection: close\r
Server: [\S\s]+\r
Date: [\S\s]+\r
--- response_body
--- grep_error_log eval: qr/#\d+ on_(request|response|log).*?(?=(, client|\s+while))/
--- grep_error_log_out eval
qr/#\d+ on_request_headers, 3 headers, eof: false
#\d+ on_response_headers, 5 headers, eof: false
#\d+ on_response_body, 0 bytes, eof: true
#\d+ on_log/
--- no_error_log
[error]



=== TEST 2: proxy_wasm steps - no request body + no response body (return 204)
--- wasm_modules: on_phases
--- config
    location /t {
        proxy_wasm on_phases;
        return 204;
    }
--- error_code: 204
--- response_body
--- grep_error_log eval: qr/#\d+ on_(request|response|log).*?(?=(, client|\s+while))/
--- grep_error_log_out eval
qr/#\d+ on_request_headers, 2 headers, eof: false
#\d+ on_response_headers, 5 headers, eof: false
#\d+ on_log/
--- no_error_log
[error]
[crit]



=== TEST 3: proxy_wasm steps - no request body + response body (echo)
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: on_phases
--- config
    location /t {
        proxy_wasm on_phases;
        echo ok;
    }
--- more_headers
Hello: wasm
--- response_body
ok
--- grep_error_log eval: qr/#\d+ on_(request|response|log).*?(?=(, client|\s+while))/
--- grep_error_log_out eval
qr/#\d+ on_request_headers, 3 headers, eof: false
#\d+ on_response_headers, 5 headers, eof: false
#\d+ on_response_body, 3 bytes, eof: false
#\d+ on_response_body, 0 bytes, eof: true
#\d+ on_log/
--- no_error_log
[error]
[crit]



=== TEST 4: proxy_wasm steps - request body + response body (echo)
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: on_phases
--- config
    location /t {
        proxy_wasm on_phases;
        echo ok;
    }
--- request
POST /t

Hello world
--- response_body
ok
--- raw_response_headers_like
HTTP\/1\.1 .*?\r
Content-Type: text\/plain\r
Transfer-Encoding: chunked\r
Connection: close\r
Server: [\S\s]+\r
Date: [\S\s]+\r
--- grep_error_log eval: qr/#\d+ on_(request|response|log).*?(?=(, client|\s+while))/
--- grep_error_log_out eval
qr/#\d+ on_request_headers, 3 headers, eof: false
#\d+ on_request_body, 11 bytes, eof: true
#\d+ on_response_headers, 5 headers, eof: false
#\d+ on_response_body, 3 bytes, eof: false
#\d+ on_response_body, 0 bytes, eof: true
#\d+ on_log/
--- no_error_log
[error]



=== TEST 5: proxy_wasm steps - request body + response body (proxy_pass)
--- wasm_modules: on_phases
--- http_config eval
qq{
    upstream test_upstream {
        server unix:$ENV{TEST_NGINX_UNIX_SOCKET};
    }

    server {
        listen unix:$ENV{TEST_NGINX_UNIX_SOCKET};

        location / {
            return 200 'Hello';
        }
    }
}
--- config
    location /t {
        proxy_wasm on_phases;
        proxy_pass http://test_upstream/;
    }
--- response_body chomp
Hello
--- grep_error_log eval: qr/#\d+ on_(request|response|log).*?(?=(, client|\s+while))/
--- grep_error_log_out eval
qr/#\d+ on_request_headers, 2 headers, eof: false
#\d+ on_response_headers, 5 headers, eof: false
#\d+ on_response_body, 5 bytes, eof: true
#\d+ on_log/
--- no_error_log
[error]
[crit]



=== TEST 6: proxy_wasm steps - request body (body > client_body_buffer_size)
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: on_phases
--- config
    client_body_buffer_size 2;

    location /t {
        proxy_wasm on_phases;
        echo ok;
    }
--- request
POST /t

Larger than a buffer
--- response_body
ok
--- grep_error_log eval: qr/#\d+ on_(request|response|log).*?(?=(, client|\s+while))/
--- grep_error_log_out eval
qr/#\d+ on_request_headers, 3 headers, eof: false
#\d+ on_request_body, 20 bytes, eof: true
#\d+ on_response_headers, 5 headers, eof: false
#\d+ on_response_body, 3 bytes, eof: false
#\d+ on_response_body, 0 bytes, eof: true
#\d+ on_log/
--- no_error_log
[error]
[crit]



=== TEST 7: proxy_wasm steps - request body (client_body_in_file_only)
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: on_phases
--- config
    client_body_in_file_only on;

    location /t {
        proxy_wasm on_phases;
        echo ok;
    }
--- request
POST /t

Hello world
--- response_body
ok
--- grep_error_log eval: qr/#\d+ on_(request|response|log).*?(?=(, client|\s+while))/
--- grep_error_log_out eval
qr/#\d+ on_request_headers, 3 headers, eof: false
#\d+ on_request_body, 11 bytes, eof: true
#\d+ on_response_headers, 5 headers, eof: false
#\d+ on_response_body, 3 bytes, eof: false
#\d+ on_response_body, 0 bytes, eof: true
#\d+ on_log/
--- error_log eval
qr/\[notice\] .*? a client request body is buffered to a temporary file/
--- no_error_log
[error]



=== TEST 8: proxy_wasm steps - default content handler
should cause HTTP 404 from static module (default content handler)
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: on_phases
--- config
    location /t {
        proxy_wasm on_phases;
        echo_status 201;
    }
--- error_code: 404
--- response_body eval
qr/404 Not Found/
--- grep_error_log eval: qr/#\d+ on_(request|response|log).*?(?=(, client|\s+while))/
--- grep_error_log_out eval
qr/#\d+ on_request_headers, 2 headers, eof: false
#\d+ on_response_headers, 5 headers, eof: false
#\d+ on_response_body, \d+ bytes, eof: true
#\d+ on_log/
--- error_log eval
qr/\[error\] .*? open\(\) \".*?\" failed/
--- no_error_log
[crit]



=== TEST 9: proxy_wasm steps - specified before content producer
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: on_phases
--- config
    location /t {
        proxy_wasm on_phases;
        echo ok;
    }
--- error_code: 200
--- response_body
ok
--- grep_error_log eval: qr/#\d+ on_(request|response|log).*?(?=(, client|\s+while))/
--- grep_error_log_out eval
qr/#\d+ on_request_headers, 2 headers, eof: false
#\d+ on_response_headers, 5 headers, eof: false
#\d+ on_response_body, 3 bytes, eof: false
#\d+ on_response_body, 0 bytes, eof: true
#\d+ on_log/
--- no_error_log
[error]
[crit]



=== TEST 10: proxy_wasm steps - specified after content producer
should produce a response from echo, even if proxy_wasm was added
below it, it should wrap around echo
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: on_phases
--- config
    location /t {
        echo ok;
        proxy_wasm on_phases;
    }
--- error_code: 200
--- response_body
ok
--- grep_error_log eval: qr/#\d+ on_(request|response|log).*?(?=(, client|\s+while))/
--- grep_error_log_out eval
qr/#\d+ on_request_headers, 2 headers, eof: false
#\d+ on_response_headers, 5 headers, eof: false
#\d+ on_response_body, 3 bytes, eof: false
#\d+ on_response_body, 0 bytes, eof: true
#\d+ on_log/
--- no_error_log
[error]
[crit]



=== TEST 11: proxy_wasm steps - as a subrequest with return in rewrite
should not execute a log phase
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: on_phases
--- config
    location /subrequest {
        internal;
        proxy_wasm on_phases;
        echo_status 204;
        echo_flush;
    }

    location /t {
        echo_subrequest GET '/subrequest';
    }
--- response_body
--- grep_error_log eval: qr/#\d+ on_(request|response|log).*/
--- grep_error_log_out eval
qr/#\d+ on_request_headers, 2 headers.*? subrequest: "\/subrequest".*
#\d+ on_response_headers, 5 headers.*? subrequest: "\/subrequest".*
#\d+ on_response_body, 0 bytes, eof: true.*/
--- no_error_log
[error]
on_log



=== TEST 12: proxy_wasm steps - as a subrequest with a main request body
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: on_phases
--- config
    location /subrequest {
        internal;
        proxy_wasm on_phases;
        echo ok;
    }

    location /t {
        echo_subrequest GET '/subrequest';
    }
--- request
POST /t

Hello from main request body
--- error_code: 200
--- response_body
ok
--- grep_error_log eval: qr/#\d+ on_(request|response|log).*/
--- grep_error_log_out eval
qr/#\d+ on_request_headers, 3 headers.*? subrequest: "\/subrequest".*
#\d+ on_request_body, 28 bytes, eof: true.*? subrequest: "\/subrequest".*
#\d+ on_response_headers, 5 headers.*? subrequest: "\/subrequest".*
#\d+ on_response_body, 3 bytes, eof: false.*? subrequest: "\/subrequest".*
#\d+ on_response_body, 0 bytes, eof: true.*? subrequest: "\/subrequest".*/
--- no_error_log
[error]
on_log



=== TEST 13: proxy_wasm steps - as a subrequest with a body
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: on_phases
--- config
    location /subrequest {
        proxy_wasm on_phases;
        echo ok;
    }

    location /t {
        echo_subrequest POST '/subrequest' -b 'Hello from subrequest';
    }
--- error_code: 200
--- response_body
ok
--- grep_error_log eval: qr/#\d+ on_(request|response|log).*/
--- grep_error_log_out eval
qr/#\d+ on_request_headers, 3 headers.*? subrequest: "\/subrequest".*
#\d+ on_request_body, 21 bytes, eof: true.*? subrequest: "\/subrequest".*
#\d+ on_response_headers, 5 headers.*? subrequest: "\/subrequest".*
#\d+ on_response_body, 3 bytes, eof: false.*? subrequest: "\/subrequest".*
#\d+ on_response_body, 0 bytes, eof: true.*? subrequest: "\/subrequest".*/
--- no_error_log
[error]
on_log



=== TEST 14: proxy_wasm steps - as a chained subrequest
should invoke wasm ops "done" phase to destroy proxy-wasm ctxid in "content" phase
--- skip_no_debug
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: on_phases
--- config
    location /subrequest {
        internal;
        proxy_wasm on_phases;
        proxy_wasm on_phases;
        echo ok;
    }

    location /t {
        echo_subrequest GET '/subrequest';
        echo_subrequest GET '/subrequest';
    }
--- error_code: 200
--- response_body
ok
ok
--- grep_error_log eval: qr/.*?resuming "on_done" step.*/
--- grep_error_log_out eval
qr#filter 1/2 resuming "on_done" step in "done" phase
.*? filter 2/2 resuming "on_done" step in "done" phase
.*? filter 1/2 resuming "on_done" step in "done" phase
.*? filter 2/2 resuming "on_done" step in "done" phase
\Z#
--- no_error_log
[error]
on_log



=== TEST 15: proxy_wasm steps - as a chained subrequest (logged)
--- skip_no_debug
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: on_phases
--- config
    log_subrequest on;

    location /subrequest {
        proxy_wasm on_phases;
        proxy_wasm on_phases;
        echo ok;
    }

    location /t {
        echo_subrequest GET '/subrequest';
        echo_subrequest GET '/subrequest';
    }
--- error_code: 200
--- response_body
ok
ok
--- grep_error_log eval: qr/\[proxy-wasm\].*? resuming.*/
--- grep_error_log_out eval
qr#filter 1/2 resuming "on_log" step in "log" phase
.*? filter 2/2 resuming "on_log" step in "log" phase
.*? filter 1/2 resuming "on_request_headers" step in "rewrite" phase
.*? filter 2/2 resuming "on_request_headers" step in "rewrite" phase
.*? filter 1/2 resuming "on_response_headers" step in "header_filter" phase
.*? filter 2/2 resuming "on_response_headers" step in "header_filter" phase
.*? filter 1/2 resuming "on_response_body" step in "body_filter" phase
.*? filter 2/2 resuming "on_response_body" step in "body_filter" phase
.*? filter 1/2 resuming "on_response_body" step in "body_filter" phase
.*? filter 2/2 resuming "on_response_body" step in "body_filter" phase
.*? filter 1/2 resuming "on_log" step in "log" phase
.*? filter 2/2 resuming "on_log" step in "log" phase
.*? filter 1/2 resuming "on_done" step in "done" phase
.*? filter 2/2 resuming "on_done" step in "done" phase
.*? filter 1/2 resuming "on_done" step in "done" phase
.*? filter 2/2 resuming "on_done" step in "done" phase
\Z#
--- error_log eval
[
    qr/on_log.*? subrequest: "\/subrequest"/,
    qr/on_log.*? subrequest: "\/subrequest"/,
]



=== TEST 16: proxy_wasm steps - as a parent of several subrequests
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: on_phases
--- config
    location /a {
        internal;
        echo a;
    }

    location /b {
        internal;
        echo bb;
    }

    location /t {
        echo_subrequest GET '/a';
        echo_subrequest GET '/b';
        proxy_wasm on_phases;
    }
--- response_body
a
bb
--- grep_error_log eval: qr/#\d+ on_(request|response|log).*/
--- grep_error_log_out eval
qr/#\d+ on_request_headers, 2 headers.*
#\d+ on_response_headers, 5 headers.*
#\d+ on_response_body, 2 bytes, eof: false.*
#\d+ on_response_body, 3 bytes, eof: false.*
#\d+ on_response_body, 0 bytes, eof: true/
--- no_error_log
[error]
[crit]



=== TEST 17: proxy_wasm steps - same module in multiple location{} blocks
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: on_phases
--- config
    location /subrequest/a {
        proxy_wasm on_phases;
        echo A;
    }

    location /subrequest/b {
        proxy_wasm on_phases;
        echo B;
    }

    location /t {
        echo_subrequest GET '/subrequest/a';
        echo_subrequest GET '/subrequest/b';
    }
--- error_code: 200
--- response_body
A
B
--- grep_error_log eval: qr/#\d+ on_(request|response|log).*/
--- grep_error_log_out eval
qr/#\d+ on_request_headers, 2 headers.*? subrequest: "\/subrequest\/a".*
#\d+ on_response_headers, 5 headers.*? subrequest: "\/subrequest\/a".*
#\d+ on_response_body, 2 bytes, eof: false.*? subrequest: "\/subrequest\/a".*
#\d+ on_response_body, 0 bytes, eof: true.*? subrequest: "\/subrequest\/a".*
#\d+ on_request_headers, 2 headers.*? subrequest: "\/subrequest\/b".*
#\d+ on_response_headers, 5 headers.*? subrequest: "\/subrequest\/b".*
#\d+ on_response_body, 2 bytes, eof: false.*? subrequest: "\/subrequest\/b".*
#\d+ on_response_body, 0 bytes, eof: true.*? subrequest: "\/subrequest\/b".*/
--- no_error_log
[error]
on_log



=== TEST 18: proxy_wasm steps - chained filters in same location{} block
should run each filter after the other within each phase
--- skip_no_debug
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: on_phases
--- config
    location /t {
        proxy_wasm on_phases;
        proxy_wasm on_phases;
        echo ok;
    }
--- request
POST /t
Hello world
--- response_body
ok
--- grep_error_log eval: qr/#\d+ on_(request|response|log).*?(?=(, client|\s+while))/
--- grep_error_log_out eval
qr/#\d+ on_request_headers, 3 headers, eof: false
#\d+ on_request_headers, 3 headers, eof: false
#\d+ on_request_body, 11 bytes, eof: true
#\d+ on_request_body, 11 bytes, eof: true
#\d+ on_response_headers, 5 headers, eof: false
#\d+ on_response_headers, 5 headers, eof: false
#\d+ on_response_body, 3 bytes, eof: false
#\d+ on_response_body, 3 bytes, eof: false
#\d+ on_response_body, 0 bytes, eof: true
#\d+ on_response_body, 0 bytes, eof: true
#\d+ on_log
#\d+ on_log/
--- no_error_log
[error]
[crit]



=== TEST 19: proxy_wasm steps - chained filters in server{} block
should run each filter after the other within each phase
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: on_phases
--- config
    proxy_wasm on_phases;
    proxy_wasm on_phases;

    location /t {
        echo ok;
    }
--- response_body
ok
--- grep_error_log eval: qr/#\d+ on_(request|response|log).*?(?=(, client|\s+while))/
--- grep_error_log_out eval
qr/#\d+ on_request_headers, 2 headers, eof: false
#\d+ on_request_headers, 2 headers, eof: false
#\d+ on_response_headers, 5 headers, eof: false
#\d+ on_response_headers, 5 headers, eof: false
#\d+ on_response_body, 3 bytes, eof: false
#\d+ on_response_body, 3 bytes, eof: false
#\d+ on_response_body, 0 bytes, eof: true.*
#\d+ on_response_body, 0 bytes, eof: true.*
#\d+ on_log
#\d+ on_log/
--- no_error_log
[error]
[crit]



=== TEST 20: proxy_wasm steps - chained filters in http{} block
should run each filter after the other within each phase
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: on_phases
--- http_config
    proxy_wasm on_phases;
    proxy_wasm on_phases;
--- config
    location /t {
        echo ok;
    }
--- response_body
ok
--- grep_error_log eval: qr/#\d+ on_(request|response|log).*?(?=(, client|\s+while))/
--- grep_error_log_out eval
qr/#\d+ on_request_headers, 2 headers, eof: false
#\d+ on_request_headers, 2 headers, eof: false
#\d+ on_response_headers, 5 headers, eof: false
#\d+ on_response_headers, 5 headers, eof: false
#\d+ on_response_body, 3 bytes, eof: false
#\d+ on_response_body, 3 bytes, eof: false
#\d+ on_response_body, 0 bytes, eof: true.*
#\d+ on_response_body, 0 bytes, eof: true.*
#\d+ on_log
#\d+ on_log/
--- no_error_log
[error]
[crit]



=== TEST 21: proxy_wasm steps - mixed filters in server{} and http{} blocks
should run root context of both filters
should not chain in request; instead, server{} overrides http{}
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: on_phases
--- http_config
    proxy_wasm on_phases 'log_msg=http';
--- config
    proxy_wasm on_phases 'log_msg=server';

    location /t {
        echo ok;
    }
--- grep_error_log eval: qr/#\d+ on_(request|response|log).*?(?=(, client|\s+while))/
--- grep_error_log_out eval
qr/#\d+ on_request_headers, 2 headers, eof: false
#\d+ on_response_headers, 5 headers, eof: false
#\d+ on_response_body, 3 bytes, eof: false
#\d+ on_response_body, 0 bytes, eof: true
#\d+ on_log.*/
--- error_log eval
qr/log_msg: server .*? request: "GET \/t\s+/
--- no_error_log eval
[
    qr/log_msg: http .*? request: "GET \/t\s+/,
    qr/\[error\]/
]



=== TEST 22: proxy_wasm steps - mixed filters in server{} and location{} blocks (return in rewrite)
should not chain; instead, location{} overrides server{}
--- wasm_modules: on_phases
--- config
    proxy_wasm on_phases 'log_msg=server';

    location /t {
        proxy_wasm on_phases 'log_msg=location';
        return 200;
    }
--- grep_error_log eval: qr/#\d+ on_(request|response|log).*?(?=(, client|\s+while))/
--- grep_error_log_out eval
qr/#\d+ on_request_headers, 2 headers, eof: false
#\d+ on_response_headers, 5 headers, eof: false
#\d+ on_response_body, 0 bytes, eof: true
#\d+ on_log.*/
--- error_log eval
qr/log_msg: location .*? request: "GET \/t\s+/
--- no_error_log eval
[
    qr/log_msg: server .*? request: "GET \/t\s+/,
    qr/\[error\]/
]



=== TEST 23: proxy_wasm steps - mixed filters in http{}, server{}, and location{} blocks
should not chain; instead, location{} overrides server{}, server{} overrides http{}
--- wasm_modules: on_phases
--- http_config
    proxy_wasm on_phases 'log_msg=http';
--- config
    proxy_wasm on_phases 'log_msg=server';

    location /t {
        proxy_wasm on_phases 'log_msg=location';
        return 200;
    }
--- grep_error_log eval: qr/#\d+ on_(request|response|log).*?(?=(, client|\s+while))/
--- grep_error_log_out eval
qr/#\d+ on_request_headers, 2 headers, eof: false
#\d+ on_response_headers, 5 headers, eof: false
#\d+ on_response_body, 0 bytes, eof: true
#\d+ on_log.*/
--- error_log eval
qr/log_msg: location .*? request: "GET \/t\s+/
--- no_error_log eval
[
    qr/log_msg: server .*? request: "GET \/t\s+/,
    qr/log_msg: http .*? request: "GET \/t\s+/
]
