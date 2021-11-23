# vim:set ft= ts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

skip_no_debug();
check_accum_error_log();

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
--- grep_error_log eval: qr/\*\d+.*?(resuming|creating|reusing|finalizing|freeing).*/
--- grep_error_log_out eval
qr/.*?proxy_wasm "hostcalls" filter \(1\/2\) resuming in "rewrite" phase.*
.*?wasm creating "hostcalls" instance in "main" vm.*
.*?proxy_wasm "hostcalls" filter \(2\/2\) resuming in "rewrite" phase.*
.*?proxy_wasm "hostcalls" filter \(2\/2\) reusing instance.*
.*?proxy_wasm "hostcalls" filter \(1\/2\) resuming in "header_filter" phase.*
.*?proxy_wasm "hostcalls" filter \(2\/2\) resuming in "header_filter" phase.*
.*?proxy_wasm "hostcalls" filter \(1\/2\) resuming in "body_filter" phase.*
.*?proxy_wasm "hostcalls" filter \(2\/2\) resuming in "body_filter" phase.*
.*?proxy_wasm "hostcalls" filter \(1\/2\) resuming in "log" phase.*
.*?proxy_wasm "hostcalls" filter \(2\/2\) resuming in "log" phase.*
.*?proxy_wasm "hostcalls" filter \(1\/2\) resuming in "done" phase.*
.*?proxy_wasm "hostcalls" filter \(1\/2\) finalizing context.*
.*?proxy_wasm "hostcalls" filter \(2\/2\) resuming in "done" phase.*
.*?proxy_wasm "hostcalls" filter \(2\/2\) finalizing context.*
.*?proxy_wasm freeing stream context #\d+.*
(.*?proxy_wasm "hostcalls" filter \(1\/2\) resuming in "rewrite" phase.*
.*?proxy_wasm "hostcalls" filter \(1\/2\) reusing instance.*
.*?proxy_wasm "hostcalls" filter \(2\/2\) resuming in "rewrite" phase.*
.*?proxy_wasm "hostcalls" filter \(2\/2\) reusing instance.*
.*?proxy_wasm "hostcalls" filter \(1\/2\) resuming in "header_filter" phase.*
.*?proxy_wasm "hostcalls" filter \(2\/2\) resuming in "header_filter" phase.*
.*?proxy_wasm "hostcalls" filter \(1\/2\) resuming in "body_filter" phase.*
.*?proxy_wasm "hostcalls" filter \(2\/2\) resuming in "body_filter" phase.*
.*?proxy_wasm "hostcalls" filter \(1\/2\) resuming in "log" phase.*
.*?proxy_wasm "hostcalls" filter \(2\/2\) resuming in "log" phase.*
.*?proxy_wasm "hostcalls" filter \(1\/2\) resuming in "done" phase.*
.*?proxy_wasm "hostcalls" filter \(1\/2\) finalizing context.*
.*?proxy_wasm "hostcalls" filter \(2\/2\) resuming in "done" phase.*
.*?proxy_wasm "hostcalls" filter \(2\/2\) finalizing context.*
.*?proxy_wasm freeing stream context #\d+.*)?/
--- no_error_log
[error]
[crit]



=== TEST 2: proxy_wasm - trap with none isolation mode
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    proxy_wasm_isolation none;

    location /t {
        proxy_wasm hostcalls;
        proxy_wasm hostcalls 'test=/t/log/current_time';
        echo_status 200;
    }
--- request eval
["GET /t/trap", "GET /t/send_local_response/status/204"]
--- error_code eval
[500, 204]
--- ignore_response_body
--- grep_error_log eval: qr/\*\d+.*?(resuming|creating|reusing|destroying|finalizing|freeing).*/
--- grep_error_log_out eval
qr/.*?proxy_wasm "hostcalls" filter \(1\/2\) resuming in "rewrite" phase.*
.*?wasm creating "hostcalls" instance in "main" vm.*
.*?proxy_wasm "hostcalls" filter \(1\/2\) resuming in "header_filter" phase.*
.*?proxy_wasm "hostcalls" filter \(1\/2\) failed resuming \(instance trapped\).*
.*?proxy_wasm "hostcalls" filter \(1\/2\) resuming in "body_filter" phase.*
.*?proxy_wasm "hostcalls" filter \(1\/2\) resuming in "log" phase.*
.*?proxy_wasm "hostcalls" filter \(1\/2\) resuming in "done" phase.*
.*?wasm freeing "hostcalls" instance in "main" vm.*
.*?proxy_wasm freeing stream context #\d+.*
(.*?proxy_wasm "hostcalls" filter \(1\/2\) resuming in "rewrite" phase.*
.*?proxy_wasm "hostcalls" filter \(1\/2\) resuming in "access" phase.*
.*?proxy_wasm "hostcalls" filter \(1\/2\) resuming in "content" phase.*
.*?proxy_wasm "hostcalls" filter \(1\/2\) resuming in "header_filter" phase.*
.*?proxy_wasm "hostcalls" filter \(2\/2\) resuming in "header_filter" phase.*
.*?proxy_wasm "hostcalls" filter \(2\/2\) reusing instance.*
.*?proxy_wasm "hostcalls" filter \(1\/2\) resuming in "log" phase.*
.*?proxy_wasm "hostcalls" filter \(2\/2\) resuming in "log" phase.*
.*?proxy_wasm "hostcalls" filter \(1\/2\) resuming in "done" phase.*
.*?proxy_wasm "hostcalls" filter \(1\/2\) finalizing context.*
.*?proxy_wasm "hostcalls" filter \(2\/2\) resuming in "done" phase.*
.*?proxy_wasm "hostcalls" filter \(2\/2\) finalizing context.*
.*?proxy_wasm freeing stream context #\d+.*)?/
--- no_error_log
[emerg]
stub



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
--- grep_error_log eval: qr/\*\d+.*?(resuming|creating|reusing|finalizing|freeing).*/
--- grep_error_log_out eval
qr/.*?proxy_wasm "hostcalls" filter \(1\/2\) resuming in "rewrite" phase.*
.*?wasm creating "hostcalls" instance in "main" vm.*
.*?proxy_wasm "hostcalls" filter \(2\/2\) resuming in "rewrite" phase.*
.*?proxy_wasm "hostcalls" filter \(2\/2\) reusing instance.*
.*?proxy_wasm "hostcalls" filter \(1\/2\) resuming in "header_filter" phase.*
.*?proxy_wasm "hostcalls" filter \(2\/2\) resuming in "header_filter" phase.*
.*?proxy_wasm "hostcalls" filter \(1\/2\) resuming in "body_filter" phase.*
.*?proxy_wasm "hostcalls" filter \(2\/2\) resuming in "body_filter" phase.*
.*?proxy_wasm "hostcalls" filter \(1\/2\) resuming in "log" phase.*
.*?proxy_wasm "hostcalls" filter \(2\/2\) resuming in "log" phase.*
.*?proxy_wasm "hostcalls" filter \(1\/2\) resuming in "done" phase.*
.*?proxy_wasm "hostcalls" filter \(1\/2\) finalizing context.*
.*?proxy_wasm "hostcalls" filter \(2\/2\) resuming in "done" phase.*
.*?proxy_wasm "hostcalls" filter \(2\/2\) finalizing context.*
.*?proxy_wasm freeing stream context #\d+.*
.*?wasm freeing "hostcalls" instance in "main" vm.*
(.*?proxy_wasm "hostcalls" filter \(1\/2\) resuming in "rewrite" phase.*
.*?proxy_wasm "hostcalls" filter \(1\/2\) reusing instance.*
.*?proxy_wasm "hostcalls" filter \(2\/2\) resuming in "rewrite" phase.*
.*?proxy_wasm "hostcalls" filter \(2\/2\) reusing instance.*
.*?proxy_wasm "hostcalls" filter \(1\/2\) resuming in "header_filter" phase.*
.*?proxy_wasm "hostcalls" filter \(2\/2\) resuming in "header_filter" phase.*
.*?proxy_wasm "hostcalls" filter \(1\/2\) resuming in "body_filter" phase.*
.*?proxy_wasm "hostcalls" filter \(2\/2\) resuming in "body_filter" phase.*
.*?proxy_wasm "hostcalls" filter \(1\/2\) resuming in "log" phase.*
.*?proxy_wasm "hostcalls" filter \(2\/2\) resuming in "log" phase.*
.*?proxy_wasm "hostcalls" filter \(1\/2\) resuming in "done" phase.*
.*?proxy_wasm "hostcalls" filter \(1\/2\) finalizing context.*
.*?proxy_wasm "hostcalls" filter \(2\/2\) resuming in "done" phase.*
.*?proxy_wasm "hostcalls" filter \(2\/2\) finalizing context.*
.*?proxy_wasm freeing stream context #\d+.*
.*?wasm freeing "hostcalls" instance in "main" vm.*)?/
--- no_error_log
[error]
[crit]



=== TEST 4: proxy_wasm - trap with stream isolation mode
--- ONLY
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    proxy_wasm_isolation stream;

    location /t {
        proxy_wasm hostcalls;
        proxy_wasm hostcalls 'test=/t/log/current_time';
        echo_status 200;
    }
--- request eval
["GET /t/trap", "GET /t/send_local_response/status/204"]
--- error_code eval
[500, 204]
--- ignore_response_body
--- grep_error_log eval: qr/\*\d+.*?(resuming|creating|reusing|destroying|finalizing|freeing).*/
--- grep_error_log_out eval
qr/.*?proxy_wasm "hostcalls" filter \(1\/2\) resuming in "rewrite" phase.*
.*?wasm creating "hostcalls" instance in "main" vm.*
.*?proxy_wasm "hostcalls" filter \(1\/2\) resuming in "header_filter" phase.*
.*?proxy_wasm "hostcalls" filter \(1\/2\) failed resuming \(instance trapped\).*
.*?proxy_wasm "hostcalls" filter \(1\/2\) resuming in "body_filter" phase.*
.*?proxy_wasm "hostcalls" filter \(1\/2\) resuming in "log" phase.*
.*?proxy_wasm "hostcalls" filter \(1\/2\) resuming in "done" phase.*
.*?wasm freeing "hostcalls" instance in "main" vm.*
.*?proxy_wasm freeing stream context #\d+.*
(.*?proxy_wasm "hostcalls" filter \(1\/2\) resuming in "rewrite" phase.*
.*?wasm creating "hostcalls" instance in "main" vm.*
.*?proxy_wasm "hostcalls" filter \(1\/2\) resuming in "access" phase.*
.*?proxy_wasm "hostcalls" filter \(1\/2\) resuming in "content" phase.*
.*?proxy_wasm "hostcalls" filter \(1\/2\) resuming in "header_filter" phase.*
.*?proxy_wasm "hostcalls" filter \(2\/2\) resuming in "header_filter" phase.*
.*?proxy_wasm "hostcalls" filter \(2\/2\) reusing instance.*
.*?proxy_wasm "hostcalls" filter \(1\/2\) resuming in "log" phase.*
.*?proxy_wasm "hostcalls" filter \(2\/2\) resuming in "log" phase.*
.*?proxy_wasm "hostcalls" filter \(1\/2\) resuming in "done" phase.*
.*?proxy_wasm "hostcalls" filter \(1\/2\) finalizing context.*
.*?proxy_wasm "hostcalls" filter \(2\/2\) resuming in "done" phase.*
.*?proxy_wasm "hostcalls" filter \(2\/2\) finalizing context.*
.*?proxy_wasm freeing stream context #\d+.*
.*?wasm freeing "hostcalls" instance in "main" vm.*)?/
--- no_error_log
[emerg]
[alert]
