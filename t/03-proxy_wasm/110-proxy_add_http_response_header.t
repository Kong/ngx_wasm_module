# vim:set ft= ts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

skip_valgrind();

plan tests => repeat_each() * (blocks() * 8);

add_block_preprocessor(sub {
    my $block = shift;

    if (!defined $block->no_error_log) {
        $block->set_value("no_error_log",
                          "[error]\n[crit]\n[emerg]\n[alert]\n[stderr]");
    }
});

run_tests();

__DATA__

=== TEST 1: proxy_wasm - add_http_response_header() adds a new response header
should add a response header visible in the second filter
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'on_phase=http_response_headers test_case=/t/add_http_response_header';
        proxy_wasm hostcalls  on_phase=http_response_headers;
        return 200;
    }
--- request
GET /t/log/response_headers
--- more_headers
pwm-add-resp-header: Hello=world
--- error_log eval
[
    qr/\[wasm\] #\d+ on_response_headers, 4 headers/,
    qr/\[wasm\] #\d+ on_response_headers, 5 headers/,
    qr/\[wasm\] resp Server: nginx.*?/,
    qr/\[wasm\] resp Hello: world/
]
--- no_error_log
[error]
[crit]
[emerg]



=== TEST 2: proxy_wasm - add_http_response_header() adds an already existing header
--- load_nginx_modules: ngx_http_headers_more_filter_module
--- wasm_modules: hostcalls
--- config
    location /t {
        more_set_headers 'Hello: here';
        proxy_wasm hostcalls  on_phase=http_response_headers;
        proxy_wasm hostcalls 'on_phase=http_response_headers test_case=/t/log/response_headers';
        return 200;
    }
--- request
GET /t/add_http_response_header
--- more_headers
pwm-add-resp-header: Hello=there
--- response_headers
Hello: here, there
--- error_log eval
[
    qr/\[wasm\] #\d+ on_response_headers, 5 headers/,
    qr/\[wasm\] #\d+ on_response_headers, 6 headers/,
    qr/\[wasm\] resp Hello: here/,
    qr/\[wasm\] resp Hello: there/,
]
--- no_error_log
[error]
[crit]



=== TEST 3: proxy_wasm - add_http_response_header() adds multiple times the same name/value
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls  on_phase=http_response_headers;
        proxy_wasm hostcalls  on_phase=http_response_headers;
        proxy_wasm hostcalls 'on_phase=http_response_headers test_case=/t/log/response_headers';
        return 200;
    }
--- request
GET /t/add_http_response_header
--- more_headers
pwm-add-resp-header: Hello=world
--- response_headers
Hello: world, world
--- error_log eval
[
    qr/\[wasm\] #\d+ on_response_headers, 4 headers/,
    qr/\[wasm\] #\d+ on_response_headers, 5 headers/,
    qr/\[wasm\] #\d+ on_response_headers, 6 headers/,
    qr/\[wasm\] resp Hello: world/,
    qr/\[wasm\] resp Hello: world/,
]
--- no_error_log
[error]



=== TEST 4: proxy_wasm - add_http_response_header() does not add a new empty value
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls  on_phase=http_response_headers;
        proxy_wasm hostcalls 'on_phase=http_response_headers test_case=/t/log/response_headers';
        return 200;
    }
--- request
GET /t/add_http_response_header
--- more_headers
pwm-add-resp-header: Hello=
--- response_headers
Hello:
--- error_log eval
[
    qr/\[wasm\] #\d+ on_response_headers, 4 headers/,
    qr/\[wasm\] #\d+ on_response_headers, 4 headers/,
]
--- no_error_log eval
[
    qr/\[wasm\] resp Hello: world/,
    qr/\[error\]/,
    qr/\[crit\]/,
    qr/\[emerg\]/,
]



=== TEST 5: proxy_wasm - add_http_response_header() clears Content-Length header with empty value
--- load_nginx_modules: ngx_http_headers_more_filter_module
--- wasm_modules: hostcalls
--- config
    location /t {
        more_set_headers 'Content-Length: 12';
        proxy_wasm hostcalls  on_phase=http_response_headers;
        proxy_wasm hostcalls 'on_phase=log test_case=/t/log/response_headers';
        return 200;
    }
