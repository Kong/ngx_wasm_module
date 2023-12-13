# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasmX;

our $four_pages_size = $page_size * 3;
our $reserved_space = ($page_size * 2) + 4;

plan_tests(10);
run_tests();

__DATA__

=== TEST 1: proxy_wasm queue shm - pop when data size == buffer_size - 1
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- shm_queue eval: qq/test $::four_pages_size/
--- config eval
my $buffer_size_minus_one = $::four_pages_size - $::reserved_space - 1;
qq{
    location /t {
        proxy_wasm hostcalls 'test=/t/shm/enqueue \
                              queue=test \
                              length=$buffer_size_minus_one';
        proxy_wasm hostcalls 'test=/t/shm/dequeue \
                              queue=test';
        echo ok;
    }
}
--- response_headers
status-enqueue: 0
status-dequeue: 0
exists: 1
--- response_body
ok
--- no_error_log
[error]
[crit]
circular_read: wrapping around
circular_write: wrapping around
[stub]



=== TEST 2: proxy_wasm queue shm - pop when data size == buffer_size
--- skip_no_debug
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- shm_queue eval: qq/test $::four_pages_size/
--- config eval
my $buffer_size = $::four_pages_size - $::reserved_space;
qq{
    location /t {
        proxy_wasm hostcalls 'test=/t/shm/enqueue \
                              queue=test \
                              length=$buffer_size';
        proxy_wasm hostcalls 'test=/t/shm/dequeue \
                              queue=test';
        echo ok;
    }
}
--- response_headers
status-enqueue: 0
status-dequeue: 0
exists: 1
--- response_body
ok
--- error_log
circular_read: wrapping around
circular_write: wrapping around
--- no_error_log
[error]
[crit]
[emerg]
