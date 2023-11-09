# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

skip_valgrind();
skip_no_go_sdk();

plan tests => repeat_each() * (blocks() * 5);

run_tests();

__DATA__

=== TEST 1: proxy_wasm Go SDK - shared_data example
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: go_shared_data
--- shm_kv: * 64k
--- config
    location /subrequest {
        internal;
        proxy_wasm go_shared_data;
        echo ok;
    }

    location /t {
        echo_subrequest GET '/subrequest';
        echo_subrequest GET '/subrequest';
        echo_subrequest GET '/subrequest';
    }
--- response_body
ok
ok
ok
--- error_log eval
[
    qr/\[info\] .*? shared value: 10000001/,
    qr/\[info\] .*? shared value: 10000002/,
    qr/\[info\] .*? shared value: 10000003/,
]
