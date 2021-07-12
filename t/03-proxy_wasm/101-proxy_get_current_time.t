# vim:set ft= ts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

skip_valgrind();

plan tests => repeat_each() * (blocks() * 4);

run_tests();

__DATA__

=== TEST 1: proxy_wasm - get_current_time() gets current time
should produce a log with the current time (ms as ~ns)
--- wasm_modules: hostcalls
--- load_nginx_modules: ngx_http_echo_module
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/log/current_time';
        echo ok;
    }
--- response_body
ok
--- error_log eval
[
    qr/\[info\] .*? \[wasm\] now: 2.*? UTC/,
]
--- no_error_log
[error]
