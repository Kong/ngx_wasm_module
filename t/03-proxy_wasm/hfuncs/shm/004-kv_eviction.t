# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasmX;

our $three_pages_size = $page_size * 3;
our $four_pages_size = $page_size * 4;
our $half_page_size = $page_size / 2;
our $quarter_page_size = $page_size / 4;
our $eighth_page_size = $page_size / 8;
our $sixteenth_page_size = $page_size / 16;

our $half_page_slot = -3;
our $quarter_page_slot = -3;
our $half = $half_page_size;
our $quarter = $quarter_page_size;

while ($quarter > 1) {
    $quarter >>= 1;
    $quarter_page_slot += 1;
}

while ($half > 1) {
    $half >>= 1;
    $half_page_slot += 1;
}

plan_tests(27);
run_tests();

__DATA__

=== TEST 1: proxy_wasm key/value shm eviction - evicts most recent data in the queue
--- skip_no_debug
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- shm_kv eval: qq/test $::three_pages_size/
--- config eval
qq{
    location /t {
        # set 10 items, only 4 fit
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test1 header_ok=set-1 len=$::eighth_page_size';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test2 header_ok=set-2 len=$::eighth_page_size';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test3 header_ok=set-3 len=$::eighth_page_size';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test4 header_ok=set-4 len=$::eighth_page_size';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test5 header_ok=set-5 len=$::eighth_page_size';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test6 header_ok=set-6 len=$::eighth_page_size';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test7 header_ok=set-7 len=$::eighth_page_size';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test8 header_ok=set-8 len=$::eighth_page_size';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test9 header_ok=set-9 len=$::eighth_page_size';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test10 header_ok=set-10 len=$::eighth_page_size';

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
}
--- error_code: 200
--- response_headers_like eval
qr/
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
data-7: x{$::eighth_page_size}
data-8: x{$::eighth_page_size}
data-9: x{$::eighth_page_size}
data-10: x{$::eighth_page_size}
/
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
--- valgrind
--- skip_no_debug
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- shm_kv eval: qq/test $::three_pages_size/
--- config eval
qq{
    location /t {
        # Set 9 entries. A $::page_size-byte slab will be allocated into slot $::quarter_page_slot
        # ($::quarter_page_size-byte-sized entries), meaning that 4 entries will fit
        # in the slab. Due to the given shm_kv size (two pages of overhead,
        # one for data), that's all we can allocate at a time.
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test1 header_ok=set-1 len=$::eighth_page_size';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test2 header_ok=set-2 len=$::eighth_page_size';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test3 header_ok=set-3 len=$::eighth_page_size';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test4 header_ok=set-4 len=$::eighth_page_size';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test5 header_ok=set-5 len=$::eighth_page_size';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test6 header_ok=set-6 len=$::eighth_page_size';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test7 header_ok=set-7 len=$::eighth_page_size';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test8 header_ok=set-8 len=$::eighth_page_size';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test9 header_ok=set-9 len=$::eighth_page_size';

        # At this point in time, we have entries 6, 7, 8 and 9 in the slab.
        # The last allocation is larger, but it goes over the entry size
        # of our only page. This means all four entries need to be released,
        # to free the slab for reallocation into slot $::half_page_slot ($::half_page_size-byte-sized entries).
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test10 header_ok=set-10 len=$::quarter_page_size';

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
}
--- error_code: 200
--- response_headers_like eval
qr/
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
data-10: x{$::quarter_page_size}
/
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
--- skip_no_debug
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- shm_kv eval: qq/test $::three_pages_size/
--- config eval
qq{
    location /t {
        # set 10 items, only 4 fit
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test1 header_ok=set-1 len=$::eighth_page_size';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test2 header_ok=set-2 len=$::eighth_page_size';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test3 header_ok=set-3 len=$::eighth_page_size';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test4 header_ok=set-4 len=$::eighth_page_size';
           #keep test1 alive in the LRU queue
           proxy_wasm hostcalls 'test=/t/shm/get_shared_data key=test/test1';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test5 header_ok=set-5 len=$::eighth_page_size';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test6 header_ok=set-6 len=$::eighth_page_size';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test7 header_ok=set-7 len=$::eighth_page_size';
           #keep test1 alive in the LRU queue
           proxy_wasm hostcalls 'test=/t/shm/get_shared_data key=test/test1';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test8 header_ok=set-8 len=$::eighth_page_size';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test9 header_ok=set-9 len=$::eighth_page_size';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test10 header_ok=set-10 len=$::eighth_page_size';

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
}
--- error_code: 200
--- response_headers_like eval
qr/
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
data-1: x{$::eighth_page_size}
data-2:
data-3:
data-4:
data-5:
data-6:
data-7:
data-8: x{$::eighth_page_size}
data-9: x{$::eighth_page_size}
data-10: x{$::eighth_page_size}
/
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
--- shm_kv eval: qq/test $::three_pages_size eviction=slru/
--- config eval
my $too_large_size = $::page_size + 1;
qq{
    location /t {
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len \
                              key=test/test1 \
                              header_ok=set-1 \
                              len=$too_large_size';

        # get test/test
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data \
                              key=test/test';
        echo failed;
    }
}
--- error_code: 500
--- response_body_like: 500 Internal Server Error
--- grep_error_log eval: qr/(\[crit\]|.*?failed setting value to shm).*/
--- grep_error_log_out eval
qr/\[crit\] .*? \[wasm\] "test" shm store: no memory; cannot allocate pair with key size \d+ and value size \d+
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
[stub21]
[stub22]



=== TEST 5: proxy_wasm key/value shm eviction - LRU: a set that is too big can still fail
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- shm_kv eval: qq/test $::three_pages_size eviction=lru/
--- config eval
my $too_large_size = $::page_size + 1;
qq{
    location /t {
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len \
                              key=test/test1 \
                              header_ok=set-1 \
                              len=$too_large_size';

        # get test/test
        proxy_wasm hostcalls 'test=/t/shm/get_shared_data \
                              key=test/test';
        echo failed;
    }
}
--- error_code: 500
--- response_body_like: 500 Internal Server Error
--- grep_error_log eval: qr/(\[crit\]|.*?failed setting value to shm).*/
--- grep_error_log_out eval
qr/\[crit\] .*? \[wasm\] "test" shm store: no memory; cannot allocate pair with key size 10 and value size \d+
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
[stub21]
[stub22]



=== TEST 6: proxy_wasm key/value shm eviction - LRU: may over-deallocate
--- skip_no_debug
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- shm_kv eval: qq/test $::four_pages_size eviction=lru/
--- config eval
qq{
    location /t {
        # Set 4 small entries. A $::page_size-byte slab will be allocated into slot $::quarter_page_slot
        # ($::quarter_page_size-byte-sized entries), meaning that 4 entries will fit
        # in the slab.
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test1 header_ok=set-1 len=$::eighth_page_size';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test2 header_ok=set-2 len=$::eighth_page_size';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test3 header_ok=set-3 len=$::eighth_page_size';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test4 header_ok=set-4 len=$::eighth_page_size';

        # Set 2 large entries. Another $::page_size-byte slab will be allocated,
        # this time into slot $::half_page_slot ($::half_page_size-byte-sized entries), meaning that 2
        # entries will fit.
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test5 header_ok=set-5 len=$::quarter_page_size';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test6 header_ok=set-6 len=$::quarter_page_size';

        # The shm is now full.
        # We will now try to allocate another large entry.
        # It will not fit, and the LRU algorithm will deallocate the
        # least-recently-used entries, which are all small. Even after
        # 2 entries are released (and we effectively have released the
        # desired amount of memory), there's still no slab available of
        # the correct size, so the allocation will only succeed after
        # all 4 entries of the "small" slab are released and that slab
        # gets reallocated into the "large" slot.
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test7 header_ok=set-7 len=$::quarter_page_size';

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
}
--- error_code: 200
--- response_headers_like eval
qr/
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
data-5: x{$::quarter_page_size}
data-6: x{$::quarter_page_size}
data-7: x{$::quarter_page_size}
/
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
--- skip_no_debug
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- shm_kv eval: qq/test $::four_pages_size eviction=slru/
--- config eval
qq{
    location /t {
        # Set 4 small entries. A $::page_size-byte slab will be allocated into slot $::quarter_page_slot
        # ($::quarter_page_size-byte-sized entries), meaning that 4 entries will fit
        # in the slab.
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test1 header_ok=set-1 len=$::eighth_page_size';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test2 header_ok=set-2 len=$::eighth_page_size';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test3 header_ok=set-3 len=$::eighth_page_size';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test4 header_ok=set-4 len=$::eighth_page_size';

        # Set 2 large entries. Another $::page_size-byte slab will be allocated,
        # this time into slot $::half_page_slot ($::half_page_size-byte-sized entries), meaning that 2
        # entries will fit.
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test5 header_ok=set-5 len=$::quarter_page_size';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test6 header_ok=set-6 len=$::quarter_page_size';

        # The shm is now full.
        # We will now try to allocate another large entry.
        # It will not fit, and the LRU algorithm will deallocate the
        # least-recently-used entries, which are all small. Even after
        # 2 entries are released (and we effectively have released the
        # desired amount of memory), there's still no slab available of
        # the correct size, so the allocation will only succeed after
        # all 4 entries of the "small" slab are released and that slab
        # gets reallocated into the "large" slot.
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/test7 header_ok=set-7 len=$::quarter_page_size';

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
}
--- error_code: 200
--- response_headers_like eval
qr/
set-1: 1
set-2: 1
set-3: 1
set-4: 1
set-5: 1
set-6: 1
set-7: 1
data-1: x{$::eighth_page_size}
data-2: x{$::eighth_page_size}
data-3: x{$::eighth_page_size}
data-4: x{$::eighth_page_size}
data-5:
data-6: x{$::quarter_page_size}
data-7: x{$::quarter_page_size}
/
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
--- skip_no_debug
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- shm_kv eval: qq/test $::four_pages_size eviction=slru/
--- config eval
qq{
    location /t {
        # Fill an 8-entry slab.
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/8.1 header_ok=set-8-1 len=$::sixteenth_page_size';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/8.2 header_ok=set-8-2 len=$::sixteenth_page_size';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/8.3 header_ok=set-8-3 len=$::sixteenth_page_size';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/8.4 header_ok=set-8-4 len=$::sixteenth_page_size';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/8.5 header_ok=set-8-5 len=$::sixteenth_page_size';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/8.6 header_ok=set-8-6 len=$::sixteenth_page_size';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/8.7 header_ok=set-8-7 len=$::sixteenth_page_size';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/8.8 header_ok=set-8-8 len=$::sixteenth_page_size';

        # Fill a 2-entry slab.
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/2.1 header_ok=set-2-1 len=$::quarter_page_size';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/2.2 header_ok=set-2-2 len=$::quarter_page_size';

        # No more available slabs. Try to allocate a "4"-sized entry.
        # This will end up releasing the 2-entry slab.
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/4.1 header_ok=set-4-1 len=$::eighth_page_size';

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
}
--- error_code: 200
--- response_headers_like eval
qr/
data-8-1: x{$::sixteenth_page_size}
data-8-2: x{$::sixteenth_page_size}
data-8-3: x{$::sixteenth_page_size}
data-8-4: x{$::sixteenth_page_size}
data-8-5: x{$::sixteenth_page_size}
data-8-6: x{$::sixteenth_page_size}
data-8-7: x{$::sixteenth_page_size}
data-8-8: x{$::sixteenth_page_size}
data-4-1: x{$::eighth_page_size}
data-2-1:
data-2-2:
/
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
--- skip_no_debug
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- shm_kv eval: qq/test $::three_pages_size eviction=slru/
--- config eval
qq{
    location /t {
        # Fill an 8-entry slab, it's the only slab available.
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/8.1 header_ok=set-8-1 len=$::sixteenth_page_size';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/8.2 header_ok=set-8-2 len=$::sixteenth_page_size';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/8.3 header_ok=set-8-3 len=$::sixteenth_page_size';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/8.4 header_ok=set-8-4 len=$::sixteenth_page_size';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/8.5 header_ok=set-8-5 len=$::sixteenth_page_size';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/8.6 header_ok=set-8-6 len=$::sixteenth_page_size';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/8.7 header_ok=set-8-7 len=$::sixteenth_page_size';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/8.8 header_ok=set-8-8 len=$::sixteenth_page_size';

        # Now try to store a larger item.
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/2.1 header_ok=set-2-1 len=$::quarter_page_size';

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
}
--- error_code: 200
--- response_headers_like eval
qr/
data-8-1:
data-8-2:
data-8-3:
data-8-4:
data-8-5:
data-8-6:
data-8-7:
data-8-8:
data-2-1: x{$::quarter_page_size}
/
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
--- skip_no_debug
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- shm_kv eval: qq/test $::four_pages_size eviction=slru/
--- config eval
qq{
    location /t {
        # Fill an 8-entry slab.
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/8.1 header_ok=set-8-1 len=$::sixteenth_page_size';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/8.2 header_ok=set-8-2 len=$::sixteenth_page_size';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/8.3 header_ok=set-8-3 len=$::sixteenth_page_size';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/8.4 header_ok=set-8-4 len=$::sixteenth_page_size';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/8.5 header_ok=set-8-5 len=$::sixteenth_page_size';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/8.6 header_ok=set-8-6 len=$::sixteenth_page_size';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/8.7 header_ok=set-8-7 len=$::sixteenth_page_size';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/8.8 header_ok=set-8-8 len=$::sixteenth_page_size';

        # Fill an 4-entry slab. No more space.
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/4.1 header_ok=set-4-1 len=$::eighth_page_size';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/4.2 header_ok=set-4-2 len=$::eighth_page_size';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/4.3 header_ok=set-4-3 len=$::eighth_page_size';
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/4.4 header_ok=set-4-4 len=$::eighth_page_size';

        # Now try to store a larger item.
        proxy_wasm hostcalls 'test=/t/shm/set_shared_data_by_len key=test/2.1 header_ok=set-2-1 len=$::quarter_page_size';

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
}
--- error_code: 200
--- response_headers_like eval
qr/
data-8-1: x{$::sixteenth_page_size}
data-8-2: x{$::sixteenth_page_size}
data-8-3: x{$::sixteenth_page_size}
data-8-4: x{$::sixteenth_page_size}
data-8-5: x{$::sixteenth_page_size}
data-8-6: x{$::sixteenth_page_size}
data-8-7: x{$::sixteenth_page_size}
data-8-8: x{$::sixteenth_page_size}
data-4-1:
data-4-2:
data-4-3:
data-4-4:
data-2-1: x{$::quarter_page_size}
/
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
[stub17]
[stub18]
