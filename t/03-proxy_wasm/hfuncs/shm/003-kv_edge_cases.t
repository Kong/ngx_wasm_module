# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

skip_valgrind();

plan tests => repeat_each() * (blocks() * 25);

run_tests();

__DATA__

=== TEST 1: proxy_wasm key/value shm - most recent data in the LRU queue
--- skip_no_debug: 25
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- shm_kv: test 12288
--- config
    location /t {
        # set 10 items, only 4 fit
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test1 header_ok=set-1 len=512';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test2 header_ok=set-2 len=512';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test3 header_ok=set-3 len=512';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test4 header_ok=set-4 len=512';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test5 header_ok=set-5 len=512';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test6 header_ok=set-6 len=512';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test7 header_ok=set-7 len=512';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test8 header_ok=set-8 len=512';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test9 header_ok=set-9 len=512';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test10 header_ok=set-10 len=512';

        # check all items
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-1 key=test/test1';
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-2 key=test/test2';
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-3 key=test/test3';
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-4 key=test/test4';
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-5 key=test/test5';
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-6 key=test/test6';
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-7 key=test/test7';
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-8 key=test/test8';
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-9 key=test/test9';
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-10 key=test/test10';

        echo ok;
    }
--- error_code: 200
--- response_headers_like
set-1: 1
set-2: 1
set-3: 1
set-4: 1
set-5: 1
set-6: 1
set-7: 1
set-8: 1
set-9: 1
set-10: 1
data-1:
data-2:
data-3:
data-4:
data-5:
data-6:
data-7: x{512}
data-8: x{512}
data-9: x{512}
data-10: x{512}
--- response_body
ok
--- grep_error_log eval: qr/.*wasm "test" shm store.*/
--- grep_error_log_out eval
qr/\[debug\] .*? wasm "test" shm store: initialized
.*? \[debug\] .*? wasm "test" shm store: expired LRU entry
.*? \[debug\] .*? wasm "test" shm store: expired LRU entry
.*? \[debug\] .*? wasm "test" shm store: expired LRU entry
.*? \[debug\] .*? wasm "test" shm store: expired LRU entry
.*? \[debug\] .*? wasm "test" shm store: expired LRU entry
.*? \[debug\] .*? wasm "test" shm store: expired LRU entry/
--- no_error_log
[emerg]
[crit]



=== TEST 2: proxy_wasm key/value shm - a large allocation may evict multiple entries
--- skip_no_debug: 25
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- shm_kv: test 12288
--- config
    location /t {
        # Set 9 entries. A 4096-byte slab will be allocated into slot 7
        # (1024-byte-sized entries), meaning that 4 entries will fit
        # in the slab. Due to the given shm_kv size (two pages of overhead,
        # one for data), that's all we can allocate at a time.
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test1 header_ok=set-1 len=512';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test2 header_ok=set-2 len=512';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test3 header_ok=set-3 len=512';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test4 header_ok=set-4 len=512';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test5 header_ok=set-5 len=512';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test6 header_ok=set-6 len=512';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test7 header_ok=set-7 len=512';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test8 header_ok=set-8 len=512';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test9 header_ok=set-9 len=512';

        # At this point in time, we have entries 6, 7, 8 and 9 in the slab.
        # The last allocation is larger, but it goes over the entry size
        # of our only page. This means all four entries need to be released,
        # to free the slab for reallocation into slot 8 (2048-byte-sized entries).
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test10 header_ok=set-10 len=1024';

        # check all items
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-1 key=test/test1';
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-2 key=test/test2';
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-3 key=test/test3';
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-4 key=test/test4';
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-5 key=test/test5';
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-6 key=test/test6';
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-7 key=test/test7';
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-8 key=test/test8';
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-9 key=test/test9';
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-10 key=test/test10';

        echo ok;
    }
