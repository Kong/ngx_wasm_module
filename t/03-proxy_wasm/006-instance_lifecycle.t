# vim:set ft= ts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

skip_no_debug();

plan tests => repeat_each() * (blocks() * 4);

run_tests();

__DATA__

=== TEST 1: proxy_wasm - default isolation mode (unique)
should reuse the root instance of each filter
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/log/current_time';
        proxy_wasm hostcalls 'test=/t/log/current_time';
        return 200;
    }
--- ignore_response_body
--- grep_error_log eval: qr/\*\d+.*?(proxy_wasm\s|(creating|freeing) ".*?" instance).*/
--- grep_error_log_out eval
qr/.*?proxy_wasm "hostcalls" filter resuming in "rewrite" phase.*
.*?wasm creating "hostcalls" instance in "main" vm.*
.*?proxy_wasm "hostcalls" filter resuming in "rewrite" phase.*
.*?proxy_wasm context #\d+ "hostcalls" filter \(2\/2\) reusing instance.*
.*?proxy_wasm "hostcalls" filter resuming in "header_filter" phase.*
.*?proxy_wasm context #\d+ "hostcalls" filter \(1\/2\) reusing instance.*
.*?proxy_wasm "hostcalls" filter resuming in "header_filter" phase.*
.*?proxy_wasm context #\d+ "hostcalls" filter \(2\/2\) reusing instance.*
.*?proxy_wasm "hostcalls" filter resuming in "body_filter" phase.*
.*?proxy_wasm context #\d+ "hostcalls" filter \(1\/2\) reusing instance.*
.*?proxy_wasm "hostcalls" filter resuming in "body_filter" phase.*
.*?proxy_wasm context #\d+ "hostcalls" filter \(2\/2\) reusing instance.*
.*?proxy_wasm "hostcalls" filter resuming in "log" phase.*
.*?proxy_wasm context #\d+ "hostcalls" filter \(1\/2\) reusing instance.*
.*?proxy_wasm "hostcalls" filter resuming in "log" phase.*
.*?proxy_wasm context #\d+ "hostcalls" filter \(2\/2\) reusing instance.*
.*?proxy_wasm "hostcalls" filter resuming in "done" phase.*
.*?proxy_wasm context #\d+ "hostcalls" filter \(1\/2\) reusing instance.*
.*?proxy_wasm context #\d+ "hostcalls" filter \(1\/2\) finalizing context.*
.*?proxy_wasm "hostcalls" filter resuming in "done" phase.*
.*?proxy_wasm context #\d+ "hostcalls" filter \(2\/2\) reusing instance.*
.*?proxy_wasm context #\d+ "hostcalls" filter \(2\/2\) finalizing context.*
.*?proxy_wasm context #\d+ freeing stream context.*/
--- no_error_log
[error]
[crit]



=== TEST 2: proxy_wasm - filter isolation mode
should use an instance for each filter
--- wasm_modules: hostcalls
--- config
    proxy_wasm_isolation filter;

    location /t {
        proxy_wasm hostcalls 'test=/t/log/current_time';
        proxy_wasm hostcalls 'test=/t/log/current_time';
        return 200;
    }
