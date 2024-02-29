# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasmX;

plan_tests(4);
run_tests();

__DATA__

=== TEST 1: wasm socket directives - sanity
--- main_config
    wasm {
        socket_connect_timeout 1s;
        socket_send_timeout    1s;
        socket_read_timeout    1s;
        socket_buffer_size     1024;
        socket_large_buffers   1 1024;
        socket_buffer_reuse    off;
    }
--- no_error_log
[error]
[crit]
[emerg]
