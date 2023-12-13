# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasmX;

workers(2);
master_on();

plan_tests(5);
run_tests();

__DATA__

=== TEST 1: kv - with minimum size
--- valgrind
--- skip_no_debug
--- main_config eval
qq{
    wasm {
        shm_kv test $::min_shm_size;
    }
}
--- error_log
"test" shm: initialization
"test" shm store: initialized
--- no_error_log
[error]
[crit]



=== TEST 2: kv - with default namespace
--- skip_no_debug
--- main_config eval
qq{
    wasm {
        shm_kv * $::min_shm_size;
    }
}
--- error_log
"*" shm: initialization
"*" shm store: initialized
--- no_error_log
[error]
[crit]
