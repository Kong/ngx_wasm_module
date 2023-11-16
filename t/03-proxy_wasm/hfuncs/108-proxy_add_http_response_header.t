# vim:set ft= ts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

add_block_preprocessor(sub {
    my $block = shift;

    if (!defined $block->no_error_log) {
        $block->set_value("no_error_log",
                          "[error]\n[crit]\n[alert]");
    }
});

plan_tests(5);
run_tests();

__DATA__

=== TEST 1: proxy_wasm - add_http_response_header() adds a new non-builtin header
should add a response header visible in the second filter
--- valgrind
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'on=response_headers \
                              test=/t/add_response_header \
                              value=Hello:world';
        proxy_wasm hostcalls 'on=response_headers \
                              test=/t/log/response_headers';
        return 200;
    }
--- grep_error_log eval: qr/(\[hostcalls\] on_response_headers|resp Hello).*/
--- grep_error_log_out eval
qr/.*? on_response_headers, 5 headers.*
.*? on_response_headers, 6 headers.*
resp Hello: world.*/



=== TEST 2: proxy_wasm - add_http_response_header() adds a new non-builtin header with empty value
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'on=response_headers \
                              test=/t/add_response_header
                              value=Hello:';
        proxy_wasm hostcalls 'on=response_headers \
                              test=/t/log/response_headers';
        return 200;
    }
--- response_headers
Hello:
--- grep_error_log eval: qr/(\[hostcalls\] on_response_headers|resp Hello).*/
--- grep_error_log_out eval
qr/.*? on_response_headers, 5 headers.*
.*? on_response_headers, 6 headers.*
resp Hello:\s.*/
--- no_error_log
[error]
[crit]



=== TEST 3: proxy_wasm - add_http_response_header() adds an existing non-builtin header
--- load_nginx_modules: ngx_http_headers_more_filter_module
--- wasm_modules: hostcalls
--- config
    location /t {
        more_set_headers 'Hello: here';
        proxy_wasm hostcalls 'on=response_headers \
                              test=/t/add_response_header \
                              value=Hello:there';
        proxy_wasm hostcalls 'on=response_headers \
                              test=/t/log/response_headers';
        return 200;
    }
--- response_headers
Hello: here, there
--- ignore_response_body
--- grep_error_log eval: qr/(\[hostcalls\] on_response_headers|resp Hello).*/
--- grep_error_log_out eval
qr/.*? on_response_headers, 6 headers.*
.*? on_response_headers, 7 headers.*
resp Hello: here.*
resp Hello: there.*/
--- no_error_log
[error]
[crit]



=== TEST 4: proxy_wasm - add_http_response_header() adds an existing non-builtin header with empty value
--- load_nginx_modules: ngx_http_headers_more_filter_module
--- wasm_modules: hostcalls
--- config
    location /t {
        more_set_headers 'Hello: here';
        proxy_wasm hostcalls 'on=response_headers \
                              test=/t/add_response_header \
                              value=Hello:';
        proxy_wasm hostcalls 'on=response_headers \
                              test=/t/log/response_headers';
        return 200;
    }
--- response_headers_like
Hello: here,\s
--- ignore_response_body
--- grep_error_log eval: qr/(\[hostcalls\] on_response_headers|resp Hello).*/
--- grep_error_log_out eval
qr/.*? on_response_headers, 6 headers.*
.*? on_response_headers, 7 headers.*
resp Hello: here.*
resp Hello:\s.*/
--- no_error_log
[error]
[crit]



=== TEST 5: proxy_wasm - add_http_response_header() adds the same non-builtin header/value multiple times
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'on=response_headers \
                              test=/t/add_response_header \
                              value=Hello:world';
        proxy_wasm hostcalls 'on=response_headers \
                              test=/t/add_response_header \
                              value=Hello:world';
        proxy_wasm hostcalls 'on=response_headers \
                              test=/t/log/response_headers';
        return 200;
    }
--- response_headers
Hello: world, world
--- grep_error_log eval: qr/(\[hostcalls\] on_response_headers|resp Hello).*/
--- grep_error_log_out eval
qr/.*? on_response_headers, 6 headers.*
.*? on_response_headers, 7 headers.*
resp Hello: world.*
resp Hello: world.*/
--- no_error_log
[error]
[crit]



