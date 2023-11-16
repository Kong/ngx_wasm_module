# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

workers(2);
master_on();
#skip_valgrind();

plan tests => repeat_each() * (blocks() * 5);

run_tests();

__DATA__

=== TEST 1: queue - with minimum size
--- skip_no_debug: 5
--- main_config
    wasm {
        shm_queue test 12288;
    }
--- error_log
"test" shm: initialization
"test" shm queue: initialized (4096 bytes available)
--- no_error_log
[error]
[crit]
