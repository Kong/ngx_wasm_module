# vim:set ft= ts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

skip_valgrind();

plan tests => repeat_each() * (blocks() * 5);

add_block_preprocessor(sub {
    my $block = shift;

    if (!defined $block->no_error_log) {
        $block->set_value("no_error_log",
                          "[error]\n[crit]\n[alert]");
    }
});

run_tests();

__DATA__

=== TEST 1: proxy_wasm - add_http_response_header() adds a new non-builtin header
should add a response header visible in the second filter
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'on_phase=http_response_headers test_case=/t/add_http_response_header';
        proxy_wasm hostcalls 'on_phase=http_response_headers test_case=/t/log/response_headers';
        return 200;
    }
--- more_headers
pwm-add-resp-header: Hello=world
--- grep_error_log eval: qr/\[wasm\].*? ((#\d+ on_response_headers)|resp Hello).*/
--- grep_error_log_out eval
qr/\[wasm\] #\d+ on_response_headers, 5 headers
\[wasm\] #\d+ on_response_headers, 6 headers
\[wasm\] resp Hello: world .*?
\z/



=== TEST 2: proxy_wasm - add_http_response_header() adds a new non-builtin header with empty value
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'on_phase=http_response_headers test_case=/t/add_http_response_header';
        proxy_wasm hostcalls 'on_phase=http_response_headers test_case=/t/log/response_headers';
        return 200;
    }
--- more_headers
pwm-add-resp-header: Hello=
--- response_headers
Hello:
--- grep_error_log eval: qr/\[wasm\].*? ((#\d+ on_response_headers)|resp Hello).*/
--- grep_error_log_out eval
qr/\[wasm\] #\d+ on_response_headers, 5 headers
\[wasm\] #\d+ on_response_headers, 6 headers
\[wasm\] resp Hello:\s .*?
\z/
--- no_error_log
[error]
[crit]



=== TEST 3: proxy_wasm - add_http_response_header() adds an existing non-builtin header
--- load_nginx_modules: ngx_http_headers_more_filter_module
--- wasm_modules: hostcalls
--- config
    location /t {
        more_set_headers 'Hello: here';
        proxy_wasm hostcalls 'on_phase=http_response_headers test_case=/t/add_http_response_header';
        proxy_wasm hostcalls 'on_phase=http_response_headers test_case=/t/log/response_headers';
        return 200;
    }
--- more_headers
pwm-add-resp-header: Hello=there
--- response_headers
Hello: here, there
--- ignore_response_body
--- grep_error_log eval: qr/\[wasm\].*? ((#\d+ on_response_headers)|resp Hello).*/
--- grep_error_log_out eval
qr/\[wasm\] #\d+ on_response_headers, 6 headers
\[wasm\] #\d+ on_response_headers, 7 headers
\[wasm\] resp Hello: here .*?
\[wasm\] resp Hello: there .*?
\z/
--- no_error_log
[error]
[crit]



=== TEST 4: proxy_wasm - add_http_response_header() adds an existing non-builtin header with empty value
--- load_nginx_modules: ngx_http_headers_more_filter_module
--- wasm_modules: hostcalls
--- config
    location /t {
        more_set_headers 'Hello: here';
        proxy_wasm hostcalls 'on_phase=http_response_headers test_case=/t/add_http_response_header';
        proxy_wasm hostcalls 'on_phase=http_response_headers test_case=/t/log/response_headers';
        return 200;
    }
--- more_headers
pwm-add-resp-header: Hello=
--- response_headers_like
Hello: here,\s
--- ignore_response_body
--- grep_error_log eval: qr/\[wasm\].*? ((#\d+ on_response_headers)|resp Hello).*/
--- grep_error_log_out eval
qr/\[wasm\] #\d+ on_response_headers, 6 headers
\[wasm\] #\d+ on_response_headers, 7 headers
\[wasm\] resp Hello: here .*?
\[wasm\] resp Hello:\s .*?
\z/
--- no_error_log
[error]
[crit]



=== TEST 5: proxy_wasm - add_http_response_header() adds the same non-builtin header/value multiple times
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'on_phase=http_response_headers test_case=/t/add_http_response_header';
        proxy_wasm hostcalls 'on_phase=http_response_headers test_case=/t/add_http_response_header';
        proxy_wasm hostcalls 'on_phase=http_response_headers test_case=/t/log/response_headers';
        return 200;
    }
--- more_headers
pwm-add-resp-header: Hello=world
--- response_headers
Hello: world, world
--- grep_error_log eval: qr/\[wasm\].*? ((#\d+ on_response_headers)|resp Hello).*/
--- grep_error_log_out eval
qr/\[wasm\] #\d+ on_response_headers, 5 headers
\[wasm\] #\d+ on_response_headers, 6 headers
\[wasm\] #\d+ on_response_headers, 7 headers
\[wasm\] resp Hello: world .*?
\[wasm\] resp Hello: world .*?
\z/
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
        proxy_wasm hostcalls 'on_phase=http_response_headers test_case=/t/add_http_response_header';
        proxy_wasm hostcalls 'on_phase=log test_case=/t/log/response_headers';
        echo ok;
    }
--- more_headers
pwm-add-resp-header: Content-Length=3
--- response_headers
Content-Length: 3
--- grep_error_log eval: qr/\[wasm\].*? ((#\d+ on_response_headers)|resp Content-Length).*/
--- grep_error_log_out eval
qr/\[wasm\] #\d+ on_response_headers, 5 headers
\[wasm\] #\d+ on_response_headers, 5 headers
\[wasm\] resp Content-Length: 3 .*?
\z/
--- no_error_log
[error]
[crit]



=== TEST 7: proxy_wasm - add_http_response_header() adds a new Content-Length header with 0 value
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'on_phase=http_response_headers test_case=/t/add_http_response_header';
        proxy_wasm hostcalls 'on_phase=log test_case=/t/log/response_headers';
        echo ok;
    }
--- more_headers
pwm-add-resp-header: Content-Length=0
--- response_headers
Content-Length: 0
--- grep_error_log eval: qr/\[wasm\].*? ((#\d+ on_response_headers)|resp Content-Length).*/
--- grep_error_log_out eval
qr/\[wasm\] #\d+ on_response_headers, 5 headers
\[wasm\] #\d+ on_response_headers, 5 headers
\[wasm\] resp Content-Length: 0 .*?
\z/
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
        proxy_wasm hostcalls 'on_phase=http_response_headers test_case=/t/add_http_response_header';
        proxy_wasm hostcalls 'on_phase=log test_case=/t/log/response_headers';
        return 200;
    }
--- more_headers
pwm-add-resp-header: Content-Length=3
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
        proxy_wasm hostcalls 'on_phase=http_response_headers test_case=/t/add_http_response_header';
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
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls  on_phase=http_response_headers;
        proxy_wasm hostcalls 'on_phase=log test_case=/t/log/response_headers';
        return 200;
    }
--- request
GET /t/add_http_response_header
--- more_headers
pwm-add-resp-header: Content-Length=FF
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
        proxy_wasm hostcalls 'on_phase=http_response_headers test_case=/t/add_http_response_header';
        proxy_wasm hostcalls 'on_phase=log test_case=/t/log/response_headers';
        return 200;
    }
--- more_headers
pwm-add-resp-header: Cache-Control=no-cache
--- response_headers
Cache-Control: no-cache
--- response_body
--- grep_error_log eval: qr/\[wasm\].*? ((#\d+ on_response_headers)|resp Cache-Control).*/
--- grep_error_log_out eval
qr/\[wasm\] #\d+ on_response_headers, 5 headers
\[wasm\] #\d+ on_response_headers, 6 headers
\[wasm\] resp Cache-Control: no-cache .*?
\z/
--- no_error_log
[error]



=== TEST 12: proxy_wasm - add_http_response_header() adds an existing builtin multi header (Cache-Control)
--- load_nginx_modules: ngx_http_headers_more_filter_module
--- wasm_modules: hostcalls
--- config
    location /t {
        more_set_headers 'Cache-Control: no-store';
        proxy_wasm hostcalls  on_phase=http_response_headers;
        proxy_wasm hostcalls 'on_phase=log test_case=/t/log/response_headers';
        return 200;
    }
--- request
GET /t/add_http_response_header
--- more_headers
pwm-add-resp-header: Cache-Control=no-cache
--- response_headers
Cache-Control: no-store, no-cache
--- response_body
--- grep_error_log eval: qr/\[wasm\].*? ((#\d+ on_response_headers)|resp Cache-Control).*/
--- grep_error_log_out eval
qr/\[wasm\] #\d+ on_response_headers, 6 headers
\[wasm\] #\d+ on_response_headers, 7 headers
\[wasm\] resp Cache-Control: no-store .*?
\[wasm\] resp Cache-Control: no-cache .*?
\z/
--- no_error_log
[error]



=== TEST 13: proxy_wasm - add_http_response_header() adds an existing builtin multi header (Cache-Control) with empty value
--- load_nginx_modules: ngx_http_headers_more_filter_module
--- wasm_modules: hostcalls
--- config
    location /t {
        more_set_headers 'Cache-Control: no-store';
        proxy_wasm hostcalls 'on_phase=http_response_headers test_case=/t/add_http_response_header';
        proxy_wasm hostcalls 'on_phase=log test_case=/t/log/response_headers';
        return 200;
    }
--- more_headers
pwm-add-resp-header: Cache-Control=
--- response_headers_like
Cache-Control: no-store,\s
--- response_body
--- grep_error_log eval: qr/\[wasm\].*? ((#\d+ on_response_headers)|resp Cache-Control).*/
--- grep_error_log_out eval
qr/\[wasm\] #\d+ on_response_headers, 6 headers
\[wasm\] #\d+ on_response_headers, 7 headers
\[wasm\] resp Cache-Control: no-store .*?
\[wasm\] resp Cache-Control:\s .*?
\z/
--- no_error_log
[error]



=== TEST 14: proxy_wasm - add_http_response_header() recycles an existing builtin multi header (Cache-Control)
coverage only
--- load_nginx_modules: ngx_http_headers_more_filter_module
--- wasm_modules: hostcalls
--- config
    location /t {
        more_set_headers 'Cache-Control: no-store';
        proxy_wasm hostcalls 'on_phase=http_response_headers test_case=/t/set_http_response_headers';
        proxy_wasm hostcalls 'on_phase=http_response_headers test_case=/t/add_http_response_header';
        proxy_wasm hostcalls 'on_phase=log test_case=/t/log/response_headers';
        return 200;
    }
--- more_headers
pwm-add-resp-header: Cache-Control=no-cache
--- response_headers
Cache-Control: no-cache
--- response_body
--- grep_error_log eval: qr/\[wasm\].*? ((#\d+ on_response_headers)|resp Cache-Control).*/
--- grep_error_log_out eval
qr/\[wasm\] #\d+ on_response_headers, 6 headers
\[wasm\] #\d+ on_response_headers, 5 headers
\[wasm\] #\d+ on_response_headers, 6 headers
\[wasm\] resp Cache-Control: no-cache .*?
\z/
--- no_error_log
[error]
