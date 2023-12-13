# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasmX;

our $four_pages_size = $page_size * 3;
our $reserved_space = ($page_size * 2) + 4;

plan_tests(6);
run_tests();

__DATA__

=== TEST 1: proxy_wasm queue shm - push when data_size == buffer_size
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
        echo ok;
    }
}
--- response_headers
status-enqueue: 0
--- response_body
ok
--- error_log
circular_write: wrapping around
--- no_error_log
[error]
[crit]



=== TEST 2: proxy_wasm queue shm - push when data size == buffer_size + 1
--- valgrind
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- shm_queue eval: qq/test $::four_pages_size/
--- config eval
my $buffer_size_plus_one = $::four_pages_size - $::reserved_space + 1;
qq{
    location /t {
        proxy_wasm hostcalls 'test=/t/shm/enqueue \
                              queue=test \
                              length=$buffer_size_plus_one';
        echo ok;
    }
}
--- error_code: 500
--- response_body_like: 500 Internal Server Error
--- grep_error_log eval: qr/.*?could not enqueue.*/
--- grep_error_log_out eval
qr~(\[error\]|Uncaught RuntimeError|\s+).*?host trap \(internal error\): could not enqueue \(queue is full\).*~
--- no_error_log
[crit]
[emerg]
circular_write: wrapping around



=== TEST 3: proxy_wasm queue shm - push when data size == buffer_size - 1
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
        echo ok;
    }
}
--- response_headers
status-enqueue: 0
--- response_body
ok
--- no_error_log
[error]
[crit]
circular_write: wrapping around
