# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

skip_valgrind();

plan tests => repeat_each() * (blocks() * 25);

run_tests();

__DATA__

=== TEST 1: proxy_wasm key/value shm eviction - evicts most recent data in the queue
--- skip_no_debug: 25
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- shm_kv: test 49152
--- config
    location /t {
        # set 10 items, only 4 fit
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test1 header_ok=set-1 len=2048';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test2 header_ok=set-2 len=2048';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test3 header_ok=set-3 len=2048';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test4 header_ok=set-4 len=2048';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test5 header_ok=set-5 len=2048';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test6 header_ok=set-6 len=2048';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test7 header_ok=set-7 len=2048';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test8 header_ok=set-8 len=2048';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test9 header_ok=set-9 len=2048';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test10 header_ok=set-10 len=2048';

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
data-7: x{2048}
data-8: x{2048}
data-9: x{2048}
data-10: x{2048}
--- response_body
ok
--- grep_error_log eval: qr/.*wasm "test" shm store.*/
--- grep_error_log_out eval
qr/\[debug\] [^"]* wasm "test" shm store: initialized
[^"]* \[debug\] [^"]* wasm "test" shm store: expired LRU entry
[^"]* \[debug\] [^"]* wasm "test" shm store: expired LRU entry
[^"]* \[debug\] [^"]* wasm "test" shm store: expired LRU entry
[^"]* \[debug\] [^"]* wasm "test" shm store: expired LRU entry
[^"]* \[debug\] [^"]* wasm "test" shm store: expired LRU entry
[^"]* \[debug\] [^"]* wasm "test" shm store: expired LRU entry/
--- no_error_log
[emerg]
[crit]



=== TEST 2: proxy_wasm key/value shm eviction - a large allocation may evict multiple entries
--- skip_eval: 25: ($::osname =~ m/darwin/ && $::archname =~ /arm64/)
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
qr/\[debug\] [^"]* wasm "test" shm store: initialized
[^"]* \[debug\] [^"]* wasm "test" shm store: expired LRU entry
[^"]* \[debug\] [^"]* wasm "test" shm store: expired LRU entry
[^"]* \[debug\] [^"]* wasm "test" shm store: expired LRU entry
[^"]* \[debug\] [^"]* wasm "test" shm store: expired LRU entry
[^"]* \[debug\] [^"]* wasm "test" shm store: expired LRU entry
[^"]* \[debug\] [^"]* wasm "test" shm store: expired LRU entry/
--- no_error_log
[emerg]
[crit]




=== TEST 2: proxy_wasm key/value shm eviction - a large allocation may evict multiple entries, macOS (arm64)
--- skip_eval: 25: ($::osname !~ m/darwin/ && $::archname !~ /arm64/)
--- skip_no_debug: 25
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- shm_kv: test 49152
--- config
    location /t {
        # Set 9 entries. A 16-kilobyte slab will be allocated into slot 9
        # (4096-byte-sized entries), meaning that 4 entries will fit
        # in the slab. Due to the given shm_kv size (two pages of overhead,
        # one for data), that's all we can allocate at a time.
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test1 header_ok=set-1 len=2048';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test2 header_ok=set-2 len=2048';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test3 header_ok=set-3 len=2048';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test4 header_ok=set-4 len=2048';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test5 header_ok=set-5 len=2048';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test6 header_ok=set-6 len=2048';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test7 header_ok=set-7 len=2048';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test8 header_ok=set-8 len=2048';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test9 header_ok=set-9 len=2048';

        # At this point in time, we have entries 6, 7, 8 and 9 in the slab.
        # The last allocation is larger, but it goes over the entry size
        # of our only page. This means all four entries need to be released,
        # to free the slab for reallocation into slot 8 (2048-byte-sized entries).
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test10 header_ok=set-10 len=4096';

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
data-10: x{4096}
--- response_body
ok
--- grep_error_log eval: qr/.*wasm "test" shm store.*/
--- grep_error_log_out eval
qr/\[debug\] [^"]* wasm "test" shm store: initialized
[^"]* \[debug\] [^"]* wasm "test" shm store: expired LRU entry
[^"]* \[debug\] [^"]* wasm "test" shm store: expired LRU entry
[^"]* \[debug\] [^"]* wasm "test" shm store: expired LRU entry
[^"]* \[debug\] [^"]* wasm "test" shm store: expired LRU entry
[^"]* \[debug\] [^"]* wasm "test" shm store: expired LRU entry
[^"]* \[debug\] [^"]* wasm "test" shm store: expired LRU entry/
--- no_error_log
[emerg]
[crit]



=== TEST 3: proxy_wasm key/value shm eviction - getting keeps data alive in the LRU queue
--- skip_eval: 25: ($::osname =~ m/darwin/ && $::archname =~ /arm64/)
--- skip_no_debug: 25
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- shm_kv: test 49152
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
qr/\[debug\] [^"]* wasm "test" shm store: initialized
[^"]* \[debug\] [^"]* wasm "test" shm store: expired LRU entry
[^"]* \[debug\] [^"]* wasm "test" shm store: expired LRU entry
[^"]* \[debug\] [^"]* wasm "test" shm store: expired LRU entry
[^"]* \[debug\] [^"]* wasm "test" shm store: expired LRU entry
[^"]* \[debug\] [^"]* wasm "test" shm store: expired LRU entry
[^"]* \[debug\] [^"]* wasm "test" shm store: expired LRU entry/
--- no_error_log
[emerg]
[crit]



=== TEST 3: proxy_wasm key/value shm eviction - getting keeps data alive in the LRU queue, macOS (arm64)
--- skip_eval: 25: ($::osname !~ m/darwin/ && $::archname !~ /arm64/)
--- skip_no_debug: 25
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- shm_kv: test 49152
--- config
    location /t {
        # set 10 items, only 4 fit
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test1 header_ok=set-1 len=2048';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test2 header_ok=set-2 len=2048';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test3 header_ok=set-3 len=2048';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test4 header_ok=set-4 len=2048';
        # keep test1 alive in the LRU queue
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data key=test/test1';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test5 header_ok=set-5 len=2048';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test6 header_ok=set-6 len=2048';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test7 header_ok=set-7 len=2048';
        # keep test1 alive in the LRU queue
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data key=test/test1';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test8 header_ok=set-8 len=2048';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test9 header_ok=set-9 len=2048';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test10 header_ok=set-10 len=2048';

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
data-1: x{2048}
data-2:
data-3:
data-4:
data-5:
data-6:
data-7:
data-8: x{2048}
data-9: x{2048}
data-10: x{2048}
--- response_body
ok
--- grep_error_log eval: qr/.*wasm "test" shm store.*/
--- grep_error_log_out eval
qr/\[debug\] [^"]* wasm "test" shm store: initialized
[^"]* \[debug\] [^"]* wasm "test" shm store: expired LRU entry
[^"]* \[debug\] [^"]* wasm "test" shm store: expired LRU entry
[^"]* \[debug\] [^"]* wasm "test" shm store: expired LRU entry
[^"]* \[debug\] [^"]* wasm "test" shm store: expired LRU entry
[^"]* \[debug\] [^"]* wasm "test" shm store: expired LRU entry
[^"]* \[debug\] [^"]* wasm "test" shm store: expired LRU entry/
--- no_error_log
[emerg]
[crit]



=== TEST 4: proxy_wasm key/value shm eviction - SLRU: a set that is too big can still fail
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- shm_kv: test 49152 eviction=slru
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len \
                              key=test/test1 \
                              header_ok=set-1 \
                              len=50000';

        # get test/test
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data \
                              key=test/test';
        echo failed;
    }
--- error_code: 500
--- response_body_like: 500 Internal Server Error
--- grep_error_log eval: qr/(\[crit\]|.*?failed setting value to shm).*/
--- grep_error_log_out eval
qr/\[crit\] .*? \[wasm\] "test" shm store: no memory; cannot allocate pair with key size 10 and value size 50000
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



=== TEST 5: proxy_wasm key/value shm eviction - LRU: a set that is too big can still fail
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- shm_kv: test 49152 eviction=lru
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



=== TEST 6: proxy_wasm key/value shm eviction - LRU: may over-deallocate
--- skip_eval: 25: ($::osname =~ m/darwin/ && $::archname =~ /arm64/)
--- skip_no_debug: 25
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- shm_kv: test 16384 eviction=lru
--- config
    location /t {
        # Set 4 small entries. A 4096-byte slab will be allocated into slot 7
        # (1024-byte-sized entries), meaning that 4 entries will fit
        # in the slab.
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test1 header_ok=set-1 len=512';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test2 header_ok=set-2 len=512';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test3 header_ok=set-3 len=512';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test4 header_ok=set-4 len=512';

        # Set 2 large entries. Another 4096-byte slab will be allocated,
        # this time into slot 8 (2048-byte-sized entries), meaning that 2
        # entries will fit.
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test5 header_ok=set-5 len=1024';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test6 header_ok=set-6 len=1024';

        # The shm is now full.
        # We will now try to allocate another large entry.
        # It will not fit, and the LRU algorithm will deallocate the
        # least-recently-used entries, which are all small. Even after
        # 2 entries are released (and we effectively have released the
        # desired amount of memory), there's still no slab available of
        # the correct size, so the allocation will only succeed after
        # all 4 entries of the "small" slab are released and that slab
        # gets reallocated into the "large" slot.
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test7 header_ok=set-7 len=1024';

        # check all items
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-1 key=test/test1';
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-2 key=test/test2';
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-3 key=test/test3';
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-4 key=test/test4';
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-5 key=test/test5';
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-6 key=test/test6';
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-7 key=test/test7';

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
data-1:
data-2:
data-3:
data-4:
data-5: x{1024}
data-6: x{1024}
data-7: x{1024}
--- response_body
ok
--- grep_error_log eval: qr/.*wasm "test" shm store.*/
--- grep_error_log_out eval
qr/^[^"]*\[debug\] [^"]* wasm "test" shm store: initialized
[^"]* \[debug\] [^"]* wasm "test" shm store: expired LRU entry
[^"]* \[debug\] [^"]* wasm "test" shm store: expired LRU entry
[^"]* \[debug\] [^"]* wasm "test" shm store: expired LRU entry
[^"]* \[debug\] [^"]* wasm "test" shm store: expired LRU entry[^"]*$/
--- no_error_log
[emerg]
[crit]
[stub1]
[stub2]
[stub3]
[stub4]
[stub5]
[stub6]



=== TEST 6: proxy_wasm key/value shm eviction - LRU: may over-deallocate, macOS (arm64)
--- skip_eval: 25: ($::osname !~ m/darwin/ && $::archname !~ /arm64/)
--- skip_no_debug: 25
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- shm_kv: test 65536 eviction=lru
--- config
    location /t {
        # Set 4 small entries. A 16-kilobyte slab will be allocated into slot 7
        # (4-kilobyte-sized entries), meaning that 4 entries will fit
        # in the slab.
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test1 header_ok=set-1 len=2048';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test2 header_ok=set-2 len=2048';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test3 header_ok=set-3 len=2048';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test4 header_ok=set-4 len=2048';

        # Set 2 large entries. Another 16-kilobyte slab will be allocated,
        # this time into slot 8 (8-kilobyte-sized entries), meaning that 2
        # entries will fit.
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test5 header_ok=set-5 len=4096';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test6 header_ok=set-6 len=4096';

        # The shm is now full.
        # We will now try to allocate another large entry.
        # It will not fit, and the LRU algorithm will deallocate the
        # least-recently-used entries, which are all small. Even after
        # 2 entries are released (and we effectively have released the
        # desired amount of memory), there's still no slab available of
        # the correct size, so the allocation will only succeed after
        # all 4 entries of the "small" slab are released and that slab
        # gets reallocated into the "large" slot.
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test7 header_ok=set-7 len=4096';

        # check all items
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-1 key=test/test1';
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-2 key=test/test2';
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-3 key=test/test3';
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-4 key=test/test4';
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-5 key=test/test5';
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-6 key=test/test6';
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-7 key=test/test7';

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
data-1:
data-2:
data-3:
data-4:
data-5: x{4096}
data-6: x{4096}
data-7: x{4096}
--- response_body
ok
--- grep_error_log eval: qr/.*wasm "test" shm store.*/
--- grep_error_log_out eval
qr/^[^"]*\[debug\] [^"]* wasm "test" shm store: initialized
[^"]* \[debug\] [^"]* wasm "test" shm store: expired LRU entry
[^"]* \[debug\] [^"]* wasm "test" shm store: expired LRU entry
[^"]* \[debug\] [^"]* wasm "test" shm store: expired LRU entry
[^"]* \[debug\] [^"]* wasm "test" shm store: expired LRU entry[^"]*$/
--- no_error_log
[emerg]
[crit]
[stub1]
[stub2]
[stub3]
[stub4]
[stub5]
[stub6]



=== TEST 7: proxy_wasm key/value shm eviction - SLRU: minimizes deallocation
--- skip_eval: 25: ($::osname =~ m/darwin/ && $::archname =~ /arm64/)
--- skip_no_debug: 25
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- shm_kv: test 16384 eviction=slru
--- config
    location /t {
        # Set 4 small entries. A 4096-byte slab will be allocated into slot 7
        # (1024-byte-sized entries), meaning that 4 entries will fit
        # in the slab.
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test1 header_ok=set-1 len=512';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test2 header_ok=set-2 len=512';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test3 header_ok=set-3 len=512';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test4 header_ok=set-4 len=512';

        # Set 2 large entries. Another 4096-byte slab will be allocated,
        # this time into slot 8 (2048-byte-sized entries), meaning that 2
        # entries will fit.
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test5 header_ok=set-5 len=1024';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test6 header_ok=set-6 len=1024';

        # The shm is now full.
        # We will now try to allocate another large entry.
        # It will not fit, and the LRU algorithm will deallocate the
        # least-recently-used entries, which are all small. Even after
        # 2 entries are released (and we effectively have released the
        # desired amount of memory), there's still no slab available of
        # the correct size, so the allocation will only succeed after
        # all 4 entries of the "small" slab are released and that slab
        # gets reallocated into the "large" slot.
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test7 header_ok=set-7 len=1024';

        # check all items
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-1 key=test/test1';
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-2 key=test/test2';
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-3 key=test/test3';
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-4 key=test/test4';
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-5 key=test/test5';
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-6 key=test/test6';
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-7 key=test/test7';

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
data-1: x{512}
data-2: x{512}
data-3: x{512}
data-4: x{512}
data-5:
data-6: x{1024}
data-7: x{1024}
--- response_body
ok
--- grep_error_log eval: qr/.*wasm "test" shm store.*/
--- grep_error_log_out eval
qr/^[^"]*\[debug\] [^"]* wasm "test" shm store: initialized
[^"]* \[debug\] [^"]* wasm "test" shm store: expired LRU entry[^"]*$/
--- no_error_log
[emerg]
[crit]
[stub1]
[stub2]
[stub3]
[stub4]
[stub5]
[stub6]



=== TEST 7: proxy_wasm key/value shm eviction - SLRU: minimizes deallocation, macOS (arm64)
--- skip_eval: 25: ($::osname !~ m/darwin/ && $::archname !~ /arm64/)
--- skip_no_debug: 25
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- shm_kv: test 65536 eviction=slru
--- config
    location /t {
        # Set 4 small entries. A 4096-byte slab will be allocated into slot 7
        # (1024-byte-sized entries), meaning that 4 entries will fit
        # in the slab.
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test1 header_ok=set-1 len=2048';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test2 header_ok=set-2 len=2048';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test3 header_ok=set-3 len=2048';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test4 header_ok=set-4 len=2048';

        # Set 2 large entries. Another 4096-byte slab will be allocated,
        # this time into slot 8 (2048-byte-sized entries), meaning that 2
        # entries will fit.
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test5 header_ok=set-5 len=4096';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test6 header_ok=set-6 len=4096';

        # The shm is now full.
        # We will now try to allocate another large entry.
        # It will not fit, and the LRU algorithm will deallocate the
        # least-recently-used entries, which are all small. Even after
        # 2 entries are released (and we effectively have released the
        # desired amount of memory), there's still no slab available of
        # the correct size, so the allocation will only succeed after
        # all 4 entries of the "small" slab are released and that slab
        # gets reallocated into the "large" slot.
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test7 header_ok=set-7 len=4096';

        # check all items
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-1 key=test/test1';
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-2 key=test/test2';
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-3 key=test/test3';
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-4 key=test/test4';
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-5 key=test/test5';
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-6 key=test/test6';
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-7 key=test/test7';

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
data-1: x{2048}
data-2: x{2048}
data-3: x{2048}
data-4: x{2048}
data-5:
data-6: x{4096}
data-7: x{4096}
--- response_body
ok
--- grep_error_log eval: qr/.*wasm "test" shm store.*/
--- grep_error_log_out eval
qr/^[^"]*\[debug\] [^"]* wasm "test" shm store: initialized
[^"]* \[debug\] [^"]* wasm "test" shm store: expired LRU entry[^"]*$/
--- no_error_log
[emerg]
[crit]
[stub1]
[stub2]
[stub3]
[stub4]
[stub5]
[stub6]



=== TEST 8: proxy_wasm key/value shm eviction - SLRU: deallocates larger items if needed
--- skip_eval: 25: ($::osname =~ m/darwin/ && $::archname =~ /arm64/)
--- skip_no_debug: 25
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- shm_kv: test 16384 eviction=slru
--- config
    location /t {
        # Fill an 8-entry slab.
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/8.1 header_ok=set-8-1 len=256';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/8.2 header_ok=set-8-2 len=256';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/8.3 header_ok=set-8-3 len=256';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/8.4 header_ok=set-8-4 len=256';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/8.5 header_ok=set-8-5 len=256';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/8.6 header_ok=set-8-6 len=256';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/8.7 header_ok=set-8-7 len=256';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/8.8 header_ok=set-8-8 len=256';

        # Fill a 2-entry slab.
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/2.1 header_ok=set-2-1 len=1024';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/2.2 header_ok=set-2-2 len=1024';

        # No more available slabs. Try to allocate a "4"-sized entry.
        # This will end up releasing the 2-entry slab.
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/4.1 header_ok=set-4-1 len=512';

        # check all items
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-8-1 key=test/8.1';
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-8-2 key=test/8.2';
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-8-3 key=test/8.3';
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-8-4 key=test/8.4';
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-8-5 key=test/8.5';
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-8-6 key=test/8.6';
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-8-7 key=test/8.7';
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-8-8 key=test/8.8';

        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-4-1 key=test/4.1';

        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-2-1 key=test/2.1';
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-2-2 key=test/2.2';

        echo ok;
    }
--- error_code: 200
--- response_headers_like
data-8-1: x{256}
data-8-2: x{256}
data-8-3: x{256}
data-8-4: x{256}
data-8-5: x{256}
data-8-6: x{256}
data-8-7: x{256}
data-8-8: x{256}
data-4-1: x{512}
data-2-1:
data-2-2:
--- response_body
ok
--- grep_error_log eval: qr/.*wasm "test" shm store.*/
--- grep_error_log_out eval
qr/^[^"]*\[debug\] [^"]* wasm "test" shm store: initialized
[^"]* \[debug\] [^"]* wasm "test" shm store: expired LRU entry
[^"]* \[debug\] [^"]* wasm "test" shm store: expired LRU entry[^"]*$/
--- no_error_log
[emerg]
[crit]
[stub1]
[stub2]
[stub3]
[stub4]
[stub5]
[stub6]
[stub7]
[stub8]
[stub9]



=== TEST 8: proxy_wasm key/value shm eviction - SLRU: deallocates larger items if needed, macOS (arm64)
--- skip_eval: 25: ($::osname !~ m/darwin/ && $::archname !~ /arm64/)
--- skip_no_debug: 25
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- shm_kv: test 65536 eviction=slru
--- config
    location /t {
        # Fill an 8-entry slab.
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/8.1 header_ok=set-8-1 len=1024';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/8.2 header_ok=set-8-2 len=1024';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/8.3 header_ok=set-8-3 len=1024';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/8.4 header_ok=set-8-4 len=1024';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/8.5 header_ok=set-8-5 len=1024';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/8.6 header_ok=set-8-6 len=1024';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/8.7 header_ok=set-8-7 len=1024';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/8.8 header_ok=set-8-8 len=1024';

        # Fill a 2-entry slab.
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/2.1 header_ok=set-2-1 len=4096';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/2.2 header_ok=set-2-2 len=4096';

        # No more available slabs. Try to allocate a "4"-sized entry.
        # This will end up releasing the 2-entry slab.
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/4.1 header_ok=set-4-1 len=2048';

        # check all items
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-8-1 key=test/8.1';
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-8-2 key=test/8.2';
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-8-3 key=test/8.3';
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-8-4 key=test/8.4';
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-8-5 key=test/8.5';
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-8-6 key=test/8.6';
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-8-7 key=test/8.7';
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-8-8 key=test/8.8';

        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-4-1 key=test/4.1';

        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-2-1 key=test/2.1';
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-2-2 key=test/2.2';

        echo ok;
    }
--- error_code: 200
--- response_headers_like
data-8-1: x{1024}
data-8-2: x{1024}
data-8-3: x{1024}
data-8-4: x{1024}
data-8-5: x{1024}
data-8-6: x{1024}
data-8-7: x{1024}
data-8-8: x{1024}
data-4-1: x{2048}
data-2-1:
data-2-2:
--- response_body
ok
--- grep_error_log eval: qr/.*wasm "test" shm store.*/
--- grep_error_log_out eval
qr/^[^"]*\[debug\] [^"]* wasm "test" shm store: initialized
[^"]* \[debug\] [^"]* wasm "test" shm store: expired LRU entry
[^"]* \[debug\] [^"]* wasm "test" shm store: expired LRU entry[^"]*$/
--- no_error_log
[emerg]
[crit]
[stub1]
[stub2]
[stub3]
[stub4]
[stub5]
[stub6]
[stub7]
[stub8]
[stub9]



=== TEST 9: proxy_wasm key/value shm eviction - SLRU: deallocates smaller items if no other choice
--- skip_eval: 25: ($::osname =~ m/darwin/ && $::archname =~ /arm64/)
--- skip_no_debug: 25
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- shm_kv: test 12288 eviction=slru
--- config
    location /t {
        # Fill an 8-entry slab, it's the only slab available.
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/8.1 header_ok=set-8-1 len=256';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/8.2 header_ok=set-8-2 len=256';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/8.3 header_ok=set-8-3 len=256';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/8.4 header_ok=set-8-4 len=256';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/8.5 header_ok=set-8-5 len=256';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/8.6 header_ok=set-8-6 len=256';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/8.7 header_ok=set-8-7 len=256';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/8.8 header_ok=set-8-8 len=256';

        # Now try to store a larger item.
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/2.1 header_ok=set-2-1 len=1024';

        # check all items
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-8-1 key=test/8.1';
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-8-2 key=test/8.2';
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-8-3 key=test/8.3';
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-8-4 key=test/8.4';
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-8-5 key=test/8.5';
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-8-6 key=test/8.6';
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-8-7 key=test/8.7';
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-8-8 key=test/8.8';

        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-2-1 key=test/2.1';

        echo ok;
    }
--- error_code: 200
--- response_headers_like
data-8-1:
data-8-2:
data-8-3:
data-8-4:
data-8-5:
data-8-6:
data-8-7:
data-8-8:
data-2-1: x{1024}
--- response_body
ok
--- grep_error_log eval: qr/.*wasm "test" shm store.*/
--- grep_error_log_out eval
qr/^[^"]*\[debug\] [^"]* wasm "test" shm store: initialized
[^"]* \[debug\] [^"]* wasm "test" shm store: expired LRU entry
[^"]* \[debug\] [^"]* wasm "test" shm store: expired LRU entry
[^"]* \[debug\] [^"]* wasm "test" shm store: expired LRU entry
[^"]* \[debug\] [^"]* wasm "test" shm store: expired LRU entry
[^"]* \[debug\] [^"]* wasm "test" shm store: expired LRU entry
[^"]* \[debug\] [^"]* wasm "test" shm store: expired LRU entry
[^"]* \[debug\] [^"]* wasm "test" shm store: expired LRU entry
[^"]* \[debug\] [^"]* wasm "test" shm store: expired LRU entry[^"]*$/
--- no_error_log
[emerg]
[crit]
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



=== TEST 9: proxy_wasm key/value shm eviction - SLRU: deallocates smaller items if no other choice, macOS (arm64)
--- skip_eval: 25: ($::osname !~ m/darwin/ && $::archname !~ /arm64/)
--- skip_no_debug: 25
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- shm_kv: test 49152 eviction=slru
--- config
    location /t {
        # Fill an 8-entry slab, it's the only slab available.
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/8.1 header_ok=set-8-1 len=1024';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/8.2 header_ok=set-8-2 len=1024';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/8.3 header_ok=set-8-3 len=1024';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/8.4 header_ok=set-8-4 len=1024';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/8.5 header_ok=set-8-5 len=1024';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/8.6 header_ok=set-8-6 len=1024';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/8.7 header_ok=set-8-7 len=1024';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/8.8 header_ok=set-8-8 len=1024';

        # Now try to store a larger item.
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/2.1 header_ok=set-2-1 len=4096';

        # check all items
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-8-1 key=test/8.1';
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-8-2 key=test/8.2';
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-8-3 key=test/8.3';
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-8-4 key=test/8.4';
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-8-5 key=test/8.5';
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-8-6 key=test/8.6';
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-8-7 key=test/8.7';
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-8-8 key=test/8.8';

        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-2-1 key=test/2.1';

        echo ok;
    }
--- error_code: 200
--- response_headers_like
data-8-1:
data-8-2:
data-8-3:
data-8-4:
data-8-5:
data-8-6:
data-8-7:
data-8-8:
data-2-1: x{4096}
--- response_body
ok
--- grep_error_log eval: qr/.*wasm "test" shm store.*/
--- grep_error_log_out eval
qr/^[^"]*\[debug\] [^"]* wasm "test" shm store: initialized
[^"]* \[debug\] [^"]* wasm "test" shm store: expired LRU entry
[^"]* \[debug\] [^"]* wasm "test" shm store: expired LRU entry
[^"]* \[debug\] [^"]* wasm "test" shm store: expired LRU entry
[^"]* \[debug\] [^"]* wasm "test" shm store: expired LRU entry
[^"]* \[debug\] [^"]* wasm "test" shm store: expired LRU entry
[^"]* \[debug\] [^"]* wasm "test" shm store: expired LRU entry
[^"]* \[debug\] [^"]* wasm "test" shm store: expired LRU entry
[^"]* \[debug\] [^"]* wasm "test" shm store: expired LRU entry[^"]*$/
--- no_error_log
[emerg]
[crit]
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



=== TEST 10: proxy_wasm key/value shm eviction - SLRU: deallocates the largest available "smaller slab"
--- skip_eval: 25: ($::osname =~ m/darwin/ && $::archname =~ /arm64/)
--- skip_no_debug: 25
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- shm_kv: test 16384 eviction=slru
--- config
    location /t {
        # Fill an 8-entry slab.
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/8.1 header_ok=set-8-1 len=256';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/8.2 header_ok=set-8-2 len=256';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/8.3 header_ok=set-8-3 len=256';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/8.4 header_ok=set-8-4 len=256';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/8.5 header_ok=set-8-5 len=256';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/8.6 header_ok=set-8-6 len=256';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/8.7 header_ok=set-8-7 len=256';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/8.8 header_ok=set-8-8 len=256';

        # Fill an 4-entry slab. No more space.
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/4.1 header_ok=set-4-1 len=512';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/4.2 header_ok=set-4-2 len=512';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/4.3 header_ok=set-4-3 len=512';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/4.4 header_ok=set-4-4 len=512';

        # Now try to store a larger item.
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/2.1 header_ok=set-2-1 len=1024';

        # check all items
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-8-1 key=test/8.1';
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-8-2 key=test/8.2';
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-8-3 key=test/8.3';
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-8-4 key=test/8.4';
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-8-5 key=test/8.5';
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-8-6 key=test/8.6';
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-8-7 key=test/8.7';
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-8-8 key=test/8.8';

        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-4-1 key=test/4.1';
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-4-2 key=test/4.2';
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-4-3 key=test/4.3';
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-4-4 key=test/4.4';

        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-2-1 key=test/2.1';

        echo ok;
    }
--- error_code: 200
--- response_headers_like
data-8-1: x{256}
data-8-2: x{256}
data-8-3: x{256}
data-8-4: x{256}
data-8-5: x{256}
data-8-6: x{256}
data-8-7: x{256}
data-8-8: x{256}
data-4-1:
data-4-2:
data-4-3:
data-4-4:
data-2-1: x{1024}
--- response_body
ok
--- grep_error_log eval: qr/.*wasm "test" shm store.*/
--- grep_error_log_out eval
qr/^[^"]*\[debug\] [^"]* wasm "test" shm store: initialized
[^"]* \[debug\] [^"]* wasm "test" shm store: expired LRU entry
[^"]* \[debug\] [^"]* wasm "test" shm store: expired LRU entry
[^"]* \[debug\] [^"]* wasm "test" shm store: expired LRU entry
[^"]* \[debug\] [^"]* wasm "test" shm store: expired LRU entry[^"]*$/
--- no_error_log
[emerg]
[crit]
[stub1]
[stub2]
[stub3]
[stub4]
[stub5]
[stub6]
[stub7]



=== TEST 10: proxy_wasm key/value shm eviction - SLRU: deallocates the largest available "smaller slab", macOS (arm64)
--- skip_eval: 25: ($::osname !~ m/darwin/ && $::archname !~ /arm64/)
--- skip_no_debug: 25
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- shm_kv: test 65536 eviction=slru
--- config
    location /t {
        # Fill an 8-entry slab.
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/8.1 header_ok=set-8-1 len=1024';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/8.2 header_ok=set-8-2 len=1024';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/8.3 header_ok=set-8-3 len=1024';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/8.4 header_ok=set-8-4 len=1024';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/8.5 header_ok=set-8-5 len=1024';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/8.6 header_ok=set-8-6 len=1024';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/8.7 header_ok=set-8-7 len=1024';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/8.8 header_ok=set-8-8 len=1024';

        # Fill an 4-entry slab. No more space.
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/4.1 header_ok=set-4-1 len=2048';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/4.2 header_ok=set-4-2 len=2048';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/4.3 header_ok=set-4-3 len=2048';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/4.4 header_ok=set-4-4 len=2048';

        # Now try to store a larger item.
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/2.1 header_ok=set-2-1 len=4096';

        # check all items
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-8-1 key=test/8.1';
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-8-2 key=test/8.2';
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-8-3 key=test/8.3';
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-8-4 key=test/8.4';
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-8-5 key=test/8.5';
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-8-6 key=test/8.6';
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-8-7 key=test/8.7';
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-8-8 key=test/8.8';

        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-4-1 key=test/4.1';
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-4-2 key=test/4.2';
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-4-3 key=test/4.3';
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-4-4 key=test/4.4';

        proxy_wasm hostcalls 'test=/t/shm/get_shared_data header_data=data-2-1 key=test/2.1';

        echo ok;
    }
--- error_code: 200
--- response_headers_like
data-8-1: x{1024}
data-8-2: x{1024}
data-8-3: x{1024}
data-8-4: x{1024}
data-8-5: x{1024}
data-8-6: x{1024}
data-8-7: x{1024}
data-8-8: x{1024}
data-4-1:
data-4-2:
data-4-3:
data-4-4:
data-2-1: x{4096}
--- response_body
ok
--- grep_error_log eval: qr/.*wasm "test" shm store.*/
--- grep_error_log_out eval
qr/^[^"]*\[debug\] [^"]* wasm "test" shm store: initialized
[^"]* \[debug\] [^"]* wasm "test" shm store: expired LRU entry
[^"]* \[debug\] [^"]* wasm "test" shm store: expired LRU entry
[^"]* \[debug\] [^"]* wasm "test" shm store: expired LRU entry
[^"]* \[debug\] [^"]* wasm "test" shm store: expired LRU entry[^"]*$/
--- no_error_log
[emerg]
[crit]
[stub1]
[stub2]
[stub3]
[stub4]
[stub5]
[stub6]
[stub7]



=== TEST 11: proxy_wasm key/value shm eviction - SLRU: smallest possible queue size
For an idea of the order of magnitude, x86_64 with ngx_pagesize at 4kb this
produces a node of ~96 + 2 bytes (may vary depending on the queue node struct).
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- shm_kv: * 1m eviction=slru
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data \
                              key=k \
                              value=v';

        proxy_wasm hostcalls 'test=/t/shm/get_shared_data \
                              key=k';

        echo ok;
    }
--- response_headers
cas: 1
exists: 1
data: v
--- response_body
ok
--- no_error_log
[error]
[crit]
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
