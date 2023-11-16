# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

plan_tests(4);
run_tests();

__DATA__

=== TEST 1: proxy_wasm - NYI host function
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/nyi_host_func';
        echo fail;
    }
--- error_code: 500
--- response_body_like: 500 Internal Server Error
--- error_log eval
qr/host trap \(function not yet implemented\): proxy_close_stream/,
--- no_error_log
[crit]