=== TEST 6: proxy_wasm - add_http_response_header() adds a new Content-Length header
5 headers before add: TE, no CL
5 headers after add: CL replaced TE
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'on=response_headers \
                              test=/t/add_response_header \
                              value=Content-Length:3';
        proxy_wasm hostcalls 'on=log \
                              test=/t/log/response_headers';
        echo ok;
    }
--- response_headers
Content-Length: 3
--- grep_error_log eval: qr/(\[hostcalls\] on_response_headers|resp Content-Length).*/
--- grep_error_log_out eval
qr/.*? on_response_headers, 5 headers.*
.*? on_response_headers, 5 headers.*
resp Content-Length: 3.*/
--- no_error_log
[error]
[crit]



=== TEST 7: proxy_wasm - add_http_response_header() adds a new Content-Length header with 0 value
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'on=response_headers \
                              test=/t/add_response_header \
                              value=Content-Length:0';
        proxy_wasm hostcalls 'on=log \
                              test=/t/log/response_headers';
        echo ok;
    }
--- response_headers
Content-Length: 0
--- grep_error_log eval: qr/(\[hostcalls\] on_response_headers|resp Content-Length).*/
--- grep_error_log_out eval
qr/.*? on_response_headers, 5 headers.*
.*? on_response_headers, 5 headers.*
resp Content-Length: 0.*/
--- no_error_log
[error]
[crit]



=== TEST 8: proxy_wasm - add_http_response_header() cannot add Content-Length header if exists
should log an error but not produce a trap
--- load_nginx_modules: ngx_http_headers_more_filter_module
--- wasm_modules: hostcalls
--- config
    location /t {
        more_set_headers 'Content-Length: 0';
        proxy_wasm hostcalls 'on=response_headers \
                              test=/t/add_response_header \
                              value=Content-Length:3';
        proxy_wasm hostcalls 'on=log \
                              test=/t/log/response_headers';
        return 200;
    }
--- response_body
--- error_log eval
qr/\[error\] .*? \[wasm\] cannot add new "Content-Length" builtin response header/
--- no_error_log
[crit]
[alert]



=== TEST 9: proxy_wasm - add_http_response_header() cannot add empty Content-Length
should log an error but not produce a trap
--- load_nginx_modules: ngx_http_headers_more_filter_module
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'on=response_headers \
                              test=/t/add_response_header \
                              value=Content-Length:';
        return 200;
    }
--- more_headers
pwm-add-resp-header: Content-Length=
--- response_body
--- error_log eval
qr/\[error\] .*? \[wasm\] attempt to set invalid Content-Length response header: ""/
--- no_error_log
[crit]
[alert]



=== TEST 10: proxy_wasm - add_http_response_header() cannot add invalid Content-Length header
should log an error but not produce a trap
--- valgrind
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'on=response_headers \
                              test=/t/add_response_header \
                              value=Content-Length:FF';
        proxy_wasm hostcalls 'on=log \
                              test=/t/log/response_headers';
        return 200;
    }
--- response_headers
Content-Length: 0
--- error_log eval
qr/\[error\] .*? \[wasm\] attempt to set invalid Content-Length response header: "FF"/
--- no_error_log
[crit]
[alert]



=== TEST 11: proxy_wasm - add_http_response_header() adds a builtin multi header (Cache-Control)
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'on=response_headers \
                              test=/t/add_response_header \
                              value=Cache-Control:no-cache';
        proxy_wasm hostcalls 'on=log \
                              test=/t/log/response_headers';
        return 200;
    }
--- response_headers
Cache-Control: no-cache
--- response_body
--- grep_error_log eval: qr/(\[hostcalls\] on_response_headers|resp Cache-Control).*/
--- grep_error_log_out eval
qr/.*? on_response_headers, 5 headers.*
.*? on_response_headers, 6 headers.*
resp Cache-Control: no-cache.*/
--- no_error_log
[error]



