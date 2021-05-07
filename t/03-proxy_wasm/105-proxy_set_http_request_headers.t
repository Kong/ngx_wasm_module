# vim:set ft= ts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

skip_valgrind();

plan tests => repeat_each() * (blocks() * 6);

run_tests();

__DATA__

=== TEST 1: proxy_wasm - set_http_request_headers() sets request headers
should produce a response with headers
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test_case=/t/set_http_request_headers';
        proxy_wasm hostcalls 'test_case=/t/echo/headers';
    }
--- response_body eval
qq{Host: localhost
Connection: close
Hello: world
}
--- no_error_log
[error]
[crit]
[alert]
[emerg]
