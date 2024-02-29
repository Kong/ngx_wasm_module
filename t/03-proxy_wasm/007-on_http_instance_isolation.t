# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasmX;

skip_no_debug();

plan_tests(8);
no_shuffle();
run_tests();

__DATA__

=== TEST 1: proxy_wasm - default isolation mode (none)
should use a global instance reused across streams
--- valgrind
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/log/current_time';
        proxy_wasm hostcalls 'test=/t/log/current_time';
        return 200;
    }
--- request eval
["GET /t", "GET /t"]
--- ignore_response_body
--- grep_error_log eval: qr/(\*\d+.*?(resuming|new instance|reusing|finalizing|freeing|trap in)|#\d+ on_(configure|vm_start)).*/
--- grep_error_log_out eval
[qr/#0 on_vm_start[^#*]*
#0 on_configure[^#*]*
#0 on_vm_start[^#*]*
#0 on_configure[^#*]*
\*\d+ .*? filter reusing instance[^#*]*
\*\d+ .*? filter reusing instance[^#*]*
\*\d+ .*? filter 1\/2 resuming "on_request_headers" step in "rewrite" phase[^#*]*
\*\d+ .*? filter 2\/2 resuming "on_request_headers" step in "rewrite" phase[^#*]*
\*\d+ .*? filter 1\/2 resuming "on_response_headers" step in "header_filter" phase[^#*]*
\*\d+ .*? filter 2\/2 resuming "on_response_headers" step in "header_filter" phase[^#*]*
\*\d+ .*? filter 1\/2 resuming "on_response_body" step in "body_filter" phase[^#*]*
\*\d+ .*? filter 2\/2 resuming "on_response_body" step in "body_filter" phase[^#*]*
\*\d+ .*? filter 1\/2 resuming "on_log" step in "log" phase[^#*]*
\*\d+ .*? filter 2\/2 resuming "on_log" step in "log" phase[^#*]*
\*\d+ .*? filter 1\/2 resuming "on_done" step in "done" phase[^#*]*
\*\d+ .*? filter 1\/2 finalizing context[^#*]*
\*\d+ .*? filter 2\/2 resuming "on_done" step in "done" phase[^#*]*
\*\d+ .*? filter 2\/2 finalizing context[^#*]*
\*\d+ .*? filter freeing context #\d+ \(1\/2\)[^#*]*
\*\d+ .*? filter freeing context #\d+ \(2\/2\)\Z/,
qr/\A\*\d+ .*? filter reusing instance[^#*]*
\*\d+ .*? filter reusing instance[^#*]*
\*\d+ .*? filter 1\/2 resuming "on_request_headers" step in "rewrite" phase[^#*]*
\*\d+ .*? filter 2\/2 resuming "on_request_headers" step in "rewrite" phase[^#*]*
\*\d+ .*? filter 1\/2 resuming "on_response_headers" step in "header_filter" phase[^#*]*
\*\d+ .*? filter 2\/2 resuming "on_response_headers" step in "header_filter" phase[^#*]*
\*\d+ .*? filter 1\/2 resuming "on_response_body" step in "body_filter" phase[^#*]*
\*\d+ .*? filter 2\/2 resuming "on_response_body" step in "body_filter" phase[^#*]*
\*\d+ .*? filter 1\/2 resuming "on_log" step in "log" phase[^#*]*
\*\d+ .*? filter 2\/2 resuming "on_log" step in "log" phase[^#*]*
\*\d+ .*? filter 1\/2 resuming "on_done" step in "done" phase[^#*]*
\*\d+ .*? filter 1\/2 finalizing context[^#*]*
\*\d+ .*? filter 2\/2 resuming "on_done" step in "done" phase[^#*]*
\*\d+ .*? filter 2\/2 finalizing context[^#*]*
\*\d+ .*? filter freeing context #\d+ \(1\/2\)[^#*]*
\*\d+ .*? filter freeing context #\d+ \(2\/2\)\Z/]
--- no_error_log
[error]
[crit]



=== TEST 2: proxy_wasm - trap with none isolation mode
Should recycle the global instance when trapped.
--- valgrind
--- wasm_modules: hostcalls
--- config
    proxy_wasm_isolation none;

    location /t {
        proxy_wasm hostcalls;
        proxy_wasm hostcalls 'on=log test=/t/log/current_time';
        return 204;
    }
--- request eval
["GET /t/trap", "GET /t/send_local_response/status/204"]
--- error_code eval
[500, 204]
--- ignore_response_body
--- grep_error_log eval: qr/(\*\d.*?(resuming|new instance|reusing|finalizing|freeing|now)|(.*?unreachable)).*/
--- grep_error_log_out eval
[qr/\*\d+ .*? filter reusing instance[^#*]*
\*\d+ .*? filter reusing instance[^#*]*
\*\d+ .*? filter 1\/2 resuming "on_request_headers" step in "rewrite" phase[^#*]*
(.*?(Uncaught RuntimeError: )?unreachable|\s*wasm trap: wasm `unreachable` instruction executed)[^#*]*
\*\d+ .*? filter chain failed resuming: previous error \(instance trapped\)[^#*]*
\*\d+ .*? filter freeing context #\d+ \(1\/2\)[^#*]*
\*\d+ .*? filter freeing context #\d+ \(2\/2\)[^#*]*\Z/,
qr/\A\*\d+ .*? filter new instance[^#*]*
\*\d+ .*? filter reusing instance[^#*]*
\*\d+ .*? filter 1\/2 resuming "on_request_headers" step in "rewrite" phase[^#*]*
\*\d+ .*? filter 1\/2 resuming "on_response_headers" step in "header_filter" phase[^#*]*
\*\d+ .*? filter 2\/2 resuming "on_response_headers" step in "header_filter" phase[^#*]*
\*\d+ .*? filter 1\/2 resuming "on_log" step in "log" phase[^#*]*
\*\d+ .*? filter 2\/2 resuming "on_log" step in "log" phase[^#*]*
\*\d+ .*? now: .*? while logging request[^#*]*
\*\d+ .*? filter 1\/2 resuming "on_done" step in "done" phase[^#*]*
\*\d+ .*? filter 1\/2 finalizing context[^#*]*
\*\d+ .*? filter 2\/2 resuming "on_done" step in "done" phase[^#*]*
\*\d+ .*? filter 2\/2 finalizing context[^#*]*
\*\d+ .*? filter freeing context #\d+ \(1\/2\)[^#*]*
\*\d+ wasm freeing "hostcalls" instance in "main" vm[^#*]*
\*\d+ .*? filter freeing context #\d+ \(2\/2\)\Z/]
--- no_error_log
[emerg]
[alert]



=== TEST 3: proxy_wasm - stream isolation mode
should use an instance per stream
req0 might free an instance from the previous test in HUP mode.
--- valgrind
--- wasm_modules: hostcalls
--- config
    proxy_wasm_isolation stream;

    location /t {
        proxy_wasm hostcalls 'test=/t/log/current_time';
        proxy_wasm hostcalls 'test=/t/log/current_time';
        return 200;
    }
--- request eval
["GET /t", "GET /t"]
--- ignore_response_body
--- grep_error_log eval: qr/(\*\d+.*?(resuming|new instance|reusing|finalizing|freeing|trap in)|#\d+ on_(configure|vm_start)).*/
--- grep_error_log_out eval
[qr/(\*\d+ .*? freeing "hostcalls" instance in "main" vm \(.*?\)[^#*]*)?#0 on_vm_start[^#*]*
#0 on_configure[^#*]*
#0 on_vm_start[^#*]*
#0 on_configure[^#*]*(\*\d+ .*? freeing "hostcalls" instance in "main" vm \(.*?\)[^#*]*)?
\*\d+ .*? filter new instance[^#*]*
#0 on_configure[^#*]*
\*\d+ .*? filter reusing instance[^#*]*
#0 on_configure[^#*]*
\*\d+ .*? filter 1\/2 resuming "on_request_headers" step in "rewrite" phase[^#*]*
\*\d+ .*? filter 2\/2 resuming "on_request_headers" step in "rewrite" phase[^#*]*
\*\d+ .*? filter 1\/2 resuming "on_response_headers" step in "header_filter" phase[^#*]*
\*\d+ .*? filter 2\/2 resuming "on_response_headers" step in "header_filter" phase[^#*]*
\*\d+ .*? filter 1\/2 resuming "on_response_body" step in "body_filter" phase[^#*]*
\*\d+ .*? filter 2\/2 resuming "on_response_body" step in "body_filter" phase[^#*]*
\*\d+ .*? filter 1\/2 resuming "on_log" step in "log" phase[^#*]*
\*\d+ .*? filter 2\/2 resuming "on_log" step in "log" phase[^#*]*
\*\d+ .*? filter 1\/2 resuming "on_done" step in "done" phase[^#*]*
\*\d+ .*? filter 1\/2 finalizing context[^#*]*
\*\d+ .*? filter 2\/2 resuming "on_done" step in "done" phase[^#*]*
\*\d+ .*? filter 2\/2 finalizing context[^#*]*
\*\d+ .*? freeing "hostcalls" instance in "main" vm[^#*]*
\*\d+ .*? filter freeing context #\d+ \(1\/2\)[^#*]*
\*\d+ .*? filter freeing context #\d+ \(2\/2\)[^#*]*\Z/,
qr/\A\*\d+ .*? filter new instance[^#*]*
#0 on_configure[^#*]*
\*\d+ .*? filter reusing instance[^#*]*
#0 on_configure[^#*]*
\*\d+ .*? filter 1\/2 resuming "on_request_headers" step in "rewrite" phase[^#*]*
\*\d+ .*? filter 2\/2 resuming "on_request_headers" step in "rewrite" phase[^#*]*
\*\d+ .*? filter 1\/2 resuming "on_response_headers" step in "header_filter" phase[^#*]*
\*\d+ .*? filter 2\/2 resuming "on_response_headers" step in "header_filter" phase[^#*]*
\*\d+ .*? filter 1\/2 resuming "on_response_body" step in "body_filter" phase[^#*]*
\*\d+ .*? filter 2\/2 resuming "on_response_body" step in "body_filter" phase[^#*]*
\*\d+ .*? filter 1\/2 resuming "on_log" step in "log" phase[^#*]*
\*\d+ .*? filter 2\/2 resuming "on_log" step in "log" phase[^#*]*
\*\d+ .*? filter 1\/2 resuming "on_done" step in "done" phase[^#*]*
\*\d+ .*? filter 1\/2 finalizing context[^#*]*
\*\d+ .*? filter 2\/2 resuming "on_done" step in "done" phase[^#*]*
\*\d+ .*? filter 2\/2 finalizing context[^#*]*
\*\d+ .*? freeing "hostcalls" instance in "main" vm[^#*]*
\*\d+ .*? filter freeing context #\d+ \(1\/2\)[^#*]*
\*\d+ .*? filter freeing context #\d+ \(2\/2\)[^#*]*\Z/]
--- no_error_log
[error]
[crit]



=== TEST 4: proxy_wasm - trap with stream isolation mode
--- valgrind
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    proxy_wasm_isolation stream;

    location /t {
        proxy_wasm hostcalls;
        proxy_wasm hostcalls 'on=log test=/t/log/current_time';
        echo_status 200;
    }
--- request eval
["GET /t/trap", "GET /t/send_local_response/status/204"]
--- error_code eval
[500, 204]
--- ignore_response_body
--- grep_error_log eval: qr/(\*\d.*?(resuming|new instance|reusing|finalizing context|freeing|now)|(.*?unreachable)).*/
--- grep_error_log_out eval
[qr/\*\d+ .*? filter new instance[^#*]*
\*\d+ .*? filter reusing instance[^#*]*
\*\d+ .*? filter 1\/2 resuming "on_request_headers" step in "rewrite" phase[^#*]*
(.*?(Uncaught RuntimeError: )?unreachable|\s*wasm trap: wasm `unreachable` instruction executed)[^#*]*
\*\d+ .*? filter chain failed resuming: previous error \(instance trapped\)[^#*]*
\*\d+ .*? freeing "hostcalls" instance in "main" vm[^#*]*
\*\d+ .*? filter freeing context #\d+ \(1\/2\)[^#*]*
\*\d+ .*? filter freeing context #\d+ \(2\/2\)[^#*]*\Z/,
qr/\A\*\d+ .*? filter new instance[^#*]*
\*\d+ .*? filter reusing instance[^#*]*
\*\d+ .*? filter 1\/2 resuming "on_request_headers" step in "rewrite" phase[^#*]*
\*\d+ .*? filter 1\/2 resuming "on_response_headers" step in "header_filter" phase[^#*]*
\*\d+ .*? filter 2\/2 resuming "on_response_headers" step in "header_filter" phase[^#*]*
\*\d+ .*? filter 1\/2 resuming "on_log" step in "log" phase[^#*]*
\*\d+ .*? filter 2\/2 resuming "on_log" step in "log" phase[^#*]*
\*\d+ .*? now: .*? while logging request[^#*]*
\*\d+ .*? filter 1\/2 resuming "on_done" step in "done" phase[^#*]*
\*\d+ .*? filter 1\/2 finalizing context[^#*]*
\*\d+ .*? filter 2\/2 resuming "on_done" step in "done" phase[^#*]*
\*\d+ .*? filter 2\/2 finalizing context[^#*]*
\*\d+ .*? freeing "hostcalls" instance in "main" vm[^#*]*
\*\d+ .*? filter freeing context #\d+ \(1\/2\)[^#*]*
\*\d+ .*? filter freeing context #\d+ \(2\/2\)[^#*]*\Z/]
--- no_error_log
[emerg]
[alert]



=== TEST 5: proxy_wasm - globals with default isolation mode (none)
GET /t
on_request_headers A: 123000 + 1
on_request_headers B: 123001 + 1
on_log A: 123002
on_log B: 123002
GET /t
on_request_headers A: 123002 + 1
on_request_headers B: 123003 + 1
on_log A: 123004
on_log B: 123004
--- wasm_modules: instance_lifecycle
--- config
    location /t {
        # A
        proxy_wasm instance_lifecycle;
        # B
        proxy_wasm instance_lifecycle;
        return 200;
    }
--- request eval
["GET /t", "GET /t"]
--- error_code eval
[200, 200]
--- ignore_response_body
--- grep_error_log eval: qr/\*\d+.*?on_log: MY_STATIC_VARIABLE: \d+/
--- grep_error_log_out eval
[qr/\A.*?on_log: MY_STATIC_VARIABLE: 123002
.*?on_log: MY_STATIC_VARIABLE: 123002\Z/,
qr/\A.*?on_log: MY_STATIC_VARIABLE: 123004
.*?on_log: MY_STATIC_VARIABLE: 123004\Z/]
--- no_error_log
[error]
[crit]



=== TEST 6: proxy_wasm - globals with stream isolation mode
GET /t
on_request_headers A: 123000 + 1
on_request_headers B: 123001 + 1
on_log A: 123002
on_log B: 123002
GET /t
on_request_headers A: 123000 + 1
on_request_headers B: 123001 + 1
on_log A: 123002
on_log B: 123002
--- wasm_modules: instance_lifecycle
--- config
    proxy_wasm_isolation stream;

    location /t {
        # A
        proxy_wasm instance_lifecycle;
        # B
        proxy_wasm instance_lifecycle;
        return 200;
    }
--- request eval
["GET /t", "GET /t"]
--- error_code eval
[200, 200]
--- ignore_response_body
--- grep_error_log eval: qr/\*\d+.*?on_log: MY_STATIC_VARIABLE: \d+/
--- grep_error_log_out eval
[qr/\A.*?on_log: MY_STATIC_VARIABLE: 123002
.*?on_log: MY_STATIC_VARIABLE: 123002\Z/,
qr/\A.*?on_log: MY_STATIC_VARIABLE: 123002
.*?on_log: MY_STATIC_VARIABLE: 123002\Z/]
--- no_error_log
[error]
[crit]



=== TEST 7: proxy_wasm - globals with filter isolation mode
GET /t
on_request_headers A: 123000 + 1
on_request_headers B: 123000 + 1
on_log A: 123001
on_log B: 123001
GET /t
on_request_headers A: 123000 + 1
on_request_headers B: 123000 + 1
on_log A: 123001
on_log B: 123001
--- valgrind
--- wasm_modules: instance_lifecycle
--- config
    proxy_wasm_isolation filter;

    location /t {
        # A
        proxy_wasm instance_lifecycle;
        # B
        proxy_wasm instance_lifecycle;
        return 200;
    }
--- request eval
["GET /t", "GET /t"]
--- error_code eval
[200, 200]
--- ignore_response_body
--- grep_error_log eval: qr/\*\d+.*?on_log: MY_STATIC_VARIABLE: \d+/
--- grep_error_log_out eval
[qr/\A.*?on_log: MY_STATIC_VARIABLE: 123001
.*?on_log: MY_STATIC_VARIABLE: 123001\Z/,
qr/\A.*?on_log: MY_STATIC_VARIABLE: 123001
.*?on_log: MY_STATIC_VARIABLE: 123001\Z/]
--- no_error_log
[error]
[crit]
