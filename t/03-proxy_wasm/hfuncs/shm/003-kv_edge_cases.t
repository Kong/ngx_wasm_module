# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

plan_tests(5);
run_tests();

__DATA__

=== TEST 1: proxy_wasm key/value shm - 'no memory' errors if no eviction policy is active
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- shm_kv: test 12288 eviction=none
--- config
    location /t {
        # only 4 fit
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test1 header_ok=set-1 len=512';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test2 header_ok=set-2 len=512';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test3 header_ok=set-3 len=512';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test4 header_ok=set-4 len=512';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test5 header_ok=set-5 len=512';

        echo ok;
    }
--- error_code: 500
--- response_body_like: 500 Internal Server Error
--- grep_error_log eval: qr/(\[crit\]|.*?failed setting value to shm).*/
--- grep_error_log_out eval
qr/\[crit\] .*? \[wasm\] "test" shm store: no memory; cannot allocate pair with key size 10 and value size 512
(.*?\[error\]|Uncaught RuntimeError|\s+).*?host trap \(internal error\): failed setting value to shm \(could not write to slab\).*/
--- no_error_log
[emerg]
[alert]