--- request
GET /t/add_http_response_header
--- more_headers
pwm-add-resp-header: Content-Length=
--- response_headers
Content-Length:
--- grep_error_log eval: qr/\[wasm\] .*?(#\d+ entering "\S+"|resp\s).*?(?=\s+<)/
--- grep_error_log_out eval
qr/\[wasm\] .*? entering "HttpResponseHeaders"
\[wasm\] .*? entering "Log"
\[wasm\] resp Server: nginx.*?
\[wasm\] resp Date: .*? GMT/



=== TEST 6: proxy_wasm - add_http_response_header() updates Content-Length header with 0 value
--- load_nginx_modules: ngx_http_headers_more_filter_module
--- wasm_modules: hostcalls
--- config
    location /t {
        more_set_headers 'Content-Length: 12';
        proxy_wasm hostcalls  on_phase=http_response_headers;
        proxy_wasm hostcalls 'on_phase=log test_case=/t/log/response_headers';
        return 200;
    }
--- request
GET /t/add_http_response_header
--- more_headers
pwm-add-resp-header: Content-Length=0
--- response_headers
Content-Length: 0
--- grep_error_log eval: qr/\[wasm\] .*?(#\d+ entering "\S+"|resp\s).*?(?=\s+<)/
--- grep_error_log_out eval
qr/\[wasm\] .*? entering "HttpResponseHeaders"
\[wasm\] .*? entering "Log"
\[wasm\] resp Content-Length: 0
\[wasm\] resp Server: nginx.*?
\[wasm\] resp Date: .*? GMT/



=== TEST 7: proxy_wasm - add_http_response_header() adds Content-Length header
also adds Content-Type due to echo
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls  on_phase=http_response_headers;
        proxy_wasm hostcalls 'on_phase=log test_case=/t/log/response_headers';
        echo ok;
    }
--- request
GET /t/add_http_response_header
--- more_headers
pwm-add-resp-header: Content-Length=3
--- response_headers
Content-Length: 3
--- grep_error_log eval: qr/\[wasm\] .*?(#\d+ entering "\S+"|resp\s).*?(?=\s+<)/
--- grep_error_log_out eval
qr/\[wasm\] .*? entering "HttpResponseHeaders"
\[wasm\] .*? entering "Log"
\[wasm\] resp Server: nginx.*?
\[wasm\] resp Date: .*? GMT
\[wasm\] resp Content-Type: text\/plain
\[wasm\] resp Content-Length: 3/



=== TEST 8: proxy_wasm - add_http_response_header() updates existing Content-Length header
--- load_nginx_modules: ngx_http_echo_module ngx_http_headers_more_filter_module
--- wasm_modules: hostcalls
--- config
    location /t {
        more_set_headers 'Content-Length: 12';
        proxy_wasm hostcalls  on_phase=http_response_headers;
        proxy_wasm hostcalls 'on_phase=log test_case=/t/log/response_headers';
        echo ok;
    }
--- request
GET /t/add_http_response_header
--- more_headers
pwm-add-resp-header: Content-Length=3
--- response_headers
Content-Length: 3
--- ignore_response_body
--- grep_error_log eval: qr/\[wasm\] .*?(#\d+ entering "\S+"|resp\s).*?(?=\s+<)/
--- grep_error_log_out eval
qr/\[wasm\] .*? entering "HttpResponseHeaders"
\[wasm\] .*? entering "Log"
\[wasm\] resp Content-Length: 3
\[wasm\] resp Server: nginx.*?
\[wasm\] resp Date: .*? GMT/



=== TEST 9: proxy_wasm - add_http_response_header() catches invalid Content-Length header
should produce an error
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
stub
[crit]
[emerg]
[alert]
[stderr]



=== TEST 10: proxy_wasm - add_http_response_header() adds a builtin multi header (Cache-Control)
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
pwm-add-resp-header: Cache-Control=no-cache
--- response_headers
Cache-Control: no-cache
--- ignore_response_body
--- grep_error_log eval: qr/\[wasm\] .*?(#\d+ entering "\S+"|resp\s).*?(?=\s+<)/
--- grep_error_log_out eval
qr/\[wasm\] .*? entering "HttpResponseHeaders"
\[wasm\] .*? entering "Log"
\[wasm\] resp Server: nginx.*?
\[wasm\] resp Date: .*? GMT
\[wasm\] resp Content-Type: text\/plain
\[wasm\] resp Cache-Control: no-cache/



=== TEST 11: proxy_wasm - add_http_response_header() adds an already existing builtin multi header (Cache-Control)
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
--- ignore_response_body
--- grep_error_log eval: qr/\[wasm\] .*?(#\d+ entering "\S+"|resp\s).*?(?=\s+<)/
--- grep_error_log_out eval
qr/\[wasm\] .*? entering "HttpResponseHeaders"
\[wasm\] .*? entering "Log"
\[wasm\] resp Cache-Control: no-store
\[wasm\] resp Server: nginx.*?
\[wasm\] resp Date: .*? GMT
\[wasm\] resp Content-Type: text\/plain/



=== TEST 12: proxy_wasm - add_http_response_header() does not add an empty bultin multi header (Cache-Control)
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
pwm-add-resp-header: Cache-Control=
--- response_headers
Cache-Control: no-store
--- ignore_response_body
--- grep_error_log eval: qr/\[wasm\] .*?(#\d+ entering "\S+"|resp\s).*?(?=\s+<)/
--- grep_error_log_out eval
qr/\[wasm\] .*? entering "HttpResponseHeaders"
\[wasm\] .*? entering "Log"
\[wasm\] resp Cache-Control: no-store
\[wasm\] resp Server: nginx.*?
\[wasm\] resp Date: .*? GMT
\[wasm\] resp Content-Type: text\/plain/
