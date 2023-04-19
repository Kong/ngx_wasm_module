# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

skip_valgrind();

plan tests => repeat_each() * (blocks() * 5);

run_tests();

__DATA__

=== TEST 1: proxy_wasm key/value shm - set value when slab is full
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- shm_kv: test 12288
--- config
    location /t {
        # set test/test=hello
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data \
                              key=test/test \
                              value=hello';
        # get test/test
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data \
                              key=test/test';
        echo failed;
    }
--- error_code: 500
--- response_body_like: 500 Internal Server Error
--- grep_error_log eval: qr/(\[crit\]|.*?failed setting value to shm).*/
--- grep_error_log_out eval
qr/\[crit\] .*? ngx_slab_alloc\(\) failed: no memory
(.*?\[error\]|Uncaught RuntimeError|\s+).*?host trap \(internal error\): failed setting value to shm \(could not write to slab\).*/
--- no_error_log
[emerg]
[alert]
