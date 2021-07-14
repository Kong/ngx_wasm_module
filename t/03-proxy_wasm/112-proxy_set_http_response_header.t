# vim:set ft= ts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

skip_valgrind();

plan tests => repeat_each() * (blocks() * 5);

run_tests();

__DATA__

=== TEST 1: proxy_wasm - set_http_response_header() sets a new non-builtin header
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'on=response_headers \
                              test=/t/set_response_header \
                              value=Hello:world';
        proxy_wasm hostcalls 'on=response_headers \
                              test=/t/log/response_headers';
        return 200;
    }
--- response_headers
Hello: world
--- grep_error_log eval: qr/\[wasm\].*? ((#\d+ on_response_headers)|resp Hello).*/
--- grep_error_log_out eval
qr/\[wasm\] #\d+ on_response_headers, 5 headers
\[wasm\] #\d+ on_response_headers, 6 headers
\[wasm\] resp Hello: world .*?
\z/
--- no_error_log
[error]
[crit]



=== TEST 2: proxy_wasm - set_http_response_header() sets a new non-builtin header (case-sensitive)
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'on=response_headers \
                              test=/t/set_response_header \
                              value=hello:world';
        proxy_wasm hostcalls 'on=response_headers \
                              test=/t/log/response_headers';
        return 200;
    }
--- response_headers
Hello: world
--- grep_error_log eval: qr/\[wasm\].*? ((#\d+ on_response_headers)|resp [Hh]ello).*/
--- grep_error_log_out eval
qr/\[wasm\] #\d+ on_response_headers, 5 headers
\[wasm\] #\d+ on_response_headers, 6 headers
\[wasm\] resp hello: world .*?
\z/
--- no_error_log
[error]
[crit]



=== TEST 3: proxy_wasm - set_http_response_header() removes an existing non-builtin header when no value
--- load_nginx_modules: ngx_http_headers_more_filter_module
--- wasm_modules: hostcalls
--- config
    location /t {
        more_set_headers 'Hello: here';
        proxy_wasm hostcalls 'on=response_headers \
                              test=/t/set_response_header \
                              value=Hello:';
        proxy_wasm hostcalls 'on=response_headers \
                              test=/t/log/response_headers';
        return 200;
    }
--- response_headers_like
Hello:
--- grep_error_log eval: qr/\[wasm\].*? ((#\d+ on_response_headers)|resp [Hh]ello).*/
--- grep_error_log_out eval
qr/\[wasm\] #\d+ on_response_headers, 6 headers
\[wasm\] #\d+ on_response_headers, 5 headers
\z/
--- no_error_log
[error]
[crit]



=== TEST 4: proxy_wasm - set_http_response_header() sets the same non-builtin header multiple times
--- load_nginx_modules: ngx_http_headers_more_filter_module
--- wasm_modules: hostcalls
--- config
    location /t {
        more_set_headers 'Hello: here';
        proxy_wasm hostcalls 'on=response_headers \
                              test=/t/set_response_header \
                              value=Hello:wasm';
        proxy_wasm hostcalls 'on=response_headers \
                              test=/t/set_response_header \
                              value=Hello:wasm';
        proxy_wasm hostcalls 'on=response_headers \
                              test=/t/log/response_headers';
        return 200;
    }
--- response_headers
Hello: wasm
--- grep_error_log eval: qr/\[wasm\].*? ((#\d+ on_response_headers)|resp Hello).*/
--- grep_error_log_out eval
qr/\[wasm\] #\d+ on_response_headers, 6 headers
\[wasm\] #\d+ on_response_headers, 6 headers
\[wasm\] #\d+ on_response_headers, 6 headers
\[wasm\] resp Hello: wasm .*?
\z/
--- no_error_log
[error]
[crit]



=== TEST 5: proxy_wasm - set_http_response_header() sets the same non-builtin header multiple times (case-sensitive)
--- load_nginx_modules: ngx_http_headers_more_filter_module
--- wasm_modules: hostcalls
--- config
    location /t {
        more_set_headers 'Hello: here';
        proxy_wasm hostcalls 'on=response_headers \
                              test=/t/set_response_header \
                              value=Hello:wasm';
        proxy_wasm hostcalls 'on=response_headers \
                              test=/t/set_response_header \
                              value=hello:wasm';
        proxy_wasm hostcalls 'on=response_headers \
                              test=/t/log/response_headers';
        return 200;
    }
--- response_headers
hello: wasm
--- grep_error_log eval: qr/\[wasm\].*? ((#\d+ on_response_headers)|resp [Hh]ello).*/
--- grep_error_log_out eval
qr/\[wasm\] #\d+ on_response_headers, 6 headers
\[wasm\] #\d+ on_response_headers, 6 headers
\[wasm\] #\d+ on_response_headers, 6 headers
\[wasm\] resp hello: wasm .*?
\z/
--- no_error_log
[error]
[crit]



=== TEST 6: proxy_wasm - set_http_response_header() sets Connection header (close)
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'on=response_headers \
                              test=/t/set_response_header \
                              value=Connection:close';
        proxy_wasm hostcalls 'on=response_headers \
                              test=/t/log/response_headers';
        return 200;
    }
--- more_headers
Connection: keep-alive
--- response_headers
Connection: close
--- grep_error_log eval: qr/\[wasm\].*? ((#\d+ on_response_headers)|resp Connection).*/
--- grep_error_log_out eval
qr/\[wasm\] #\d+ on_response_headers, 5 headers
\[wasm\] #\d+ on_response_headers, 5 headers
\[wasm\] resp Connection: close .*?
\z/
--- no_error_log
[error]
[crit]



=== TEST 7: proxy_wasm - set_http_response_header() sets Connection header (upgrade)
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'on=response_headers \
                              test=/t/set_response_header \
                              value=Connection:upgrade';
        proxy_wasm hostcalls 'on=response_headers \
                              test=/t/log/response_headers';
        return 200;
    }
--- error_code: 101
--- response_headers
Connection: upgrade
--- error_log eval
qr/\[wasm\] setting "Connection: upgrade" response header, switching status code: 101/
--- grep_error_log eval: qr/\[wasm\].*? ((#\d+ on_response_headers)|resp Connection).*/
--- grep_error_log_out eval
qr/\[wasm\] #\d+ on_response_headers, 5 headers
\[wasm\] #\d+ on_response_headers, 5 headers
\[wasm\] resp Connection: upgrade .*?
\z/
--- no_error_log
[error]



=== TEST 8: proxy_wasm - set_http_response_header() sets Connection header (keep-alive)
--- abort
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'on=response_headers \
                              test=/t/set_response_header \
                              value=Connection:keep-alive';

        proxy_wasm hostcalls 'on=response_headers \
                              test=/t/log/response_headers';

        proxy_wasm hostcalls 'on=log \
                              test=/t/log/response_headers';
        return 200;
    }
--- more_headers
Connection: close
--- response_headers
Connection: keep-alive
--- grep_error_log eval: qr/\[wasm\].*? ((#\d+ entering)|resp Connection).*/
--- grep_error_log_out eval
qr/\[wasm\] #\d+ entering "ResponseHeaders" .*?
\[wasm\] #\d+ entering "ResponseHeaders" .*?
\[wasm\] resp Connection: keep-alive .*?
\[wasm\] #\d+ entering "Log" .*?
\[wasm\] resp Connection: keep-alive .*/
--- no_error_log
[error]
[crit]



=== TEST 9: proxy_wasm - set_http_response_header() cannot set invalid Connection header
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'on=response_headers \
                              test=/t/set_response_header \
                              value=Connection:unknown';
        return 200;
    }
--- more_headers
Connection: close
--- response_body
--- response_headers
Connection: close
--- error_log eval
qr/\[error\] .*? \[wasm\] attempt to set invalid Connection response header: "unknown"/
--- grep_error_log eval: qr/\[wasm\] #\d+ (on_response_[a-z]+).*/
--- grep_error_log_out eval
qr/\[wasm\] #\d+ on_response_headers, 5 headers
\[wasm\] #\d+ on_response_body, 0 bytes, end_of_stream true
\z/



=== TEST 10: proxy_wasm - set_http_response_header() removes Last-Modified header when no value
--- load_nginx_modules: ngx_http_headers_more_filter_module
--- wasm_modules: hostcalls
--- config
    location /t {
        more_set_headers 'Last-Modified: Tue, 15 Nov 1994 12:45:26 GMT';
        proxy_wasm hostcalls 'on=response_headers \
                              test=/t/set_response_header \
                              value=Last-Modified:';
        proxy_wasm hostcalls 'on=response_headers \
                              test=/t/log/response_headers';
        return 200;
    }
--- response_headers
Last-Modified:
--- grep_error_log eval: qr/\[wasm\].*? ((#\d+ on_response_headers)|resp Last-Modified).*/
--- grep_error_log_out eval
qr/\[wasm\] #\d+ on_response_headers, 6 headers
\[wasm\] #\d+ on_response_headers, 5 headers
\z/
--- no_error_log
[error]
[crit]



=== TEST 11: proxy_wasm - set_http_response_header() sets an existing builtin multi header (Cache-Control)
--- load_nginx_modules: ngx_http_headers_more_filter_module
--- wasm_modules: hostcalls
--- config
    location /t {
        more_set_headers 'Cache-Control: no-store';
        proxy_wasm hostcalls 'on=response_headers \
                              test=/t/set_response_header \
                              value=Cache-Control:no-cache';
        proxy_wasm hostcalls 'on=response_headers \
                              test=/t/log/response_headers';
        return 200;
    }
--- more_headers
pwm-set-resp-header: Cache-Control=no-cache
--- response_headers
Cache-Control: no-cache
--- response_body
--- grep_error_log eval: qr/\[wasm\].*? ((#\d+ on_response_headers)|resp Cache-Control).*/
--- grep_error_log_out eval
qr/\[wasm\] #\d+ on_response_headers, 6 headers
\[wasm\] #\d+ on_response_headers, 6 headers
\[wasm\] resp Cache-Control: no-cache .*?
\z/
--- no_error_log
[error]



=== TEST 12: proxy_wasm - set_http_response_header() x on_phases
should log an error (but no trap) when headers are sent
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'on=request_headers \
                              test=/t/set_response_header \
                              value=From:request_headers';

        proxy_wasm hostcalls 'on=response_headers \
                              test=/t/set_response_header \
                              value=From:response_headers';

        proxy_wasm hostcalls 'on=response_body \
                              test=/t/set_response_header \
                              value=From:response_body';

        proxy_wasm hostcalls 'on=log \
                              test=/t/set_response_header \
                              value=From:log';
        return 200;
    }
--- response_headers
From: response_headers
--- ignore_response_body
--- grep_error_log eval: qr/\[(error|info)\] .*? \[wasm\] .*/
--- grep_error_log_out eval
qr/.*?
\[info\] .*? \[wasm\] #\d+ entering "RequestHeaders" .*?
\[info\] .*? \[wasm\] #\d+ entering "ResponseHeaders" .*?
\[info\] .*? \[wasm\] #\d+ entering "Log" .*?
\[error\] .*? \[wasm\] cannot set response header: headers already sent/
--- no_error_log
[warn]
[crit]
