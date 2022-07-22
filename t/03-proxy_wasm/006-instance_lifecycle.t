# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

skip_no_debug();
skip_valgrind('wasmtime');

plan tests => repeat_each() * (blocks() * 8);

run_tests();

__DATA__

=== TEST 1: proxy_wasm - default isolation mode (none)
should use a global instance reused across streams
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
--- grep_error_log eval: qr/\*\d+.*?(resuming|new instance|reusing|finalizing|freeing).*/
--- grep_error_log_out eval
[qr/.*?\*\d+ proxy_wasm "hostcalls" filter \(1\/2\) reusing instance.*
\*\d+ proxy_wasm "hostcalls" filter \(2\/2\) reusing instance.*
\*\d+ proxy_wasm "hostcalls" filter \(1\/2\) resuming in "rewrite" phase.*
\*\d+ proxy_wasm "hostcalls" filter \(2\/2\) resuming in "rewrite" phase.*
\*\d+ proxy_wasm "hostcalls" filter \(1\/2\) resuming in "rewrite" phase.*
\*\d+ proxy_wasm "hostcalls" filter \(2\/2\) resuming in "rewrite" phase.*
\*\d+ proxy_wasm "hostcalls" filter \(1\/2\) resuming in "header_filter" phase.*
\*\d+ proxy_wasm "hostcalls" filter \(2\/2\) resuming in "header_filter" phase.*
\*\d+ proxy_wasm "hostcalls" filter \(1\/2\) resuming in "body_filter" phase.*
\*\d+ proxy_wasm "hostcalls" filter \(2\/2\) resuming in "body_filter" phase.*
\*\d+ proxy_wasm "hostcalls" filter \(1\/2\) resuming in "log" phase.*
\*\d+ proxy_wasm "hostcalls" filter \(2\/2\) resuming in "log" phase.*
\*\d+ proxy_wasm "hostcalls" filter \(1\/2\) resuming in "done" phase.*
\*\d+ proxy_wasm "hostcalls" filter \(1\/2\) finalizing context
\*\d+ proxy_wasm "hostcalls" filter \(2\/2\) resuming in "done" phase.*
\*\d+ proxy_wasm "hostcalls" filter \(2\/2\) finalizing context
\*\d+ proxy_wasm freeing stream context #\d+ \(main: 1\)\Z/,
qr/\A\*\d+ proxy_wasm "hostcalls" filter \(1\/2\) reusing instance.*
\*\d+ proxy_wasm "hostcalls" filter \(2\/2\) reusing instance.*
\*\d+ proxy_wasm "hostcalls" filter \(1\/2\) resuming in "rewrite" phase.*
\*\d+ proxy_wasm "hostcalls" filter \(2\/2\) resuming in "rewrite" phase.*
\*\d+ proxy_wasm "hostcalls" filter \(1\/2\) resuming in "rewrite" phase.*
\*\d+ proxy_wasm "hostcalls" filter \(2\/2\) resuming in "rewrite" phase.*
\*\d+ proxy_wasm "hostcalls" filter \(1\/2\) resuming in "header_filter" phase.*
\*\d+ proxy_wasm "hostcalls" filter \(2\/2\) resuming in "header_filter" phase.*
\*\d+ proxy_wasm "hostcalls" filter \(1\/2\) resuming in "body_filter" phase.*
\*\d+ proxy_wasm "hostcalls" filter \(2\/2\) resuming in "body_filter" phase.*
\*\d+ proxy_wasm "hostcalls" filter \(1\/2\) resuming in "log" phase.*
\*\d+ proxy_wasm "hostcalls" filter \(2\/2\) resuming in "log" phase.*
\*\d+ proxy_wasm "hostcalls" filter \(1\/2\) resuming in "done" phase.*
\*\d+ proxy_wasm "hostcalls" filter \(1\/2\) finalizing context
\*\d+ proxy_wasm "hostcalls" filter \(2\/2\) resuming in "done" phase.*
\*\d+ proxy_wasm "hostcalls" filter \(2\/2\) finalizing context
\*\d+ proxy_wasm freeing stream context #\d+ \(main: 1\)\Z/]
--- no_error_log
[error]
[crit]



=== TEST 2: proxy_wasm - trap with none isolation mode
should recycle the global instance when trapped
--- load_nginx_modules: ngx_http_echo_module
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
--- grep_error_log eval: qr/\*\d+.*?(resuming|new instance|reusing|finalizing|freeing|now|trap in).*/
--- grep_error_log_out eval
[qr/.*?\*\d+ proxy_wasm "hostcalls" filter \(1\/2\) reusing instance.*
\*\d+ proxy_wasm "hostcalls" filter \(2\/2\) reusing instance.*
\*\d+ proxy_wasm "hostcalls" filter \(1\/2\) resuming in "rewrite" phase.*
\*\d+ proxy_wasm "hostcalls" filter \(2\/2\) resuming in "rewrite" phase.*
\*\d+ proxy_wasm "hostcalls" filter \(1\/2\) resuming in "rewrite" phase.*
\*\d+ \[wasm\] trap in proxy_on_request_headers:.*?unreachable.*
\*\d+ \[wasm\] proxy_wasm "hostcalls" filter \(1\/2\) failed resuming \(instance trapped\).*
\*\d+ proxy_wasm freeing stream context #\d+ \(main: 1\).*\Z/,
qr/\A\*\d+ wasm freeing "hostcalls" instance in "main" vm.*
\*\d+ proxy_wasm "hostcalls" filter \(1\/2\) new instance.*
\*\d+ proxy_wasm "hostcalls" filter \(2\/2\) reusing instance.*
\*\d+ proxy_wasm "hostcalls" filter \(1\/2\) resuming in "rewrite" phase.*
\*\d+ proxy_wasm "hostcalls" filter \(2\/2\) resuming in "rewrite" phase.*
\*\d+ proxy_wasm "hostcalls" filter \(1\/2\) resuming in "rewrite" phase.*
\*\d+ proxy_wasm "hostcalls" filter \(1\/2\) resuming in "header_filter" phase.*
\*\d+ proxy_wasm "hostcalls" filter \(2\/2\) resuming in "header_filter" phase.*
\*\d+ proxy_wasm "hostcalls" filter \(1\/2\) resuming in "log" phase.*
\*\d+ proxy_wasm "hostcalls" filter \(2\/2\) resuming in "log" phase.*
\*\d+ now: .*? while logging request.*
\*\d+ proxy_wasm "hostcalls" filter \(1\/2\) resuming in "done" phase.*
\*\d+ proxy_wasm "hostcalls" filter \(1\/2\) finalizing context
\*\d+ proxy_wasm "hostcalls" filter \(2\/2\) resuming in "done" phase.*
\*\d+ proxy_wasm "hostcalls" filter \(2\/2\) finalizing context
\*\d+ proxy_wasm freeing stream context #\d+ \(main: 1\).*\Z/]
--- no_error_log
[emerg]
[alert]



=== TEST 3: proxy_wasm - stream isolation mode
should use an instance per stream
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
--- grep_error_log eval: qr/\*\d+.*?(resuming|new instance|reusing|finalizing|freeing).*/
--- grep_error_log_out eval
[qr/.*?\*\d+ proxy_wasm "hostcalls" filter \(1\/2\) new instance.*
\*\d+ proxy_wasm "hostcalls" filter \(2\/2\) reusing instance.*
\*\d+ proxy_wasm "hostcalls" filter \(1\/2\) resuming in "rewrite" phase.*
\*\d+ proxy_wasm "hostcalls" filter \(2\/2\) resuming in "rewrite" phase.*
\*\d+ proxy_wasm "hostcalls" filter \(1\/2\) resuming in "rewrite" phase.*
\*\d+ proxy_wasm "hostcalls" filter \(2\/2\) resuming in "rewrite" phase.*
\*\d+ proxy_wasm "hostcalls" filter \(1\/2\) resuming in "header_filter" phase.*
\*\d+ proxy_wasm "hostcalls" filter \(2\/2\) resuming in "header_filter" phase.*
\*\d+ proxy_wasm "hostcalls" filter \(1\/2\) resuming in "body_filter" phase.*
\*\d+ proxy_wasm "hostcalls" filter \(2\/2\) resuming in "body_filter" phase.*
\*\d+ proxy_wasm "hostcalls" filter \(1\/2\) resuming in "log" phase.*
\*\d+ proxy_wasm "hostcalls" filter \(2\/2\) resuming in "log" phase.*
\*\d+ proxy_wasm "hostcalls" filter \(1\/2\) resuming in "done" phase.*
\*\d+ proxy_wasm "hostcalls" filter \(1\/2\) finalizing context
\*\d+ proxy_wasm "hostcalls" filter \(2\/2\) resuming in "done" phase.*
\*\d+ proxy_wasm "hostcalls" filter \(2\/2\) finalizing context
\*\d+ proxy_wasm freeing stream context #\d+ \(main: 1\)
\*\d+ wasm freeing "hostcalls" instance in "main" vm.*\Z/,
qr/\A\*\d+ proxy_wasm "hostcalls" filter \(1\/2\) new instance.*
\*\d+ proxy_wasm "hostcalls" filter \(2\/2\) reusing instance.*
\*\d+ proxy_wasm "hostcalls" filter \(1\/2\) resuming in "rewrite" phase.*
\*\d+ proxy_wasm "hostcalls" filter \(2\/2\) resuming in "rewrite" phase.*
\*\d+ proxy_wasm "hostcalls" filter \(1\/2\) resuming in "rewrite" phase.*
\*\d+ proxy_wasm "hostcalls" filter \(2\/2\) resuming in "rewrite" phase.*
\*\d+ proxy_wasm "hostcalls" filter \(1\/2\) resuming in "header_filter" phase.*
\*\d+ proxy_wasm "hostcalls" filter \(2\/2\) resuming in "header_filter" phase.*
\*\d+ proxy_wasm "hostcalls" filter \(1\/2\) resuming in "body_filter" phase.*
\*\d+ proxy_wasm "hostcalls" filter \(2\/2\) resuming in "body_filter" phase.*
\*\d+ proxy_wasm "hostcalls" filter \(1\/2\) resuming in "log" phase.*
\*\d+ proxy_wasm "hostcalls" filter \(2\/2\) resuming in "log" phase.*
\*\d+ proxy_wasm "hostcalls" filter \(1\/2\) resuming in "done" phase.*
\*\d+ proxy_wasm "hostcalls" filter \(1\/2\) finalizing context
\*\d+ proxy_wasm "hostcalls" filter \(2\/2\) resuming in "done" phase.*
\*\d+ proxy_wasm "hostcalls" filter \(2\/2\) finalizing context
\*\d+ proxy_wasm freeing stream context #\d+ \(main: 1\)
\*\d+ wasm freeing "hostcalls" instance in "main" vm.*\Z/]
--- no_error_log
[error]
[crit]



=== TEST 4: proxy_wasm - trap with stream isolation mode
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
--- grep_error_log eval: qr/\*\d+.*?(resuming|new instance|reusing|finalizing context|freeing|now|trap in).*/
--- grep_error_log_out eval
[qr/.*?\*\d+ proxy_wasm "hostcalls" filter \(1\/2\) new instance.*
\*\d+ proxy_wasm "hostcalls" filter \(2\/2\) reusing instance.*
\*\d+ proxy_wasm "hostcalls" filter \(1\/2\) resuming in "rewrite" phase.*
\*\d+ proxy_wasm "hostcalls" filter \(2\/2\) resuming in "rewrite" phase.*
\*\d+ proxy_wasm "hostcalls" filter \(1\/2\) resuming in "rewrite" phase.*
\*\d+ \[wasm\] trap in proxy_on_request_headers:.*?unreachable.*
\*\d+ \[wasm\] proxy_wasm "hostcalls" filter \(1\/2\) failed resuming \(instance trapped\).*
\*\d+ proxy_wasm freeing stream context #\d+ \(main: 1\)
\*\d+ wasm freeing "hostcalls" instance in "main" vm.*\Z/,
qr/\A\*\d+ proxy_wasm "hostcalls" filter \(1\/2\) new instance.*
\*\d+ proxy_wasm "hostcalls" filter \(2\/2\) reusing instance.*
\*\d+ proxy_wasm "hostcalls" filter \(1\/2\) resuming in "rewrite" phase.*
\*\d+ proxy_wasm "hostcalls" filter \(2\/2\) resuming in "rewrite" phase.*
\*\d+ proxy_wasm "hostcalls" filter \(1\/2\) resuming in "rewrite" phase.*
\*\d+ proxy_wasm "hostcalls" filter \(1\/2\) resuming in "header_filter" phase.*
\*\d+ proxy_wasm "hostcalls" filter \(2\/2\) resuming in "header_filter" phase.*
\*\d+ proxy_wasm "hostcalls" filter \(1\/2\) resuming in "log" phase.*
\*\d+ proxy_wasm "hostcalls" filter \(2\/2\) resuming in "log" phase.*
\*\d+ now: .*? while logging request.*
\*\d+ proxy_wasm "hostcalls" filter \(1\/2\) resuming in "done" phase.*
\*\d+ proxy_wasm "hostcalls" filter \(1\/2\) finalizing context
\*\d+ proxy_wasm "hostcalls" filter \(2\/2\) resuming in "done" phase.*
\*\d+ proxy_wasm "hostcalls" filter \(2\/2\) finalizing context
\*\d+ proxy_wasm freeing stream context #\d+ \(main: 1\)
\*\d+ wasm freeing "hostcalls" instance in "main" vm.*\Z/]
--- no_error_log
[emerg]
[alert]
