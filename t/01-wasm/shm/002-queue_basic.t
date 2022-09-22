# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

skip_valgrind();

workers(2);
master_on();
plan tests => repeat_each() * (blocks() * 5);
run_tests();

__DATA__

=== TEST 1: queue - minimum size
--- main_config
    wasm {
        shm_queue my_queue_1 12288;
    }
--- error_log eval
qr/\[info\] .*? \[wasm\] shm init/
--- no_error_log
[emerg]
[error]
[crit]