--- error_code: 200
--- response_headers_like
set-1: 1
set-2: 1
set-3: 1
set-4: 1
set-5: 1
set-6: 1
set-7: 1
set-8: 1
set-9: 1
set-10: 1
data-1:
data-2:
data-3:
data-4:
data-5:
data-6:
data-7:
data-8:
data-9:
data-10: x{1024}
--- response_body
ok
--- grep_error_log eval: qr/.*wasm "test" shm store.*/
--- grep_error_log_out eval
qr/\[debug\] .*? wasm "test" shm store: initialized
.*? \[debug\] .*? wasm "test" shm store: expired LRU entry
.*? \[debug\] .*? wasm "test" shm store: expired LRU entry
.*? \[debug\] .*? wasm "test" shm store: expired LRU entry
.*? \[debug\] .*? wasm "test" shm store: expired LRU entry
.*? \[debug\] .*? wasm "test" shm store: expired LRU entry
.*? \[debug\] .*? wasm "test" shm store: expired LRU entry/
--- no_error_log
[emerg]
[crit]



=== TEST 3: proxy_wasm key/value shm - getting keeps data alive in the LRU queue
--- skip_no_debug: 25
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- shm_kv: test 12288
--- config
    location /t {
        # set 10 items, only 4 fit
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test1 header_ok=set-1 len=512';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test2 header_ok=set-2 len=512';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test3 header_ok=set-3 len=512';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test4 header_ok=set-4 len=512';
           #keep test1 alive in the LRU queue
           proxy_wasm hostcalls 'test=/t/shm/get_shared_data key=test/test1';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test5 header_ok=set-5 len=512';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test6 header_ok=set-6 len=512';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test7 header_ok=set-7 len=512';
           #keep test1 alive in the LRU queue
           proxy_wasm hostcalls 'test=/t/shm/get_shared_data key=test/test1';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test8 header_ok=set-8 len=512';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test9 header_ok=set-9 len=512';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test10 header_ok=set-10 len=512';

        # check all items
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-1 key=test/test1';
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-2 key=test/test2';
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-3 key=test/test3';
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-4 key=test/test4';
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-5 key=test/test5';
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-6 key=test/test6';
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-7 key=test/test7';
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-8 key=test/test8';
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-9 key=test/test9';
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-10 key=test/test10';

        echo ok;
    }
--- error_code: 200
--- response_headers_like
set-1: 1
set-2: 1
set-3: 1
set-4: 1
set-5: 1
set-6: 1
set-7: 1
set-8: 1
set-9: 1
set-10: 1
data-1: x{512}
data-2:
data-3:
data-4:
data-5:
data-6:
data-7:
data-8: x{512}
data-9: x{512}
data-10: x{512}
--- response_body
ok
--- grep_error_log eval: qr/.*wasm "test" shm store.*/
--- grep_error_log_out eval
qr/\[debug\] .*? wasm "test" shm store: initialized
.*? \[debug\] .*? wasm "test" shm store: expired LRU entry
.*? \[debug\] .*? wasm "test" shm store: expired LRU entry
.*? \[debug\] .*? wasm "test" shm store: expired LRU entry
.*? \[debug\] .*? wasm "test" shm store: expired LRU entry
.*? \[debug\] .*? wasm "test" shm store: expired LRU entry
.*? \[debug\] .*? wasm "test" shm store: expired LRU entry/
--- no_error_log
[emerg]
[crit]



=== TEST 4: proxy_wasm key/value shm - a set that is too big can still fail
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- shm_kv: test 12288
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len \
                              key=test/test1 \
                              header_ok=set-1 \
                              len=20000';

        # get test/test
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data \
                              key=test/test';
        echo failed;
    }
--- error_code: 500
--- response_body_like: 500 Internal Server Error
--- grep_error_log eval: qr/(\[crit\]|.*?failed setting value to shm).*/
--- grep_error_log_out eval
qr/\[crit\] .*? \[wasm\] "test" shm store: no memory; cannot allocate pair with key size 10 and value size 20000
(.*?\[error\]|Uncaught RuntimeError|\s+).*?host trap \(internal error\): failed setting value to shm \(could not write to slab\).*/
--- no_error_log
[emerg]
[alert]
[stub1]
[stub2]
[stub3]
[stub4]
[stub5]
[stub6]
[stub7]
[stub8]
[stub9]
[stub10]
[stub11]
[stub12]
[stub13]
[stub14]
[stub15]
[stub16]
[stub17]
[stub18]
[stub19]
[stub20]
