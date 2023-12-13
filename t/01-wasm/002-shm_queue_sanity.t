# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasmX;

workers(2);
master_on();

plan_tests(5);
run_tests();

__DATA__

=== TEST 1: queue - with minimum size
--- valgrind
--- skip_no_debug
--- main_config eval
qq{
    wasm {
        shm_queue test $::min_shm_size;
    }
}
--- error_log eval
[
    qr/\"test\" shm: initialization/,
    qr/\"test\" shm queue: initialized \(\d+ bytes available\)/
]
--- no_error_log
[error]
[crit]