--- ignore_response_body
--- grep_error_log eval: qr/\*\d+.*?(proxy_wasm\s|(creating|freeing) ".*?" instance).*/
--- grep_error_log_out eval
qr/.*?proxy_wasm "hostcalls" filter resuming in "rewrite" phase.*
.*?wasm creating "hostcalls" instance in "main" vm.*
.*?proxy_wasm "hostcalls" filter resuming in "rewrite" phase.*
.*?wasm creating "hostcalls" instance in "main" vm.*
.*?proxy_wasm "hostcalls" filter resuming in "header_filter" phase.*
.*?proxy_wasm context #\d+ "hostcalls" filter \(1\/2\) reusing instance.*
.*?proxy_wasm "hostcalls" filter resuming in "header_filter" phase.*
.*?proxy_wasm context #\d+ "hostcalls" filter \(2\/2\) reusing instance.*
.*?proxy_wasm "hostcalls" filter resuming in "body_filter" phase.*
.*?proxy_wasm context #\d+ "hostcalls" filter \(1\/2\) reusing instance.*
.*?proxy_wasm "hostcalls" filter resuming in "body_filter" phase.*
.*?proxy_wasm context #\d+ "hostcalls" filter \(2\/2\) reusing instance.*
.*?proxy_wasm "hostcalls" filter resuming in "log" phase.*
.*?proxy_wasm context #\d+ "hostcalls" filter \(1\/2\) reusing instance.*
.*?proxy_wasm "hostcalls" filter resuming in "log" phase.*
.*?proxy_wasm context #\d+ "hostcalls" filter \(2\/2\) reusing instance.*
.*?proxy_wasm "hostcalls" filter resuming in "done" phase.*
.*?proxy_wasm context #\d+ "hostcalls" filter \(1\/2\) reusing instance.*
.*?proxy_wasm context #\d+ "hostcalls" filter \(1\/2\) finalizing context.*
.*?proxy_wasm "hostcalls" filter resuming in "done" phase.*
.*?proxy_wasm context #\d+ "hostcalls" filter \(2\/2\) reusing instance.*
.*?proxy_wasm context #\d+ "hostcalls" filter \(2\/2\) finalizing context.*
.*?proxy_wasm context #\d+ freeing stream context.*/
--- no_error_log
[error]
[crit]



=== TEST 3: proxy_wasm - stream isolation mode
--- wasm_modules: hostcalls
--- config
    proxy_wasm_isolation stream;

    location /t {
        proxy_wasm hostcalls 'test=/t/log/current_time';
        proxy_wasm hostcalls 'test=/t/log/current_time';
        return 200;
    }
--- ignore_response_body
--- grep_error_log eval: qr/\*\d+.*?(proxy_wasm\s|(creating|freeing) ".*?" instance).*/
--- grep_error_log_out eval
qr/.*?proxy_wasm "hostcalls" filter resuming in "rewrite" phase.*
.*?wasm creating "hostcalls" instance in "main" vm.*
.*?proxy_wasm "hostcalls" filter resuming in "rewrite" phase.*
.*?proxy_wasm context #\d+ "hostcalls" filter \(2\/2\) reusing instance.*
.*?proxy_wasm "hostcalls" filter resuming in "header_filter" phase.*
.*?proxy_wasm context #\d+ "hostcalls" filter \(1\/2\) reusing instance.*
.*?proxy_wasm "hostcalls" filter resuming in "header_filter" phase.*
.*?proxy_wasm context #\d+ "hostcalls" filter \(2\/2\) reusing instance.*
.*?proxy_wasm "hostcalls" filter resuming in "body_filter" phase.*
.*?proxy_wasm context #\d+ "hostcalls" filter \(1\/2\) reusing instance.*
.*?proxy_wasm "hostcalls" filter resuming in "body_filter" phase.*
.*?proxy_wasm context #\d+ "hostcalls" filter \(2\/2\) reusing instance.*
.*?proxy_wasm "hostcalls" filter resuming in "log" phase.*
.*?proxy_wasm context #\d+ "hostcalls" filter \(1\/2\) reusing instance.*
.*?proxy_wasm "hostcalls" filter resuming in "log" phase.*
.*?proxy_wasm context #\d+ "hostcalls" filter \(2\/2\) reusing instance.*
.*?proxy_wasm "hostcalls" filter resuming in "done" phase.*
.*?proxy_wasm context #\d+ "hostcalls" filter \(1\/2\) reusing instance.*
.*?proxy_wasm context #\d+ "hostcalls" filter \(1\/2\) finalizing context.*
.*?proxy_wasm "hostcalls" filter resuming in "done" phase.*
.*?proxy_wasm context #\d+ "hostcalls" filter \(2\/2\) reusing instance.*
.*?proxy_wasm context #\d+ "hostcalls" filter \(2\/2\) finalizing context.*
.*?proxy_wasm context #\d+ freeing stream context.*
.*?wasm freeing "hostcalls" instance in "main" vm.*/
--- no_error_log
[error]
[crit]
