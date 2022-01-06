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
--- grep_error_log eval: qr/(resp Hello|on_response_headers,).*/
--- grep_error_log_out eval
qr/on_response_headers, 5 headers.*
on_response_headers, 6 headers.*
resp Hello: world.*/
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
--- grep_error_log eval: qr/(resp hello|on_response_headers,).*/
--- grep_error_log_out eval
qr/on_response_headers, 5 headers.*
on_response_headers, 6 headers.*
resp hello: world.*/
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
--- grep_error_log eval: qr/(resp [Hh]ello|on_response_headers,).*/
--- grep_error_log_out eval
qr/on_response_headers, 6 headers.*
on_response_headers, 5 headers.*
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
--- grep_error_log eval: qr/(resp [Hh]ello|on_response_headers,).*/
--- grep_error_log_out eval
qr/on_response_headers, 6 headers.*
on_response_headers, 6 headers.*
on_response_headers, 6 headers.*
resp Hello: wasm.*/
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
--- grep_error_log eval: qr/(resp [Hh]ello|on_response_headers,).*/
--- grep_error_log_out eval
qr/on_response_headers, 6 headers.*
on_response_headers, 6 headers.*
on_response_headers, 6 headers.*
resp hello: wasm.*/
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
--- grep_error_log eval: qr/(resp Connection|on_response_headers,).*/
--- grep_error_log_out eval
qr/on_response_headers, 5 headers.*
on_response_headers, 5 headers.*
resp Connection: close.*/
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
--- grep_error_log eval: qr/(resp Connection|on_response_headers,).*/
--- grep_error_log_out eval
qr/on_response_headers, 5 headers.*
on_response_headers, 5 headers.*
resp Connection: upgrade.*/
--- no_error_log
[error]



=== TEST 8: proxy_wasm - set_http_response_header() sets Connection header (keep-alive)
--- timeout_no_valgrind: 20s
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
--- grep_error_log eval: qr/(resp Connection|testing in).*/
--- grep_error_log_out eval
qr/testing in "ResponseHeaders".*
testing in "ResponseHeaders".*
resp Connection: keep-alive.*
testing in "Log".*
resp Connection: keep-alive.*/
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
--- grep_error_log eval: qr/\[hostcalls\] on_response_[a-z]+.*/
--- grep_error_log_out eval
qr/.*? on_response_headers, 5 headers.*
.*? on_response_body, 0 bytes, end_of_stream true.*/



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
--- grep_error_log eval: qr/(resp Last-Modified|on_response_headers,).*/
--- grep_error_log_out eval
qr/on_response_headers, 6 headers.*
on_response_headers, 5 headers.*/
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
--- grep_error_log eval: qr/(resp Cache-Control|on_response_headers,).*/
--- grep_error_log_out eval
qr/on_response_headers, 6 headers.*
on_response_headers, 6 headers.*
resp Cache-Control: no-cache.*/
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
--- grep_error_log eval: qr/\[(error|hostcalls)\] [^on_].*/
--- grep_error_log_out eval
qr/.*? testing in "RequestHeaders".*
.*? testing in "ResponseHeaders".*
.*? testing in "ResponseBody".*
\[error\] .*? \[wasm\] cannot set response header: headers already sent.*
.*? testing in "Log".*
\[error\] .*? \[wasm\] cannot set response header: headers already sent.*/
--- no_error_log
[warn]
[crit]