=== TEST 12: proxy_wasm - add_http_response_header() adds an existing builtin multi header (Cache-Control)
--- load_nginx_modules: ngx_http_headers_more_filter_module
--- wasm_modules: hostcalls
--- config
    location /t {
        more_set_headers 'Cache-Control: no-store';
        proxy_wasm hostcalls 'on=response_headers \
                              test=/t/add_response_header \
                              value=Cache-Control:no-cache';
        proxy_wasm hostcalls 'on=log \
                              test=/t/log/response_headers';
        return 200;
    }
--- response_headers
Cache-Control: no-store, no-cache
--- response_body
--- grep_error_log eval: qr/(\[hostcalls\] on_response_headers|resp Cache-Control).*/
--- grep_error_log_out eval
qr/.*? on_response_headers, 6 headers.*
.*? on_response_headers, 7 headers.*
resp Cache-Control: no-store.*
resp Cache-Control: no-cache.*/
--- no_error_log
[error]



=== TEST 13: proxy_wasm - add_http_response_header() adds an existing builtin multi header (Cache-Control) with empty value
--- load_nginx_modules: ngx_http_headers_more_filter_module
--- wasm_modules: hostcalls
--- config
    location /t {
        more_set_headers 'Cache-Control: no-store';
        proxy_wasm hostcalls 'on=response_headers \
                              test=/t/add_response_header \
                              value=Cache-Control:';
        proxy_wasm hostcalls 'on=log \
                              test=/t/log/response_headers';
        return 200;
    }
--- response_headers_like
Cache-Control: no-store,\s
--- response_body
--- grep_error_log eval: qr/(\[hostcalls\] on_response_headers|resp Cache-Control).*/
--- grep_error_log_out eval
qr/.*? on_response_headers, 6 headers.*
.*? on_response_headers, 7 headers.*
resp Cache-Control: no-store.*
resp Cache-Control:\s.*/
--- no_error_log
[error]



=== TEST 14: proxy_wasm - add_http_response_header() recycles an existing builtin multi header (Cache-Control)
coverage only
--- load_nginx_modules: ngx_http_headers_more_filter_module
--- wasm_modules: hostcalls
--- config
    location /t {
        more_set_headers 'Cache-Control: no-store';
        proxy_wasm hostcalls 'on=response_headers \
                              test=/t/set_response_headers';
        proxy_wasm hostcalls 'on=response_headers \
                              test=/t/add_response_header \
                              value=Cache-Control:no-cache';
        proxy_wasm hostcalls 'on=log \
                              test=/t/log/response_headers';
        return 200;
    }
--- response_headers
Cache-Control: no-cache
--- response_body
--- grep_error_log eval: qr/(\[hostcalls\] on_response_headers|resp Cache-Control).*/
--- grep_error_log_out eval
qr/.*? on_response_headers, 6 headers.*
.*? on_response_headers, 5 headers.*
.*? on_response_headers, 6 headers.*
resp Cache-Control: no-cache.*/
--- no_error_log
[error]



=== TEST 15: proxy_wasm - add_http_response_header() x on_phases
should log an error (but no trap) when headers are sent
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'on=request_headers \
                              test=/t/add_response_header \
                              value=From:request_headers';

        proxy_wasm hostcalls 'on=response_headers \
                              test=/t/add_response_header \
                              value=From:response_headers';

        proxy_wasm hostcalls 'on=response_body \
                              test=/t/add_response_header \
                              value=From:response_body';

        proxy_wasm hostcalls 'on=log \
                              test=/t/add_response_header \
                              value=From:log';
        return 200;
    }
--- response_headers
From: request_headers, response_headers
--- ignore_response_body
--- grep_error_log eval: qr/\[(error|hostcalls)\] [^on_].*/
--- grep_error_log_out eval
qr/.*? testing in "RequestHeaders".*
.*? testing in "ResponseHeaders".*
.*? testing in "ResponseBody".*
\[error\] .*? \[wasm\] cannot add response header: headers already sent.*
.*? testing in "Log".*
\[error\] .*? \[wasm\] cannot add response header: headers already sent.*/
--- no_error_log
[crit]
[emerg]
